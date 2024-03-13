#pragma once
#include <stdint.h>
#include <stddef.h>
#include "pico/multicore.h"
#include "pico/mutex.h"

#include "posc_trigger.hpp"

inline constexpr size_t adc_buffer_size_u16{110000};
extern uint16_t adc_buffer_u16[adc_buffer_size_u16];

void core1_main();

struct debug_data_t {
    bool adc_running;
    bool trigger_detected;
    bool adc_done;
    int dma_adc_chan;
    int dma_ctrl_chan;
    uint32_t second_cycle;
    uint32_t array_index;

    void clear() {
        adc_running = true;
        trigger_detected = false;
        adc_done = false;
        second_cycle = 0;
    }
};

class MulticoreData {
   public:
    MulticoreData(mutex_t *mutex = nullptr) : _mutex(mutex) {
    }

    void init_mutex() {
        if (_mutex != nullptr) mutex_init(_mutex);
    }

    bool try_lock(uint32_t *owner_out) {
        if (_mutex != nullptr)
            return mutex_try_enter(_mutex, owner_out);
        else
            return false;
    }

    void lock_blocking() {
        if (_mutex != nullptr) mutex_enter_blocking(_mutex);
    }

    void unlock() {
        if (_mutex != nullptr) mutex_exit(_mutex);
    }

    MulticoreData &operator=(const MulticoreData &other) {
        return *this;
    }

   private:
    mutex_t *const _mutex;
};

class DataForCore0 : public MulticoreData {
   public:
    DataForCore0(mutex_t *mutex = nullptr) : MulticoreData(mutex) {
    }

    void set_array1(uint16_t *array1, size_t length1, size_t length2) {
        array1_start = array1;
        array1_samples = length1;
        array2_samples = length2;
    }

    size_t array1_samples;
    size_t array2_samples;
    size_t trigger_index;
    uint32_t first_channel;
    uint16_t *array1_start;
};

class DataForCore1 : public MulticoreData {
   public:
    using TriggerSettings = trig::Settings;
    using sampling_size_t = adc::sampling_size_t;

    DataForCore1(mutex_t *mutex = nullptr) : MulticoreData(mutex) {
    }

    size_t pretrigger_samples;
    size_t posttrigger_samples;
    size_t number_of_samples;
    uint32_t adc_div;
    uint number_of_channels;
    TriggerSettings trigger_settings;
};

enum core0_message : uint32_t {
    START_ADC_AUTO = 0x00000000U,
    STOP_ADC,
    START_ADC_SINGLE,
};

enum core1_message : uint32_t {
    CORE1_STARTED = 0x80000000U,
    ADC_DONE,
};

inline bool fifo_contains_value() {
    return multicore_fifo_get_status() & SIO_FIFO_ST_VLD_BITS;
};

inline core1_message get_msg_from_core1() {
    return static_cast<core1_message>(multicore_fifo_pop_blocking());
};

inline core0_message get_msg_from_core0() {
    return static_cast<core0_message>(multicore_fifo_pop_blocking());
};

inline void send_msg_to_core1(core0_message msg) {
    multicore_fifo_push_blocking(static_cast<uint32_t>(msg));
};

inline void send_msg_to_core0(core1_message msg) {
    multicore_fifo_push_blocking(static_cast<uint32_t>(msg));
};
