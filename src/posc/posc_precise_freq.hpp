#pragma once
#include <stddef.h>
#include <stdint.h>
#include <etl/iterator.h>
#include <etl/algorithm.h>

class PreciseFreq {
   public:
    static constexpr int max_digits{10};

    template <size_t STRING_SIZE>
    PreciseFreq(const char (&symb_up)[STRING_SIZE], const char (&symb_down)[STRING_SIZE], int32_t min_freq10, int32_t max_freq10)
        : _freq_prec_symb_up{symb_up},
          _freq_prec_symb_down{symb_down},
          _string_size{STRING_SIZE - 1},
          _min_freq10{min_freq10},
          _max_freq10{max_freq10},
          _freq10(max_freq10) {
    }

    bool handle_incoming_char(char c) {
        const char *position;
        if (c >= 'a') {
            position = etl::find(&_freq_prec_symb_down[0], &_freq_prec_symb_down[_string_size], c);
            if (position != &_freq_prec_symb_down[_string_size]) {
                decrement_digit(etl::distance(_freq_prec_symb_down, position));
                return true;
            }
        } else {
            position = etl::find(&_freq_prec_symb_up[0], &_freq_prec_symb_up[_string_size], c);
            if (position != &_freq_prec_symb_up[_string_size]) {
                increment_digit(etl::distance(_freq_prec_symb_up, position));
                return true;
            }
        }
        return false;
    }

    void set_value(float samplerate) {
        int32_t freq10 = static_cast<int32_t>(samplerate * 100.0f);
        if (freq10 >= _min_freq10 && freq10 <= _max_freq10) {
            _freq10 = freq10;
        }
    }

    void set_max_freq(float freq) {
        _max_freq10 = static_cast<int32_t>(freq * 100.0f);
    }

    void set_min_freq(float freq) {
        _min_freq10 = static_cast<int32_t>(freq * 100.0f);
    }

    float get_freq() {
        return static_cast<float>(_freq10) / 100.0f;
    }

    void increment_digit(int digit) {
        int32_t freq10{_freq10};
        if (digit >= 0 && digit < max_digits) {
            int32_t dec = get_dec(digit);
            freq10 = freq10 + dec;
            if (freq10 > _max_freq10) {
                _freq10 = _max_freq10;
            } else {
                _freq10 = freq10;
            }
        }
    }

    void decrement_digit(int digit) {
        int32_t freq10{_freq10};
        if (digit >= 0 && digit < max_digits) {
            int32_t dec = get_dec(digit);
            freq10 = freq10 - dec;
            if (freq10 < _min_freq10) {
                _freq10 = _min_freq10;
            } else {
                _freq10 = freq10;
            }
        }
    }

    void reset() {
        set_max();
    }
    void set_min(){
        _freq10 = _min_freq10;
    }
    void set_max(){
        _freq10 = _max_freq10;
    }
    long get_dec_part() {
        return _freq10 / 100;
    }
    long get_frac_part() {
        return _freq10 % 100;
    }

   private:
    int32_t get_dec(int32_t digit) {
        int32_t dec{1};
        for (size_t i{0}; i < digit; i++) {
            dec = dec * 10;
        }
        return dec;
    }

   private:
    const char *const _freq_prec_symb_up;
    const char *const _freq_prec_symb_down;
    const size_t _string_size;
    int32_t _min_freq10;
    int32_t _max_freq10;
    int32_t _freq10;
};