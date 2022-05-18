#pragma once
#include <stdint.h>

#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "posc_dataplotter_terminal.hpp"

namespace pwm {
inline constexpr float calculate_div_float(uint8_t int_div, uint8_t frac_div) {
    return static_cast<float>(int_div) + static_cast<float>(frac_div) / 16.0f;
}

struct div_t {
    static constexpr uint8_t max_int{0xFF};
    static constexpr uint8_t max_frac{0xF};
    static constexpr uint8_t min_int{0x01};
    static constexpr uint8_t min_frac{0x0};
    static constexpr float max_float{calculate_div_float(max_int, max_frac)};
    static constexpr float min_float{calculate_div_float(min_int, min_frac)};

    uint8_t int_div;
    uint8_t frac_div;

    void set_div_from_float(float div) {
        if (div >= max_float) {
            set_max();
        } else if (div <= min_float) {
            set_min();
        } else {
            int_div = (uint8_t)div;
            frac_div = (uint8_t)((div - int_div) * (0x01 << 4));
        }
    }

    void set_min() {
        int_div = min_int;
        frac_div = min_frac;
    }

    void set_max() {
        int_div = max_int;
        frac_div = max_frac;
    }

    static div_t get_min_div() {
        return {min_int, min_frac};
    }

    static div_t get_max_div() {
        return {max_int, max_frac};
    }
};

inline float calculate_frequency(uint16_t wrap, div_t div) {
    const float _sys_clk = static_cast<float>(clock_get_hz(clk_sys));
    return _sys_clk / (static_cast<float>(wrap) * calculate_div_float(div.int_div, div.frac_div));
}

inline uint32_t calculate_wrap_for_max_freq(float freq) {
    const float _sys_clk = static_cast<float>(clock_get_hz(clk_sys));
    return static_cast<uint32_t>(_sys_clk / freq);
}

inline float max_freq(uint16_t wrap) {
    return calculate_frequency(wrap, div_t::get_min_div());
}

inline float min_freq(uint16_t wrap) {
    return calculate_frequency(wrap, div_t::get_max_div());
}

struct duty_t {
    static constexpr int8_t duty_increment{5};
    static constexpr int8_t duty_max{100};
    static constexpr int8_t duty_min{0};

    void increment_duty() {
        int8_t duty = duty_percent + duty_increment;
        if (duty >= duty_max) {
            duty_percent = duty_max;
        } else {
            duty_percent = duty;
        }
    }
    void decrement_duty() {
        int8_t duty = duty_percent - duty_increment;
        if (duty <= duty_min) {
            duty_percent = duty_min;
        } else {
            duty_percent = duty;
        }
    }
    uint16_t calc_chan_level(uint16_t wrap) {
        return (static_cast<uint32_t>(wrap) * duty_percent) / 100;
    }

    int8_t duty_percent;
};

class Manager {
   public:
    enum mode_t : int8_t {
        DISABLED = -1,
        PWM = 0,
        SINE = 1,
        TRIA = 2,
    };

    static constexpr uint32_t func_pulses{64};
    static constexpr float func_default_freq{10000};
    static constexpr uint16_t func_wrap_count{250};
    static constexpr uint16_t sine[func_pulses]{124, 136, 148, 160, 172, 183, 193, 203, 212, 220, 228, 234, 239, 243, 246, 248, 249, 248, 246, 243, 239, 234,
                                                228, 220, 212, 203, 193, 183, 172, 160, 148, 136, 124, 112, 100, 88,  76,  65,  55,  45,  36,  28,  20,  14,
                                                9,   5,   2,   0,   0,   0,   2,   5,   9,   14,  20,  28,  36,  45,  55,  65,  76,  88,  100, 112};
    static constexpr uint16_t tria[func_pulses]{124, 132, 140, 147, 155, 163, 171, 178, 186, 194, 202, 210, 217, 225, 233, 241, 249, 241, 233, 225, 217, 210,
                                                202, 194, 186, 178, 171, 163, 155, 147, 140, 132, 124, 116, 108, 101, 93,  85,  77,  70,  62,  54,  46,  38,
                                                31,  23,  15,  7,   0,   7,   15,  23,  31,  38,  46,  54,  62,  70,  77,  85,  93,  101, 108, 116};
    static constexpr const void *sine_addr{&sine[0]};
    static constexpr const void *tria_addr{&tria[0]};

    static constexpr uint16_t min_wrap{10};
    static constexpr uint16_t max_wrap{62500};

    Manager()
        : _duty{50},
          _current_mode{PWM},
          _wrap{func_wrap_count},
          _div{div_t::min_int, div_t::min_frac},
          _dma_pwm_chan{-1},
          _dma_ctrl_chan{-1},
          func_array_addr{sine_addr},
          ctrl_chan_read_addr{&func_array_addr} {
    }

    void init(uint pwm_pin) {
        if (_dma_pwm_chan < 0 && _dma_ctrl_chan < 0) {
            gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
            _pwm_pin = pwm_pin;

            _slice = pwm_gpio_to_slice_num(pwm_pin);
            _channel = pwm_gpio_to_channel(pwm_pin);

            _dma_pwm_chan = dma_claim_unused_channel(true);
            _dma_ctrl_chan = dma_claim_unused_channel(true);

            dma_channel_config chan_cfg = dma_channel_get_default_config(_dma_ctrl_chan);
            channel_config_set_transfer_data_size(&chan_cfg, DMA_SIZE_32);
            channel_config_set_read_increment(&chan_cfg, false);
            channel_config_set_write_increment(&chan_cfg, false);
            dma_channel_configure(_dma_ctrl_chan, &chan_cfg, &(dma_channel_hw_addr(_dma_pwm_chan)->al3_read_addr_trig), ctrl_chan_read_addr, 1, false);

            chan_cfg = dma_channel_get_default_config(_dma_pwm_chan);
            channel_config_set_transfer_data_size(&chan_cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&chan_cfg, true);
            channel_config_set_write_increment(&chan_cfg, false);
            channel_config_set_dreq(&chan_cfg, pwm_get_dreq(_slice));
            channel_config_set_chain_to(&chan_cfg, _dma_ctrl_chan);
            dma_channel_configure(_dma_pwm_chan, &chan_cfg, &pwm_hw->slice[_slice].cc, sine_addr, func_pulses, false);
        }
    };

    void enable(mode_t mode) {
        disable();
        if (mode != DISABLED) {
            if (mode == SINE) {
                func_array_addr = sine_addr;
                _wrap = func_wrap_count;
                dma_channel_start(_dma_ctrl_chan);
            } else if (mode == TRIA) {
                func_array_addr = tria_addr;
                _wrap = func_wrap_count;
                dma_channel_start(_dma_ctrl_chan);
            } else if (mode == PWM) {
                pwm_set_chan_level(_slice, _channel, _duty.calc_chan_level(_wrap));
            }

            setup_pwm();
            pwm_set_enabled(_slice, true);
        }
        _current_mode = mode;
    }

    const mode_t &get_current_mode() {
        return _current_mode;
    }

    void set_frequency(float frequency) {
        const float max_pmw_freq = get_max_freq();
        if (frequency >= max_pmw_freq) {
            _div.set_min();
        } else if (frequency <= get_min_freq()) {
            _div.set_max();
        } else {
            const float div = max_pmw_freq / frequency;
            _div.set_div_from_float(div);
        }
        set_pwm_div();
    }

    void set_wrap_for_func() {
        _wrap = func_wrap_count;
    }

    bool set_wrap_for_frequency(float max_frequency) {
        uint32_t wrap = calculate_wrap_for_max_freq(max_frequency);
        if (wrap != _wrap) {
            if (wrap > max_wrap) {
                _wrap = max_wrap;
            } else if (wrap < min_wrap) {
                _wrap = min_wrap;
            } else {
                _wrap = wrap;
            }
            return true;
        }
        return false;
    }

    div_t get_raw_div() {
        return _div;
    }

    float get_freq() {
        return calculate_frequency(_wrap, _div);
    }

    float get_max_freq() {
        return max_freq(_wrap);
    }

    float get_min_freq() {
        return min_freq(_wrap);
    }

    void increment_duty() {
        _duty.increment_duty();
        update_pwm_duty();
    }

    void decrement_duty() {
        _duty.decrement_duty();
        update_pwm_duty();
    }

    duty_t get_duty() {
        return _duty;
    }

   private:
    void setup_pwm() {
        pwm_set_wrap(_slice, _wrap - 1);
        set_pwm_div();
        update_pwm_duty();
    }

    void set_pwm_div() {
        pwm_set_clkdiv_int_frac(_slice, _div.int_div, _div.frac_div);
    }

    void update_pwm_duty() {
        if (_current_mode == PWM) {
            pwm_set_chan_level(_slice, _channel, _duty.calc_chan_level(_wrap));
        }
    }

    void disable() {
        func_array_addr = 0;
        dma_channel_abort(_dma_pwm_chan);
        pwm_set_chan_level(_slice, _channel, 0);
        pwm_set_counter(_slice, 1);
        pwm_set_enabled(_slice, false);
    }

   private:
    uint _pwm_pin;
    duty_t _duty;
    mode_t _current_mode;
    div_t _div;
    uint16_t _wrap;
    uint _slice;
    uint _channel;
    int _dma_pwm_chan;
    int _dma_ctrl_chan;
    const void *func_array_addr;
    const void **const ctrl_chan_read_addr;
};

}  // namespace pwm