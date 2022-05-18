#pragma once
#include <stddef.h>
#include <stdint.h>

#include "tusb.h"
#include "pico/stdio/driver.h"
#include "pico/stdio_usb.h"

namespace comm {

namespace ansi {
inline constexpr char btn_pressed_str_green[]{"\e[42m\e[32m"};
inline constexpr char btn_pressed_str_cyan[]{"\e[46m\e[36m"};
inline constexpr char btn_unpressed_str[]{"\e[47m\e[37m"};
inline constexpr char clear_formatting_str[]{"\e[0m"};
inline constexpr char clear_line[]{"\e[2K"};
}  // namespace ansi

inline size_t copy_to_buffer(char *destination, const char target) {
    destination[0] = target;
    return 1;
}

inline size_t copy_to_buffer(char *destination, const char *target, size_t num_of_characters) {
    for (size_t i{0}; i < num_of_characters; i++) {
        destination[i] = target[i];
    }
    return num_of_characters;
}

template <size_t NUMBER_OF_CHARACTERS>
inline size_t copy_to_buffer(char *destination, const char (&target)[NUMBER_OF_CHARACTERS]) {
    return copy_to_buffer(destination, target, NUMBER_OF_CHARACTERS - 1);
}

inline size_t get_number_of_digits(long number, bool count_sign = false) {
    size_t number_of_digits{0};
    if (count_sign && (number < 0)) {
        ++number_of_digits;
    }
    do {
        ++number_of_digits;
        number /= 10;
    } while (number);
    return number_of_digits;
}

inline size_t get_number_of_digits(float number, bool count_sign = false) {
    return get_number_of_digits(static_cast<long>(number), count_sign);
}

inline size_t write_number_dec(char *buff, long number, const char end = 0) {
    size_t str_size{0};
    char *out_str{buff};
    size_t number_of_digits{get_number_of_digits(number)};

    if (number < 0) {
        number = -number;
        *out_str = '-';
        ++out_str;
        ++str_size;
    }

    out_str += number_of_digits - 1;

    if (end > 0) {
        out_str[1] = end;
        ++str_size;
    }

    do {
        *out_str = (number % 10) + '0';
        number /= 10;
        --out_str;
        ++str_size;
    } while (number);

    return str_size;
}

inline size_t write_number_float(char *buff, float number, uint8_t digits, const char end = 0) {
    size_t str_size{0};
    size_t digits_written{0};
    char *out_str{buff};

    size_t n = 0;

    if (number < 0) {
        number = -number;
        *out_str = '-';
        ++out_str;
        ++str_size;
    }

    float rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i) rounding /= 10.0;

    number += rounding;

    long int_part = static_cast<long>(number);
    float remainder = number - static_cast<float>(int_part);
    digits_written = write_number_dec(out_str, int_part);
    out_str += digits_written;
    str_size += digits_written;

    if (digits > 0) {
        *out_str = '.';
        ++out_str;
        ++str_size;
    }

    while (digits-- > 0) {
        remainder *= 10.0;
        uint toPrint = static_cast<uint>(remainder);
        digits_written = write_number_dec(out_str, toPrint);
        out_str += digits_written;
        str_size += digits_written;
        remainder -= toPrint;
    }

    if (end > 0) {
        *out_str = end;
        ++str_size;
    }

    return str_size;
}

inline size_t write_escape_position(char *buff, uint32_t line, uint32_t column) {
    size_t index{2};
    buff[0] = '\e';
    buff[1] = '[';
    index += write_number_dec(&buff[index], line, ';');
    index += write_number_dec(&buff[index], column, 'H');
    return index;
}

class USBStream {
   public:
    USBStream(stdio_driver_t *usb_driver) : _usb_driver{*usb_driver} {
    }

    USBStream(const stdio_driver_t &usb_driver) : _usb_driver{usb_driver} {
    }

    void init() const {
        stdio_usb_init();
    }

    int receive_blocking() const {
        return getchar();
    }

    int receive_timeout(const uint32_t &timeout_us) const {
        return getchar_timeout_us(timeout_us);
    }

    int send(const uint8_t &byte) const {
        send(static_cast<const char>(byte));
        return 0;
    }

    int send(const char byte) const {
        send(&byte, 1);
        return 0;
    }

    int send(const uint8_t *buff, size_t count) const {
        send(reinterpret_cast<const char *>(buff), count);
        return 0;
    }

    int send(const char *buff, size_t count) const {
        _usb_driver.out_chars(buff, count);
        return 0;
    }

    bool connected() const {
        return stdio_usb_connected();
    }

    void flush() const {
        tud_cdc_write_flush();
    }

   private:
    const stdio_driver_t &_usb_driver;
};

}  // namespace comm
