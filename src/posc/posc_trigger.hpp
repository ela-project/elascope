#pragma once
#include <stddef.h>
#include <stdint.h>
#include "pico/types.h"
#include "posc_adc.hpp"

namespace trig {

enum class mode_t : uint8_t {
    AUTO = 0,
    NORM = 1,
    WAIT = 2,
    HOLD = 3,
};

class Settings {
   public:
    static constexpr uint16_t max_raw_u12{0xFFF};
    static constexpr uint16_t max_raw_u8{0xFF};
    static constexpr uint16_t min_raw{0x00};
    static constexpr uint16_t max_voltage_mV{3300};
    static constexpr uint16_t max_trigger_level_mV{3200};
    static constexpr uint16_t min_trigger_level_mV{100};
    static constexpr uint16_t level_big_step_mV{500};
    static constexpr uint16_t level_small_step_mV{100};

    static constexpr uint8_t max_pretrig{80};
    static constexpr uint8_t min_pretrig{0};
    static constexpr uint8_t pretrig_step{20};

    enum class Edge : uint8_t { RISING, FALLING };

    Settings(uint16_t max_raw = max_raw_u12)
        : _max_raw_level{max_raw}, _trigger_level_mV{500}, _pretrigger_percent(20), _trigger_edge{Edge::RISING}, _trigger_channel{0} {
        set_raw_level();
    }

    void set_edge(Edge trigger_edge) {
        _trigger_edge = trigger_edge;
    }

    float get_level() const {
        return static_cast<float>(_trigger_level_mV) / 1000.0f;
    }

    uint16_t get_level_raw() const {
        return _trigger_level_raw;
    }

    Edge get_edge() const {
        return _trigger_edge;
    }

    void increment_level_small() {
        increment_level(level_small_step_mV);
    }

    void decrement_level_small() {
        decrement_level(level_small_step_mV);
    }

    void increment_level(uint16_t increment = level_big_step_mV) {
        uint16_t new_level{_trigger_level_mV};
        if (increment >= max_trigger_level_mV) {
            new_level = max_trigger_level_mV;
        } else if (max_trigger_level_mV - increment < new_level) {
            new_level = max_trigger_level_mV;
        } else if (increment > 0) {
            new_level = (new_level + increment) - (new_level % increment);
        }
        _trigger_level_mV = etl::min(max_trigger_level_mV, new_level);
        set_raw_level();
    }

    void decrement_level(uint16_t decrement = level_big_step_mV) {
        uint16_t new_level{_trigger_level_mV};
        if (decrement >= max_trigger_level_mV) {
            new_level = min_trigger_level_mV;
        } else if (min_trigger_level_mV + decrement > new_level) {
            new_level = min_trigger_level_mV;
        } else if (decrement > 0) {
            if (new_level % decrement == 0) {
                new_level -= decrement;
            } else {
                new_level = (new_level - new_level % decrement);
            }
        }
        _trigger_level_mV = etl::max(min_trigger_level_mV, new_level);
        set_raw_level();
    }

    uint8_t increment_pretrig() {
        if (_pretrigger_percent < max_pretrig) {
            _pretrigger_percent += pretrig_step;
        }
        return _pretrigger_percent;
    }

    uint8_t decrement_pretrig() {
        if (_pretrigger_percent > min_pretrig) {
            _pretrigger_percent -= pretrig_step;
        }
        return _pretrigger_percent;
    }

    const uint8_t &get_pretrig() {
        return _pretrigger_percent;
    }

    bool detect_edge_raw(uint16_t sample_before, uint16_t sample_after) const {
        if (_trigger_edge == Edge::RISING) {
            return (sample_before < _trigger_level_raw) && (sample_after >= _trigger_level_raw);
        } else if (_trigger_edge == Edge::FALLING) {
            return (sample_before > _trigger_level_raw) && (sample_after <= _trigger_level_raw);
        }
        return false;
    }

    void set_sampling_size(adc::sampling_size_t sp_mode) {
        if (sp_mode == adc::sampling_size_t::U8) {
            _max_raw_level = max_raw_u8;
        } else if (sp_mode == adc::sampling_size_t::U12) {
            _max_raw_level = max_raw_u12;
        }
        set_raw_level();
    }

    uint16_t get_initial_sample_value() const {
        if (_trigger_edge == Edge::FALLING) {
            return min_raw;
        } else {
            return _max_raw_level;
        }
    }

    size_t calculate_pretrig_count(size_t overall_count) {
        return (overall_count * _pretrigger_percent) / 100;
    }

    void set_trigger_channel(uint channel) {
        if (channel < 4) _trigger_channel = channel;
    }

    uint get_trigger_channel() {
        return _trigger_channel;
    }

   private:
    void set_raw_level() {
        uint32_t temp = (static_cast<uint32_t>(_max_raw_level) * _trigger_level_mV) / (max_voltage_mV / 10);
        _trigger_level_raw = (temp + 5U) / 10U;
    }

   private:
    uint16_t _max_raw_level;
    uint16_t _trigger_level_mV;
    uint16_t _trigger_level_raw;
    uint8_t _pretrigger_percent;
    Edge _trigger_edge;
    uint _trigger_channel;
};
}  // namespace trig