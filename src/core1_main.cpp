#include <stdint.h>
#include "core1_main.hpp"
#include "hardware/address_mapped.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

#include "posc_adc.hpp"
#include "posc_dma.hpp"

uint16_t adc_buffer_u16[adc_buffer_size_u16];
constexpr void *adc_buffer_addr{adc_buffer_u16};

mutex_t datac1_mutex;
DataForCore1 datac1_glob{&datac1_mutex};
extern DataForCore0 datac0_glob;

#ifndef NDEBUG
debug_data_t debug_data;
#endif

int dma_ctrl_chan;
int dma_adc_chan;
volatile bool ctrl_channel_trigered{false};
volatile bool adc_chan_null_trigger{false};
volatile bool dma_cycle_forever{false};

volatile void *ctrl_chan_adc_write = adc_buffer_addr;
volatile void **ctrk_chan_read_addr = &ctrl_chan_adc_write;
io_rw_32 *ctrl_chan_write_addr;

void dma_irq_handler() {
    if (dma_channel_get_irq1_status(dma_ctrl_chan)) {
        ctrl_channel_trigered = true;
        if (!dma_cycle_forever) {
            ctrl_chan_adc_write = 0;
        }
        dma_channel_acknowledge_irq1(dma_ctrl_chan);
    }
    // if (dma_channel_get_irq1_status(dma_adc_chan)) {
    //     adc_chan_null_trigger = true;
    //     dma_channel_acknowledge_irq1(dma_adc_chan);
    // }
}

void core1_main() {
    DataForCore0 datac0_private;
    trig::Settings triggersettings_private;
    constexpr uint adc0_pin{26};
    bool adc_running, trigger_detected, adc_done;
    uint32_t end_tx_count, pretring_tx_count, current_tx_count;
    bool wait_for_next_cycle;
    uint32_t pretrig_samples, posttrig_samples, second_cycle_tx_count;
    uint32_t array_index;
    uint32_t samples[2];
    core0_message c0msg{STOP_ADC};
    uint32_t current_channel{0};
    size_t trigger_channel_index_div{1};

    const int adc_chan = dma_claim_unused_channel(true);
    const int ctrl_chan = dma_claim_unused_channel(true);
#ifndef NDEBUG
    debug_data.dma_adc_chan = adc_chan;
    debug_data.dma_ctrl_chan = ctrl_chan;
#endif
    dma_ctrl_chan = ctrl_chan;
    dma_adc_chan = adc_chan;
    ctrl_chan_write_addr = &(dma_channel_hw_addr(adc_chan)->al2_write_addr_trig);
    // void **const ctrl_read_addr = &ctrl_adc_addr;

    dma_channel_config ctrl_chan_cfg = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&ctrl_chan_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&ctrl_chan_cfg, false);
    channel_config_set_write_increment(&ctrl_chan_cfg, false);
    dma_channel_configure(ctrl_chan, &ctrl_chan_cfg, ctrl_chan_write_addr, ctrk_chan_read_addr, 1, false);

    dma_channel_config adc_chan_cfg = dma_channel_get_default_config(adc_chan);
    channel_config_set_irq_quiet(&adc_chan_cfg, true);
    channel_config_set_transfer_data_size(&adc_chan_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&adc_chan_cfg, false);
    channel_config_set_write_increment(&adc_chan_cfg, true);
    channel_config_set_dreq(&adc_chan_cfg, DREQ_ADC);
    channel_config_set_chain_to(&adc_chan_cfg, ctrl_chan);
    dma_channel_configure(adc_chan, &adc_chan_cfg, adc_buffer_addr, &(adc_hw->fifo), adc_buffer_size_u16, false);

    dma_channel_set_irq1_enabled(ctrl_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    adc_init();
    adc_gpio_init(adc0_pin);
    adc_select_input(0);
    adc_set_round_robin(0);
    adc_set_clkdiv(0.0f);

    send_msg_to_core0(CORE1_STARTED);

    while (true) {
        /*
         * Handle messages from Core0
         */
        if (fifo_contains_value()) {
            core0_message c0msg = get_msg_from_core0();
            if (c0msg == START_ADC_AUTO || c0msg == START_ADC_SINGLE) {
                if (adc_running) {
                    adc_run(false);
                    dma_channel_abort(adc_chan);
                    adc_running = false;
                }
                adc_init();
                datac1_glob.lock_blocking();

                triggersettings_private = datac1_glob.trigger_settings;

                end_tx_count = adc_buffer_size_u16 - datac1_glob.number_of_samples;
                pretrig_samples = triggersettings_private.calculate_pretrig_count(datac1_glob.number_of_samples);
                posttrig_samples = datac1_glob.number_of_samples - pretrig_samples;
                pretring_tx_count = adc_buffer_size_u16 - pretrig_samples;
                current_tx_count = adc_buffer_size_u16;

                samples[0] = triggersettings_private.get_initial_sample_value();
                adc::set_clkdiv_u32(datac1_glob.adc_div);

                datac0_private.set_array1(adc_buffer_u16, datac1_glob.number_of_samples, 0);
                datac0_private.trigger_index = 0;

                // TODO: Ability to choose which channels in particular are on
                // How many channels are enabled 0 - 4
                adc_set_round_robin(adc::get_round_robin_mask(datac1_glob.number_of_channels));

                // TODO: Select trigger input as first
                adc_select_input(0);  // ADC should always start with channel 0

                trigger_channel_index_div = adc::get_round_robin_index_divider(datac1_glob.number_of_channels);

                datac1_glob.unlock();

                dma_channel_configure(adc_chan, &adc_chan_cfg, adc_buffer_addr, &(adc_hw->fifo), adc_buffer_size_u16, false);
                dma_channel_configure(ctrl_chan, &ctrl_chan_cfg, ctrl_chan_write_addr, ctrk_chan_read_addr, 1, false);

                wait_for_next_cycle = false;
                second_cycle_tx_count = 0;
                trigger_detected = false;
                ctrl_channel_trigered = false;
                adc_chan_null_trigger = false;
                adc_running = true;
                adc_done = false;
                if (c0msg == START_ADC_SINGLE) {
                    ctrl_chan_adc_write = adc_buffer_addr;
                    dma_cycle_forever = true;
                } else {
                    ctrl_chan_adc_write = 0;
                    dma_cycle_forever = false;
                }

#ifndef NDEBUG
                debug_data.clear();
#endif

                adc_fifo_setup(true, true, 1, false, false);
                dma_channel_start(adc_chan);
                adc_run(true);
            } else if (c0msg == STOP_ADC) {
                ctrl_chan_adc_write = 0;
                adc_run(false);
                dma_channel_abort(adc_chan);
                adc_running = false;
                trigger_detected = false;
#ifndef NDEBUG
                debug_data.adc_running = false;
#endif
            }
        }

        /*
         * Handle running ADC
         */
        if (adc_running) {
            /*
             * Check for Trigger
             */
            if (current_tx_count != dma::get_transfer_count(adc_chan)) {
                current_tx_count = dma::get_transfer_count(adc_chan);

                // TODO: Solve for edge case where processor misses a sample
                current_channel = (current_channel + 1) % trigger_channel_index_div;

                if (wait_for_next_cycle && current_tx_count > end_tx_count) {
                    wait_for_next_cycle = false;
                }

                if (!dma_cycle_forever && !wait_for_next_cycle && current_tx_count <= end_tx_count) {
                    adc_run(false);
                    adc_done = true;
#ifndef NDEBUG
                    debug_data.adc_done = true;
#endif
                } else if (!trigger_detected && (current_tx_count < pretring_tx_count || ctrl_channel_trigered) && current_tx_count < adc_buffer_size_u16) {
                    array_index = adc_buffer_size_u16 - current_tx_count - 1;
                    // Check for trigger only in channel 0 samples
                    if (current_channel == 0) {
                        samples[1] = adc_buffer_u16[array_index];
                        if (triggersettings_private.detect_edge_raw(samples[0], samples[1])) {
                            if (current_tx_count < posttrig_samples) {
                                second_cycle_tx_count = (posttrig_samples - current_tx_count);
                                end_tx_count = adc_buffer_size_u16 - second_cycle_tx_count;
#ifndef NDEBUG
                                debug_data.second_cycle = second_cycle_tx_count;
#endif
                                wait_for_next_cycle = true;
                                dma_cycle_forever = false;
                                ctrl_chan_adc_write = adc_buffer_addr;
                                // dma_channel_set_trans_count(adc_chan, second_cycle_tx_count, true);
                            } else {
                                end_tx_count = current_tx_count - posttrig_samples;
                                ctrl_chan_adc_write = 0;
                            }
                            dma_cycle_forever = false;
                            trigger_detected = true;

#ifndef NDEBUG
                            debug_data.trigger_detected = true;
#endif
                        }

                        samples[0] = samples[1];
                    }
                }
            }

            /*
             * Check if DMA is finished
             */
            if ((!dma_channel_is_busy(adc_chan) && dma_channel_hw_addr(adc_chan)->write_addr == 0) || adc_done) {
                ctrl_chan_adc_write = 0;
                adc_run(false);
                dma_channel_abort(adc_chan);
                adc_running = false;
#ifndef NDEBUG
                debug_data.adc_running = false;
#endif
                const uint32_t sum_samples = pretrig_samples + posttrig_samples;
                if (trigger_detected && array_index >= pretrig_samples) {
                    if (second_cycle_tx_count) {
                        const uint32_t first_cycle_samples = sum_samples - second_cycle_tx_count;
                        datac0_private.array1_start = &adc_buffer_u16[adc_buffer_size_u16 - first_cycle_samples];
                        datac0_private.trigger_index = pretrig_samples;
                        datac0_private.array1_samples = first_cycle_samples;
                        datac0_private.array2_samples = second_cycle_tx_count;

                    } else {
                        datac0_private.array1_start = &adc_buffer_u16[array_index - pretrig_samples];
                        datac0_private.array1_samples = sum_samples;
                        datac0_private.trigger_index = pretrig_samples;
                    }
                } else if (trigger_detected && array_index < pretrig_samples) {
                    const uint32_t missing_samples = pretrig_samples - array_index;
                    datac0_private.array1_start = &adc_buffer_u16[adc_buffer_size_u16 - missing_samples];
                    datac0_private.trigger_index = pretrig_samples;
                    datac0_private.array1_samples = missing_samples;
                    datac0_private.array2_samples = sum_samples - missing_samples;
                }
#ifndef NDEBUG
                debug_data.array_index = array_index;
#endif

                datac0_glob.lock_blocking();
                datac0_glob = datac0_private;
                datac0_glob.unlock();
                send_msg_to_core0(core1_message::ADC_DONE);
            }
        }
    }
}