#include "pico/stdlib.h"
#include "posc_comms.hpp"
#include "posc_dterminal_dynamic.hpp"
#include "terminal_variables.hpp"
#include "posc_adc.hpp"
#include "posc_version.h"

const comm::USBStream usb_stream{stdio_usb};
const comm::DataPlotterStream dataplotter{usb_stream};
dt::Terminal dterminal{dataplotter};

namespace sh {
dt::StaticPart dtheader{3,
                        "ELAscope"
                        "\e[1E\e[42m<\e[0m\e[4CAbout\e[3C\e[42m>\e[0m"
                        "\e[1EPinout:"
                        "\e[1E CH1 - GP26"
                        "\e[1E CH2 - GP27"
                        "\e[1E PWM - GP16"
                        "\e[2EVersion:\e[1E " PROJECT_VERSION "\e[1E " CMAKE_BUILD_TYPE "\e[1ECompiled:\e[1E " COMPILE_DATE
                        "\e[2ECreated by:\e[1E Vít Vaněček"
                        "\e[2E Czech\e[1E Technical\e[1E University\e[1E in Prague"
                        "\e[2E Faculty of\e[1E Electrical\e[1E Engineering"
                        "\e[2E Department of\e[1E Measurement"};

constexpr dt::StaticPart* dterminal_parts[]{&dtheader};

}  // namespace sh

namespace s0 {
dt::StaticPart dtheader{3,
                        "ELAscope\e[5C\e[42m?\e[0m"
                        "\e[1E\e[42m<\e[0m  Sampling  \e[42m>\e[0m"};

dt::MultiButton dtchannel{2, 1, "ab", 0, comm::ansi::btn_pressed_str_green};
dt::StaticPart dtchannel_part{4,
                              "Channel:"
                              "\e[1E\e[3CCH1"
                              "\e[1E\e[3CCH1+2",
                              &dtchannel};

dt::FloatNumber dttrigger_level{1, 1, 1, 10, 0.5f, false};
dt::StaticPart dttrigger_level_part{2,
                                    "Trigger:"
                                    "\e[1E\e[7C\e[42m-\e[3C\e[0mV\e[42m+\e[0m",
                                    &dttrigger_level};

dt::IntNumber dtpretrigger{1, 1, 11, 2, 0, false};
dt::StaticPart dtpretrigger_part{2,
                                 "Pretrig:"
                                 "\e[1E\e[7C\e[42m{\e[3C\e[0m%\e[42m}\e[0m",
                                 &dtpretrigger};

dt::MultiButton dttrigger_selector{2, 0, "12", 0, comm::ansi::btn_pressed_str_green};
dt::StaticPart dttrigger_selector_part{2,
                                       "\e[3CRising"
                                       "\e[1E\e[3CFalling",
                                       &dttrigger_selector};

dt::MultiButton dttrigger_mode_selector{2, 1, "wxyz", 0, comm::ansi::btn_pressed_str_green};
dt::Strings dttrigger_mode{&dttrigger_mode_selector, 11, 0, dttmode_auto};
dt::StaticPart dttrigger_mode_part{6,
                                   "Mode:"
                                   "\e[1E\e[3CAUTO"
                                   "\e[1E\e[3CNORMAL"
                                   "\e[1E\e[3CSINGLE"
                                   "\e[1E\e[3CPAUSE",
                                   &dttrigger_mode};

dt::MultiButton dtsamplerate_selector{2, 1, "BCDEFGHIJ", selector_samplerates_default, comm::ansi::btn_pressed_str_green};
dt::StaticPart dtsamplerate_part{10,
                                 "Samplerate:"
                                 "\e[1E\e[3C  1 kHz"
                                 "\e[1E\e[3C  2 kHz"
                                 "\e[1E\e[3C  5 kHz"
                                 "\e[1E\e[3C 10 kHz"
                                 "\e[1E\e[3C 20 kHz"
                                 "\e[1E\e[3C 50 kHz"
                                 "\e[1E\e[3C100 kHz"
                                 "\e[1E\e[3C200 kHz"
                                 "\e[1E\e[3C500 kHz",
                                 &dtsamplerate_selector};

dt::FloatNumber dtsamplerate_disp{1, 1, 4, 14 - 4, adc::get_samplerate()};
dt::StaticPart dtsamplerate_disp_part{3, "Freq (Hz):", &dtsamplerate_disp};

dt::MultiButton dtsample_buff_selector{2, 1, "KLMN", selector_sample_size_default, comm::ansi::btn_pressed_str_green};
dt::StaticPart dtsample_buff_part{6,
                                  "Samples:"
                                  "\e[1E\e[3C1024"
                                  "\e[1E\e[3C2048"
                                  "\e[1E\e[3C4096"
                                  "\e[1E\e[3C8192",
                                  &dtsample_buff_selector};

constexpr dt::StaticPart* dterminal_parts[]{&dtheader,          &dtchannel_part,          &dttrigger_level_part,
                                            &dtpretrigger_part, &dttrigger_selector_part, &dttrigger_mode_part,
                                            &dtsamplerate_part, &dtsamplerate_disp_part,  &dtsample_buff_part};

}  // namespace s0

namespace s1 {
dt::StaticPart dtheader{3,
                        "ELAscope\e[5C\e[42m?\e[0m"
                        "\e[1E\e[42m<\e[0m  ADC Freq  \e[42m>\e[0m"};

PreciseFreq precise_adc_freq{"KJIHGFED", "kjihgfed", 73942, 50000000};
dt::IntNumber dtfreq_prec1{1, 2, 14, 2, 0, false};
dt::IntNumber dtfreq_prec0{&dtfreq_prec1, 1, 2, 11, 6, 0, false};
dt::StaticPart dtfreq_prec_part{5,
                                "\e[42mA\e[0mpply  \e[42mM\e[0max \e[42mm\e[0min"
                                "\e[1E\e[4C+\e[42m\e[32mDEFGHI\e[0m \e[42m\e[32mJK\e[0m"
                                "\e[1E\e[11C."
                                "\e[1E\e[4C-\e[42m\e[32mdefghi\e[0m \e[42m\e[32mjk\e[0m",
                                &dtfreq_prec0};

dt::StaticPart dtsamplerate_disp_part{2, "Freq (Hz):", &s0::dtsamplerate_disp};

dt::IntNumber dtadcdiv1{1, 1, 10, 3, 0, false};
dt::IntNumber dtadcdiv0{&dtadcdiv1, 1, 1, 6, 5, 0, false};
dt::StaticPart dtadcdiv_part{2,
                             "ADC Div:"
                             "\e[1E 00000+000/256",
                             &dtadcdiv0};

dt::IntNumber dtadcclk{1, 1, 12, 8, 0, false};
dt::StaticPart dtadcclk_part{3, "ADC Clk:\e[1E\e[12CHz", &dtadcclk};

constexpr dt::StaticPart* dterminal_parts[]{&dtheader, &dtfreq_prec_part, &dtsamplerate_disp_part, &dtadcdiv_part, &dtadcclk_part};

}  // namespace s1

namespace s2 {
dt::StaticPart dtheader{3,
                        "ELAscope\e[5C\e[46m?\e[0m"
                        "\e[1E\e[46m<\e[0m  PWM Gen   \e[46m>\e[0m"};

dt::IntNumber dtpwm_duty_disp{1, 2, 11, 3, 0, false};
dt::MultiButton dtpwm_func_selector{&dtpwm_duty_disp, 2, 1, "wxyz", pwm_selector_modes_default, comm::ansi::btn_pressed_str_cyan};
dt::StaticPart dtpwm_func_selector_part{6,
                                        "Function:"
                                        "\e[1E\e[3COFF"
                                        "\e[1E\e[3CPWM \e[46m{\e[3C\e[0m%\e[46m}\e[0m"
                                        "\e[1E\e[3CPWM Sine"
                                        "\e[1E\e[3CPWM Tria",
                                        &dtpwm_func_selector};

PreciseFreq precise_pwm_freq{"NLKJIHGFE", "nlkjihgfe", 195360, 50000000};
dt::IntNumber dtfreq_prec1{1, 2, 14, 2, 0, false};
dt::IntNumber dtfreq_prec0{&dtfreq_prec1, 1, 2, 11, 7, 0, false};
dt::StaticPart dtfreq_prec_part{5,
                                "\e[46mA\e[0mpply  \e[46mM\e[0max \e[46mm\e[0min"
                                "\e[1E\e[3C+\e[46m\e[36mEFGHIJK\e[0m \e[46m\e[36mLN\e[0m"
                                "\e[1E\e[11C."
                                "\e[1E\e[3C-\e[46m\e[36mefghijk\e[0m \e[46m\e[36mln\e[0m",
                                &dtfreq_prec0};

dt::FloatNumber dtpwm_freq_disp{1, 1, 4, 14 - 4, 0};
dt::StaticPart dtpwm_freq_disp_part{2, "Freq (Hz):", &dtpwm_freq_disp};

dt::IntNumber dtpwmdiv1{1, 1, 11, 2, 0, false};
dt::IntNumber dtpwmdiv0{&dtpwmdiv1, 1, 1, 8, 3, 0, false};
dt::StaticPart dtadcdiv_part{2,
                             "PWM Div:"
                             "\e[1E\e[5C000+00/16",
                             &dtpwmdiv0};

dt::IntNumber dtpwmmax{1, 1, 12, 7, 0, false};
dt::StaticPart dtpwmmax_part{3, "PWM Max:\e[1E\e[12CHz", &dtpwmmax};

dt::MultiButton dtpwmmax_selector{2, 1, "0123456", pwmmax_freqs_default, comm::ansi::btn_pressed_str_cyan};
dt::StaticPart dtpwmmax_selector_part{9,
                                      "Max freq:"
                                      "\e[1E\e[3C  2 kHz"
                                      "\e[1E\e[3C 10 kHz"
                                      "\e[1E\e[3C 50 kHz"
                                      "\e[1E\e[3C100 kHz"
                                      "\e[1E\e[3C500 kHz"
                                      "\e[1E\e[3C  1 MHz"
                                      "\e[1E\e[3C  5 MHz",
                                      &dtpwmmax_selector};

constexpr dt::StaticPart* dterminal_parts[]{&dtheader,      &dtpwm_func_selector_part, &dtfreq_prec_part, &dtpwm_freq_disp_part, &dtadcdiv_part,
                                            &dtpwmmax_part, &dtpwmmax_selector_part};

}  // namespace s2

namespace s3 {
dt::StaticPart dtheader{3,
                        "ELAscope\e[5C\e[42m?\e[0m"
                        "\e[1E\e[42m<\e[0m  Settings  \e[42m>\e[0m"};

dt::DTButton div_fract_toggle{2, 0, 'a', false};
dt::StaticPart div_fract_toggle_part{1, "\e[3CFrac div", &div_fract_toggle};

dt::DTButton div_ps_toggle{2, 0, 'b', true};
dt::StaticPart div_pwr_toggle_part{1, "\e[3CPower save", &div_ps_toggle};

constexpr dt::StaticPart* dterminal_parts[]{&dtheader, &div_fract_toggle_part, &div_pwr_toggle_part};
}  // namespace s3

void init_dterminal() {
    init_dterminal_base(dterminal, sh::dterminal_parts, sh::index);
    init_dterminal_base(dterminal, s0::dterminal_parts, s0::index);
    init_dterminal_base(dterminal, s1::dterminal_parts, s1::index);
    init_dterminal_base(dterminal, s2::dterminal_parts, s2::index);
    init_dterminal_base(dterminal, s3::dterminal_parts, s3::index);
}
