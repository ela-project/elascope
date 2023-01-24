#pragma once
#include <stdint.h>
#include <cstddef>
#include "posc_dterminal_dynamic.hpp"
#include "posc_pwm.hpp"
#include "posc_trigger.hpp"
#include "posc_dataplotter_stream.hpp"
#include "posc_dataplotter_terminal.hpp"
#include "posc_precise_freq.hpp"

extern const comm::USBStream usb_stream;
extern const comm::DataPlotterStream dataplotter;
extern dt::Terminal dterminal;

namespace sh {
inline constexpr uint8_t index{dt::Terminal::help_screen};
}

namespace s0 {
inline constexpr uint8_t index{dt::Terminal::start_screen};
inline constexpr trig::Settings::Edge selector_edges[]{trig::Settings::Edge::RISING, trig::Settings::Edge::FALLING};

extern dt::MultiButton dtchannel_selector;

extern dt::FloatNumber dttrigger_level;

extern dt::IntNumber dtpretrigger;
extern dt::MultiButton dttrigger_selector;

inline constexpr char dttmode_auto[]{"\e[48;5;31mAUTO\e[0m"};
inline constexpr char dttmode_norm[]{"\e[48;5;31mNORM\e[0m"};
inline constexpr char dttmode_wait[]{"\e[48;5;160mWAIT\e[0m"};
inline constexpr char dttmode_hold[]{"\e[48;5;164mHOLD\e[0m"};
extern dt::Strings dttrigger_mode;
extern dt::MultiButton dttrigger_mode_selector;

inline constexpr float selector_samplerates[]{1e3f, 2e3f, 5e3f, 10e3f, 20e3f, 50e3f, 100e3f, 200e3f, 500e3f};
inline constexpr size_t selector_samplerates_default{8};
extern dt::MultiButton dtsamplerate_selector;
extern dt::FloatNumber dtsamplerate_disp;

inline constexpr size_t selector_sample_size[]{1024, 2048, 4096, 8192};
inline constexpr size_t selector_sample_size_default{0};
extern dt::MultiButton dtsample_buff_selector;

inline constexpr dt::MultiButton *selector_array[]{&dtchannel_selector, &dttrigger_selector, &dtsamplerate_selector, &dttrigger_mode_selector, &dtsample_buff_selector};
}  // namespace s0

namespace s1 {
inline constexpr uint8_t index{dt::Terminal::start_screen + 1};

extern PreciseFreq precise_adc_freq;
extern dt::IntNumber dtfreq_prec0;
extern dt::IntNumber dtfreq_prec1;

extern dt::IntNumber dtadcdiv0;
extern dt::IntNumber dtadcdiv1;
extern dt::IntNumber dtadcclk;
}  // namespace s1

namespace s2 {
inline constexpr uint8_t index{dt::Terminal::start_screen + 2};
extern dt::IntNumber dtpwm_duty_disp;

extern PreciseFreq precise_pwm_freq;
extern dt::IntNumber dtfreq_prec0;
extern dt::IntNumber dtfreq_prec1;

constexpr pwm::Manager::mode_t pwm_selector_modes[]{pwm::Manager::DISABLED, pwm::Manager::PWM, pwm::Manager::SINE, pwm::Manager::TRIA};
inline constexpr size_t pwm_selector_modes_default = 1;
extern dt::MultiButton dtpwm_func_selector;

extern dt::IntNumber dtpwmdiv1;
extern dt::IntNumber dtpwmdiv0;

extern dt::FloatNumber dtpwm_freq_disp;

extern dt::IntNumber dtpwmmax;

inline constexpr float pwmmax_freqs[]{2e3, 10e3, 50e3, 100e3, 500e3, 1e6, 5e6};
inline constexpr size_t pwmmax_freqs_default = 1;
extern dt::MultiButton dtpwmmax_selector;

inline constexpr dt::MultiButton *selector_array[]{&dtpwm_func_selector, &dtpwmmax_selector};

}  // namespace s2

namespace s3 {
inline constexpr uint8_t index{dt::Terminal::start_screen + 3};

extern dt::DTButton div_fract_toggle;
extern dt::DTButton div_ps_toggle;

}

template <size_t ARRAY_SIZE>
dt::MultiButton *get_pressed_selector(signed char rx_char, dt::MultiButton *const (&selector_array)[ARRAY_SIZE]) {
    int selector_index;
    for (dt::MultiButton *selector : selector_array) {
        if (selector->button_pressed_char(rx_char) >= 0) {
            return selector;
        }
    }
    return nullptr;
}

template <size_t ARRAY_SIZE>
void init_dterminal_base(dt::Terminal &dterminal, dt::StaticPart *const (&dterminal_parts)[ARRAY_SIZE], uint8_t screen = 0) {
    dterminal.set_screen(screen);
    for (dt::StaticPart *dterminal_part : dterminal_parts) {
        dterminal.push_terminal_part(dterminal_part);
    }
}

void init_dterminal();
