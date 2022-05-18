#pragma once
#include <stddef.h>
#include <stdint.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"

namespace adc {

inline constexpr float min_adc_div{96.0f};
inline constexpr float max_samplerate{500000.0f};
inline constexpr float max_adc_div{1.0 + (ADC_DIV_INT_BITS >> ADC_DIV_INT_LSB)};

enum sampling_size_t : uint8_t {
    U8 = 8U,
    U12 = 12U,
};

inline uint32_t get_div_int_raw() {
    return adc_hw->div >> ADC_DIV_INT_LSB;
}

inline uint32_t get_div_int_u32(uint32_t div) {
    div = (div >> ADC_DIV_INT_LSB) + 1;
    if (div < 96) {
        div = 96;
    }
    return div;
}

inline uint32_t get_div_int() {
    return get_div_int_u32(adc_hw->div);
}

inline uint32_t get_div_frac_u32(uint32_t div) {
    return div & ADC_DIV_FRAC_BITS;
}

inline uint32_t get_div_frac() {
    return get_div_frac_u32(adc_hw->div);
}

inline void set_clkdiv_u32(uint32_t clkdiv) {
    adc_hw->div = clkdiv;
}

inline uint32_t div_u32_from_float(float clkdiv) {
    return (uint32_t)((clkdiv - 1.0f) * (float)(1 << ADC_DIV_INT_LSB));
}

inline float div_float_from_u32(uint32_t clkdiv) {
    float div_temp = (static_cast<float>(clkdiv) / static_cast<float>(1 << ADC_DIV_INT_LSB)) + 1.0f;
    return (div_temp > adc::min_adc_div) ? div_temp : adc::min_adc_div;
}

inline float get_adc_real_div() {
    float div{div_float_from_u32(adc_hw->div)};
    return div < adc::min_adc_div ? adc::min_adc_div : div;
}

inline float samplerate_form_div(uint32_t div) {
    uint32_t adc_clk{clock_get_hz(clk_adc)};
    return static_cast<float>(adc_clk) / (div_float_from_u32(div));
}

inline float get_samplerate() {
    uint32_t adc_clk{clock_get_hz(clk_adc)};
    return samplerate_form_div(adc_hw->div);
}

inline uint32_t div_from_samplerate(float samplerate) {
    if (samplerate >= adc::max_samplerate) {
        return div_u32_from_float(1.0f);
    }

    float adc_clk{static_cast<float>(clock_get_hz(clk_adc))};
    float div{adc_clk / samplerate};
    if (div > adc::max_adc_div) div = adc::max_adc_div;
    return div_u32_from_float(div);
}

}  // namespace adc