#include <stdint.h>
#include <sys/types.h>
#include <cassert>
#include <cstddef>
#include <cstdio>

#include <etl/string.h>
#include <etl/to_string.h>
#include <etl/algorithm.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/structs/dma.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

#include "posc_adc.hpp"
#include "posc_dma.hpp"
#include "posc_pwm.hpp"
#include "posc_trigger.hpp"
#include "posc_dataplotter_terminal.hpp"
#include "terminal_variables.hpp"
#include "core1_main.hpp"

mutex_t datac0_mutex;
DataForCore0 datac0_glob{&datac0_mutex};
extern DataForCore1 datac1_glob;

extern debug_data_t debug_data;
extern volatile void *ctrl_chan_adc_write;
extern volatile bool ctrl_channel_trigered;
extern volatile bool adc_chan_null_trigger;
extern volatile bool dma_cycle_forever;

namespace s0 {
void handle_selector_values(dt::MultiButton *selector, DataForCore1 &data_for_core1) {
    if (selector == &s0::dtsamplerate_selector) {
        uint32_t adc_div_32 = adc::div_from_samplerate(s0::selector_samplerates[selector->get_active_button()]);
        s1::dtadcdiv0.set_value(adc::get_div_int_u32(adc_div_32));
        s1::dtadcdiv1.set_value(adc::get_div_frac_u32(adc_div_32));

        const size_t num_of_channels = s0::dtchannel_selector.get_active_button() + 1;
        s0::dtsamplerate_disp.set_value(adc::samplerate_form_div(adc_div_32) / num_of_channels);
        s1::precise_adc_freq.set_value(adc::samplerate_form_div(adc_div_32));
        s1::dtfreq_prec0.set_value(static_cast<int32_t>(adc::samplerate_form_div(adc_div_32)));
        s1::dtfreq_prec1.set_value(0);
        data_for_core1.adc_div = adc_div_32;
    } else if (selector == &s0::dtsample_buff_selector) {
        data_for_core1.number_of_samples = s0::selector_sample_size[selector->get_active_button()];
    } else if (selector == &s0::dttrigger_selector) {
        data_for_core1.trigger_settings.set_edge(s0::selector_edges[selector->get_active_button()]);
    }
}
}  // namespace s0

namespace s2 {
void update_all_displays(pwm::Manager &pwm_manager) {
    s2::dtpwmmax.set_value(pwm_manager.get_max_freq());
    s2::dtpwmdiv0.set_value(pwm_manager.get_raw_div().int_div);
    s2::dtpwmdiv1.set_value(pwm_manager.get_raw_div().frac_div);
    s2::dtpwm_freq_disp.set_value(pwm_manager.get_freq());
    s2::dtfreq_prec0.set_value(s2::precise_pwm_freq.get_dec_part());
    s2::dtfreq_prec1.set_value(s2::precise_pwm_freq.get_frac_part());
}
void handle_selector_values(dt::MultiButton *selector, pwm::Manager &pwm_manager) {
    if (selector == &s2::dtpwm_func_selector) {
        const pwm::Manager::mode_t new_mode = s2::pwm_selector_modes[selector->get_active_button()];
        if (new_mode != pwm_manager.get_current_mode()) {
            if (new_mode == pwm::Manager::SINE || new_mode == pwm::Manager::TRIA) {
                pwm_manager.set_wrap_for_func();
                if (pwm_manager.get_current_mode() != pwm::Manager::SINE && pwm_manager.get_current_mode() != pwm::Manager::TRIA) {
                    pwm_manager.set_frequency(pwm::Manager::func_default_freq);
                }
            } else if (new_mode == pwm::Manager::PWM) {
                pwm_manager.set_wrap_for_frequency(s2::pwmmax_freqs[pwmmax_freqs_default]);
                pwm_manager.set_frequency(pwm_manager.get_max_freq());
                s2::dtpwmmax_selector.button_pressed(pwmmax_freqs_default);
            }
            pwm_manager.enable(new_mode);
            s2::precise_pwm_freq.set_max_freq(pwm_manager.get_max_freq());
            s2::precise_pwm_freq.set_min_freq(pwm_manager.get_min_freq());
            s2::precise_pwm_freq.set_value(pwm_manager.get_freq());
            update_all_displays(pwm_manager);
        }
        if (new_mode != pwm_manager.PWM) {
            s2::dtpwmmax_selector.deactivate_all_buttons();
        }
    } else if (selector == &s2::dtpwmmax_selector) {
        if (pwm_manager.get_current_mode() == pwm_manager.PWM) {
            if (pwm_manager.set_wrap_for_frequency(s2::pwmmax_freqs[selector->get_active_button()])) {
                pwm_manager.set_frequency(pwm_manager.get_max_freq());
                s2::precise_pwm_freq.set_max_freq(pwm_manager.get_max_freq());
                s2::precise_pwm_freq.set_min_freq(pwm_manager.get_min_freq());
                s2::precise_pwm_freq.set_value(pwm_manager.get_freq());
                update_all_displays(pwm_manager);
                pwm_manager.enable(pwm_manager.PWM);
            }
        } else {
            selector->deactivate_all_buttons();
        }
    }
}

}  // namespace s2

enum class ADCState_t : uint8_t {
    STOPPED,
    RUNNING_AUTO,
    RUNNING_NORMAL,
    WAITING,
    PAUSED,
};

int main() {
    constexpr unsigned int led_pin{25}, pwm_pin{16}, ps_pin{23};
    uint pwm_timer;
    DataForCore1 datac1_private;
    bool usb_was_connected{false};
    signed char rx_char;
    ADCState_t adc_state{ADCState_t::STOPPED};
    dt::MultiButton *pressed_selector;
    pwm::Manager pwm_manager;
    trig::mode_t trigger_mode;
    bool force_render_static_parts{false};

    init_dterminal();
    datac0_glob.init_mutex();
    datac1_glob.init_mutex();

    usb_stream.init();

    gpio_init(led_pin);
    gpio_set_dir(led_pin, true);

    gpio_init(ps_pin);
    gpio_set_dir(ps_pin, true);

    gpio_put(ps_pin, !s3::div_ps_toggle.is_pressed());

    pwm_manager.init(pwm_pin);

    pwm_manager.set_frac_div(s3::div_fract_toggle.is_pressed());

    for (dt::MultiButton *selector : s0::selector_array) {
        s0::handle_selector_values(selector, datac1_private);
    }

    s0::dtpretrigger.set_value(datac1_private.trigger_settings.get_pretrig());

    s1::dtfreq_prec0.set_value(s1::precise_adc_freq.get_dec_part());
    s1::dtfreq_prec1.set_value(s1::precise_adc_freq.get_frac_part());
    s1::dtadcclk.set_value(clock_get_hz(clk_adc));

    s2::dtpwm_duty_disp.set_value(pwm_manager.get_duty().duty_percent);
    pwm_manager.set_wrap_for_frequency(s2::pwmmax_freqs[s2::pwmmax_freqs_default]);
    pwm_manager.set_frequency(pwm_manager.get_max_freq());
    s2::precise_pwm_freq.set_max_freq(pwm_manager.get_max_freq());
    s2::precise_pwm_freq.set_min_freq(pwm_manager.get_min_freq());
    s2::precise_pwm_freq.set_value(pwm_manager.get_freq());
    s2::update_all_displays(pwm_manager);

    datac1_private.number_of_channels = s0::dtchannel_selector.get_active_button() + 1;

    datac1_glob.lock_blocking();
    datac1_glob = datac1_private;
    datac1_glob.unlock();

    multicore_launch_core1(core1_main);

    core1_message msg = get_msg_from_core1();
    if (msg != CORE1_STARTED) {
        panic("$$XUnexpectred message form Core1");
    }

    pwm_manager.enable(s2::pwm_selector_modes[s2::dtpwm_func_selector.get_active_button()]);

    while (true) {
        if (usb_stream.connected()) {
            if (usb_was_connected == false) {
                while (usb_stream.receive_timeout(0) > 0) {
                }
                usb_was_connected = true;
                dterminal.set_screen(s0::index);
                gpio_put(led_pin, true);
                busy_wait_ms(1);
                dataplotter.send_info("Connected");
                dataplotter.send_setting("noclickclr:40,48.5.31,48.5.160,48.5.164");
                busy_wait_ms(1);
                dterminal.print_static_elements(true);
                busy_wait_ms(1);
                dterminal.print_dynamic_elements(true);
                busy_wait_ms(1);
                datac1_glob.lock_blocking();
                datac1_glob = datac1_private;
                datac1_glob.unlock();

                trigger_mode = trig::mode_t::AUTO;
                s0::dttrigger_mode_selector.button_pressed(0);
                s0::dttrigger_mode.set_string(s0::dttmode_auto);
                adc_state = ADCState_t::RUNNING_AUTO;
                send_msg_to_core1(START_ADC_AUTO);
            }

            if (fifo_contains_value()) {
                core1_message c1msg = get_msg_from_core1();
                if (c1msg == ADC_DONE) {
                    datac0_glob.lock_blocking();
                    datac1_glob.lock_blocking();
                    float time_step = 1.0f / adc::samplerate_form_div(datac1_glob.adc_div);
                    uint8_t useful_bits = static_cast<uint8_t>(adc::sampling_size_t::U12);
                    static uint number_of_channels_before = 1;

                    etl::string<12> channels{};

                    etl::to_string(datac0_glob.first_channel, channels, false);

                    etl::to_string(datac0_glob.first_channel + 1, channels, false);
                    if (datac1_glob.number_of_channels > 1) {
                        for (uint i{1}; i < datac1_glob.number_of_channels; ++i) {
                            channels.push_back('+');
                            etl::to_string(((i + datac0_glob.first_channel) % datac1_glob.number_of_channels) + 1, channels, true);
                        }
                    }

                    channels.push_back(',');

                    if (number_of_channels_before > datac1_glob.number_of_channels && number_of_channels_before > 1) {
                        etl::string<12> clear_channels{};
                        etl::to_string(number_of_channels_before, clear_channels, true);
                        uint last_channel = etl::min(datac1_glob.number_of_channels, 1U);  // Handle number_of_channels = 0
                        for (uint i{number_of_channels_before - 1}; i > last_channel; --i) {
                            clear_channels.push_back('+');
                            etl::to_string(i, clear_channels, true);
                        }
                        clear_channels.push_back(',');
                        dataplotter.clear_channel_data(clear_channels);
                    }

                    number_of_channels_before = datac1_glob.number_of_channels;

                    const size_t trigger_div = etl::max(datac1_glob.number_of_channels, 1U);

                    if (datac0_glob.array2_samples > 0) {
                        dataplotter.send_channel_data_two(channels, time_step, datac0_glob.array1_samples, datac0_glob.array2_samples, useful_bits, 0.0f, 3.3f,
                                                          datac0_glob.trigger_index / trigger_div, datac0_glob.array1_start, adc_buffer_u16);
                    } else {
                        dataplotter.send_channel_data(channels, time_step, datac0_glob.array1_samples, useful_bits, 0.0f, 3.3f,
                                                      datac0_glob.trigger_index / trigger_div, datac0_glob.array1_start);
                    }

#ifndef NDEBUG
                    if (adc_state == ADCState_t::WAITING) {
                        dataplotter.send_info("Single 1 2 TI SC AI\n");
                        printf("%d %d %d %d %d", datac0_glob.array1_samples, datac0_glob.array2_samples, datac0_glob.trigger_index, debug_data.second_cycle,
                               debug_data.array_index);
                    }

#endif

                    datac1_glob = datac1_private;
                    datac1_glob.unlock();
                    datac0_glob.unlock();
                    if (adc_state == ADCState_t::RUNNING_AUTO) {
                        send_msg_to_core1(START_ADC_AUTO);
                        adc_state = ADCState_t::RUNNING_AUTO;
                    } else if (adc_state == ADCState_t::RUNNING_NORMAL) {
                        send_msg_to_core1(START_ADC_SINGLE);
                        adc_state = ADCState_t::RUNNING_NORMAL;
                    } else if (adc_state == ADCState_t::WAITING) {
                        adc_state = ADCState_t::PAUSED;
                        s0::dttrigger_mode.set_string(s0::dttmode_hold);
                    }
                }
            }

            rx_char = usb_stream.receive_timeout(0);
            bool force_dynamic_parts{false};
            if (rx_char > 0) {
#ifndef NDEBUG
                dataplotter.send_info("Received char: ");
                dataplotter.send(rx_char);
#endif

                uint8_t current_screen{dterminal.get_current_screen()};
                if (rx_char == '<') {
                    dterminal.previous_screen();
                    dterminal.print_static_elements(true);
                    force_dynamic_parts = true;
                } else if (rx_char == '>') {
                    dterminal.next_screen();
                    dterminal.print_static_elements(true);
                    force_dynamic_parts = true;
                } else if (rx_char == '?') {
                    dterminal.set_screen(sh::index);
                    dterminal.print_static_elements(true);
                }
#ifndef NDEBUG
                else if (rx_char == '!') {
                    dataplotter.send_info("\nRun Trig Done\n");
                    printf("%d %d %d", debug_data.adc_running, debug_data.trigger_detected, debug_data.adc_done);
                    dataplotter.send_info("\nCTrig Sc CW\n");
                    printf("%d %d %p", ctrl_channel_trigered, debug_data.second_cycle, ctrl_chan_adc_write);
                    dataplotter.send_info("\nDCF NT\n");
                    printf("%d %d", dma_cycle_forever, adc_chan_null_trigger);
                    dataplotter.send_info("\nAtx Ctx CAdd AAdd\n");
                    printf("%d %d ", dma::get_transfer_count(debug_data.dma_adc_chan), dma::get_transfer_count(debug_data.dma_ctrl_chan));
                    printf("%x %x", dma_channel_hw_addr(debug_data.dma_adc_chan)->write_addr, dma_channel_hw_addr(debug_data.dma_ctrl_chan)->write_addr);
                    dataplotter.send_info("Ctrl\n");
                    print_dma_ctrl(dma_channel_hw_addr(debug_data.dma_ctrl_chan));
                    printf("\n%d", dma_debug_hw->ch[debug_data.dma_ctrl_chan].tcr);
                    dataplotter.send_info("Adc\n");
                    print_dma_ctrl(dma_channel_hw_addr(debug_data.dma_adc_chan));
                    printf("\n%d", dma_debug_hw->ch[debug_data.dma_adc_chan].tcr);
                    dataplotter.send_info("DMA FIFO\n");
                    printf("%x", dma_hw->fifo_levels);
                }
#endif
                else if (current_screen == s0::index) {
                    if (rx_char == ')') {
                        datac1_private.trigger_settings.increment_level_small();
                        s0::dttrigger_level.set_value(datac1_private.trigger_settings.get_level());
                    } else if (rx_char == '+') {
                        datac1_private.trigger_settings.increment_level();
                        s0::dttrigger_level.set_value(datac1_private.trigger_settings.get_level());
                    } else if (rx_char == '(') {
                        datac1_private.trigger_settings.decrement_level_small();
                        s0::dttrigger_level.set_value(datac1_private.trigger_settings.get_level());
                    } else if (rx_char == '-') {
                        datac1_private.trigger_settings.decrement_level();
                        s0::dttrigger_level.set_value(datac1_private.trigger_settings.get_level());
                    } else if (rx_char == '{') {
                        s0::dtpretrigger.set_value(datac1_private.trigger_settings.decrement_pretrig());
                    } else if (rx_char == '}') {
                        s0::dtpretrigger.set_value(datac1_private.trigger_settings.increment_pretrig());
                    } else {
                        pressed_selector = get_pressed_selector(rx_char, s0::selector_array);
                        if (pressed_selector == &s0::dttrigger_mode_selector) {
                            trigger_mode = static_cast<trig::mode_t>(pressed_selector->get_active_button());

                            if (trigger_mode == trig::mode_t::AUTO && adc_state != ADCState_t::RUNNING_AUTO) {
                                s0::dttrigger_mode.set_string(s0::dttmode_auto);
                                send_msg_to_core1(STOP_ADC);
                                datac1_glob.lock_blocking();
                                datac1_glob = datac1_private;
                                datac1_glob.unlock();
                                send_msg_to_core1(START_ADC_AUTO);
                                adc_state = ADCState_t::RUNNING_AUTO;

                            } else if (trigger_mode == trig::mode_t::NORM && adc_state != ADCState_t::RUNNING_NORMAL) {
                                s0::dttrigger_mode.set_string(s0::dttmode_norm);
                                if (adc_state != ADCState_t::WAITING) {
                                    send_msg_to_core1(STOP_ADC);
                                    datac1_glob.lock_blocking();
                                    datac1_glob = datac1_private;
                                    datac1_glob.unlock();
                                    send_msg_to_core1(START_ADC_SINGLE);
                                }
                                adc_state = ADCState_t::RUNNING_NORMAL;
                            } else if (trigger_mode == trig::mode_t::WAIT && adc_state != ADCState_t::WAITING) {
                                s0::dttrigger_mode.set_string(s0::dttmode_wait);
                                if (adc_state != ADCState_t::RUNNING_NORMAL) {
                                    send_msg_to_core1(STOP_ADC);
                                    datac1_glob.lock_blocking();
                                    datac1_glob = datac1_private;
                                    datac1_glob.unlock();
                                    send_msg_to_core1(START_ADC_SINGLE);
                                }
                                adc_state = ADCState_t::WAITING;
                            } else if (trigger_mode == trig::mode_t::HOLD && adc_state != ADCState_t::PAUSED) {
                                s0::dttrigger_mode.set_string(s0::dttmode_hold);
                                send_msg_to_core1(STOP_ADC);
                                adc_state = ADCState_t::PAUSED;
                            }
                        } else if (pressed_selector == &s0::dtchannel_selector) {
                            const auto previous = datac1_private.number_of_channels - 1;
                            const auto new_num_of_channels = pressed_selector->get_active_button();
                            datac1_private.number_of_channels = new_num_of_channels + 1;
                            if (previous != new_num_of_channels && new_num_of_channels < s0::max_num_of_channels) {
                                s0::dtsample_buff_part.set_static_part(s0::channel_sample_buff_strs[new_num_of_channels]);
                                s0::dtsamplerate_part.set_static_part(s0::channel_samplerate_strs[new_num_of_channels]);
                                force_render_static_parts = true;

                                uint32_t adc_div_32 = adc::div_from_samplerate(s1::precise_adc_freq.get_freq());
                                const size_t num_of_channels = new_num_of_channels + 1;
                                s0::dtsamplerate_disp.set_value(adc::samplerate_form_div(adc_div_32) / num_of_channels);
                            }
                        }
                        s0::handle_selector_values(pressed_selector, datac1_private);
                    }
                } else if (current_screen == s1::index) {
                    if (rx_char == 'A') {
                        uint32_t adc_div_32 = adc::div_from_samplerate(s1::precise_adc_freq.get_freq());
                        if (!s3::div_fract_toggle.is_pressed()) {
                            adc_div_32 &= ~(ADC_DIV_FRAC_BITS);
                        }
                        s1::dtadcdiv0.set_value(adc::get_div_int_u32(adc_div_32));
                        s1::dtadcdiv1.set_value(adc::get_div_frac_u32(adc_div_32));
                        const size_t num_of_channels = s0::dtchannel_selector.get_active_button() + 1;
                        s0::dtsamplerate_disp.set_value(adc::samplerate_form_div(adc_div_32) / num_of_channels);
                        s0::dtsamplerate_selector.deactivate_all_buttons();
                        datac1_private.adc_div = adc_div_32;
                    } else if (rx_char == 'M' || rx_char == 'm') {
                        if (rx_char == 'M') {
                            s1::precise_adc_freq.set_max();
                        } else {
                            s1::precise_adc_freq.set_min();
                        }
                        s1::dtfreq_prec0.set_value(s1::precise_adc_freq.get_dec_part());
                        s1::dtfreq_prec1.set_value(s1::precise_adc_freq.get_frac_part());
                    } else if (s1::precise_adc_freq.handle_incoming_char(rx_char)) {
                        s1::dtfreq_prec0.set_value(s1::precise_adc_freq.get_dec_part());
                        s1::dtfreq_prec1.set_value(s1::precise_adc_freq.get_frac_part());
                    }
                } else if (current_screen == s2::index) {
                    if (rx_char == '{') {
                        pwm_manager.decrement_duty();
                        s2::dtpwm_duty_disp.set_value(pwm_manager.get_duty().duty_percent);
                    } else if (rx_char == '}') {
                        pwm_manager.increment_duty();
                        s2::dtpwm_duty_disp.set_value(pwm_manager.get_duty().duty_percent);
                    } else if (rx_char == 'A') {
                        pwm_manager.set_frequency(s2::precise_pwm_freq.get_freq());
                        s2::dtpwmdiv0.set_value(pwm_manager.get_raw_div().int_div);
                        s2::dtpwmdiv1.set_value(pwm_manager.get_raw_div().frac_div);
                        s2::dtpwm_freq_disp.set_value(pwm_manager.get_freq());
                    } else if (rx_char == 'M' || rx_char == 'm') {
                        if (rx_char == 'M') {
                            s2::precise_pwm_freq.set_max();
                        } else {
                            s2::precise_pwm_freq.set_min();
                        }
                        s2::dtfreq_prec0.set_value(s2::precise_pwm_freq.get_dec_part());
                        s2::dtfreq_prec1.set_value(s2::precise_pwm_freq.get_frac_part());
                    } else if (s2::precise_pwm_freq.handle_incoming_char(rx_char)) {
                        s2::dtfreq_prec0.set_value(s2::precise_pwm_freq.get_dec_part());
                        s2::dtfreq_prec1.set_value(s2::precise_pwm_freq.get_frac_part());
                    } else {
                        pressed_selector = get_pressed_selector(rx_char, s2::selector_array);

                        s2::handle_selector_values(pressed_selector, pwm_manager);
                    }
                } else if (current_screen == s3::index) {
                    if (rx_char == s3::div_fract_toggle.get_button_char()) {
                        s3::div_fract_toggle.button_toggle();
                        pwm_manager.set_frac_div(s3::div_fract_toggle.is_pressed());
                    } else if (rx_char == s3::div_ps_toggle.get_button_char()) {
                        s3::div_ps_toggle.button_toggle();
                        gpio_put(ps_pin, !s3::div_ps_toggle.is_pressed());
                    }
                }
            }

            if (force_render_static_parts) {
                dterminal.print_static_elements(false);
                force_render_static_parts = false;
            }
            dterminal.print_dynamic_elements(force_dynamic_parts);
        } else {
            if (usb_was_connected) {
                send_msg_to_core1(STOP_ADC);
                adc_state = ADCState_t::STOPPED;
            }
            usb_was_connected = false;
            gpio_put(led_pin, false);
        }
    }
    return 0;
}