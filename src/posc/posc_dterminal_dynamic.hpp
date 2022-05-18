#pragma once
#include <stdint.h>
#include <etl/algorithm.h>
#include <etl/iterator.h>
#include <cstddef>
#include "posc_comms.hpp"

namespace dt {

class DynamicPart {
   public:
    DynamicPart(bool data_to_send = true, size_t column_pos = 1, size_t line_offset = 0) : DynamicPart(nullptr, data_to_send, column_pos, line_offset) {
    }

    DynamicPart(DynamicPart* next, bool data_to_send = true, size_t column_pos = 1, size_t line_offset = 0)
        : _data_to_send{data_to_send}, _line_offset{line_offset}, _column_pos{column_pos < 1 ? 1 : column_pos}, _next{next} {
    }

    virtual size_t write_to_buff(char* buffer, size_t line_num, size_t data_index = 0) = 0;

    bool data_to_send() const {
        return _data_to_send;
    }

    void foce_data_send() {
        _data_to_send = true;
    }

    DynamicPart* get_next() {
        return _next;
    }

   protected:
    bool _data_to_send;
    const size_t _column_pos;
    const size_t _line_offset;
    DynamicPart* const _next;
};

class DTButton : public DynamicPart {
   public:
    DTButton(size_t colum_pos, size_t line_offset, const char button_character, bool button_is_pressed = false)
        : DynamicPart(true, colum_pos, line_offset), _button_character(button_character), _button_is_pressed{button_is_pressed} {
    }

    void button_pressed() {
        _button_is_pressed = true;
        _data_to_send = true;
    }

    void button_unpressed() {
        _button_is_pressed = false;
        _data_to_send = true;
    }

    void button_toggle() {
        _button_is_pressed = !_button_is_pressed;
        _data_to_send = true;
    }

    const char& get_button_char() const {
        return _button_character;
    }

    bool is_pressed() const {
        return _button_is_pressed;
    }

    size_t write_to_buff(char* buffer, size_t line_num, size_t data_index = 0) override {
        size_t num_of_written{0};
        num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset, _column_pos);
        if (_button_is_pressed) {
            num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::btn_pressed_str_green);
        } else {
            num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::btn_unpressed_str);
        }
        num_of_written += comm::copy_to_buffer(&buffer[num_of_written], _button_character);
        num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::clear_formatting_str);
        _data_to_send = false;
        return num_of_written;
    }

   private:
    const char _button_character;
    bool _button_is_pressed;
};

class MultiButton : public DynamicPart {
   public:
    template <size_t NUMBER_OF_BUTONS, size_t PRESSED_COLOR_SIZE>
    MultiButton(size_t colum_pos, size_t line_offset, const char (&button_characters)[NUMBER_OF_BUTONS], size_t active_button,
                const char (&pressed_color)[PRESSED_COLOR_SIZE])
        : MultiButton(nullptr, colum_pos, line_offset, NUMBER_OF_BUTONS - 1, button_characters, active_button, PRESSED_COLOR_SIZE - 1, pressed_color) {
    }

    template <size_t NUMBER_OF_BUTONS, size_t PRESSED_COLOR_SIZE>
    MultiButton(DynamicPart* next, size_t colum_pos, size_t line_offset, const char (&button_characters)[NUMBER_OF_BUTONS], size_t active_button,
                const char (&pressed_color)[PRESSED_COLOR_SIZE])
        : MultiButton(next, colum_pos, line_offset, NUMBER_OF_BUTONS - 1, button_characters, active_button, PRESSED_COLOR_SIZE - 1, pressed_color) {
    }

    MultiButton(DynamicPart* next, size_t colum_pos, size_t line_offset, size_t number_of_buttons, const char* button_characters, size_t active_button,
                size_t pressed_color_size, const char* pressed_color)
        : DynamicPart(next, true, colum_pos, line_offset),
          _button_characters{button_characters},
          _number_of_buttons{number_of_buttons},
          _active_button{active_button},
          _pressed_color{pressed_color},
          _pressed_color_size{pressed_color_size} {
    }

    int button_pressed_char(char button_char) {
        auto pos = etl::find(&_button_characters[0], &_button_characters[_number_of_buttons], button_char);
        if (pos != &_button_characters[_number_of_buttons]) {
            return button_pressed(static_cast<size_t>(etl::distance(_button_characters, pos)));
        }
        return -1;
    }

    int button_pressed(size_t button_index) {
        if (button_index < _number_of_buttons) {
            _active_button = button_index;
            _data_to_send = true;
            return button_index;
        }
        return -1;
    }

    void deactivate_all_buttons() {
        if (_active_button != _number_of_buttons) {
            _active_button = _number_of_buttons;
            _data_to_send = true;
        }
    }

    size_t get_active_button() const {
        return _active_button;
    }

    size_t write_to_buff(char* buffer, size_t line_num, size_t data_index = 0) override {
        size_t num_of_written{0};

        num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset + data_index, _column_pos);
        if (data_index == _active_button) {
            num_of_written += comm::copy_to_buffer(&buffer[num_of_written], _pressed_color, _pressed_color_size);
        } else {
            num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::btn_unpressed_str);
        }
        num_of_written += comm::copy_to_buffer(&buffer[num_of_written], _button_characters[data_index]);
        num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::clear_formatting_str);

        if (data_index >= _number_of_buttons - 1) {
            _data_to_send = false;
        }
        return num_of_written;
    }

   private:
    const char* _button_characters;
    const size_t _number_of_buttons;
    size_t _active_button;
    const char* const _pressed_color;
    const size_t _pressed_color_size;
};

class FloatNumber : public DynamicPart {
   public:
    FloatNumber(DynamicPart* next, size_t colum_pos, size_t line_offset, uint8_t digits, size_t point_position = 0, float start_value = 0.0f,
                bool clear_line = true)
        : DynamicPart(next, true, colum_pos, line_offset),
          _digist{digits},
          _point_position{point_position},
          _clear_line(clear_line),
          _current_value{start_value} {
    }

    FloatNumber(size_t colum_pos, size_t line_offset, uint8_t digits, size_t point_position = 0, float start_value = 0.0f, bool clear_line = true)
        : DynamicPart(true, colum_pos, line_offset), _digist{digits}, _point_position{point_position}, _clear_line(clear_line), _current_value{start_value} {
    }

    void set_value(float value) {
        if (value != _current_value) {
            _current_value = value;
            _data_to_send = true;
        }
    }

    size_t write_to_buff(char* buffer, size_t line_num, size_t data_index = 0) override {
        size_t num_of_written{0};

        if (_point_position > 0) {
            const size_t number_of_digits{comm::get_number_of_digits(_current_value, true)};
            const size_t point_colum_pos{_point_position > number_of_digits ? _point_position - number_of_digits : 1};
            num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset, point_colum_pos);
        } else {
            num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset, _column_pos);
        }
        if (_clear_line) num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::clear_line);
        num_of_written += comm::write_number_float(&buffer[num_of_written], _current_value, _digist);
        _data_to_send = false;
        return num_of_written;
    }

   private:
    const uint8_t _digist;
    const size_t _point_position;
    const bool _clear_line;
    float _current_value;
};

class IntNumber : public DynamicPart {
   public:
    IntNumber(DynamicPart* next, size_t colum_pos, size_t line_offset, size_t last_digit_pos = 0, size_t minimum_digits = 0, long start_value = 0,
              bool clear_line = true)
        : DynamicPart(next, true, colum_pos, line_offset),
          _last_digit_pos{last_digit_pos},
          _minimum_digits{minimum_digits},
          _clear_line(clear_line),
          _current_value{start_value} {
    }

    IntNumber(size_t colum_pos, size_t line_offset, size_t last_digit_pos = 0, size_t minimum_digits = 0, long start_value = 0, bool clear_line = true)
        : DynamicPart(true, colum_pos, line_offset),
          _last_digit_pos{last_digit_pos},
          _minimum_digits{minimum_digits},
          _clear_line(clear_line),
          _current_value{start_value} {
    }

    void set_value(long value) {
        if (value != _current_value) {
            _current_value = value;
            _data_to_send = true;
        }
    }

    size_t write_to_buff(char* buffer, size_t line_num, size_t data_index = 0) override {
        size_t num_of_written{0};
        const size_t number_of_digits{comm::get_number_of_digits(_current_value, true)};

        if (_last_digit_pos > 0) {
            const size_t temp{_minimum_digits > number_of_digits ? _minimum_digits - 1 : number_of_digits - 1};
            const size_t last_digit_colum_pos{_last_digit_pos > temp ? _last_digit_pos - temp : 1};
            num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset, last_digit_colum_pos);
        } else {
            num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset, _column_pos);
        }
        if (_clear_line) num_of_written += comm::copy_to_buffer(&buffer[num_of_written], comm::ansi::clear_line);
        if (_minimum_digits > number_of_digits) {
            for (size_t i{_minimum_digits - number_of_digits}; i; --i) {
                num_of_written += comm::copy_to_buffer(&buffer[num_of_written], "0");
            }
        }
        num_of_written += comm::write_number_dec(&buffer[num_of_written], _current_value);
        _data_to_send = false;
        return num_of_written;
    }

   private:
    const size_t _last_digit_pos;
    const size_t _minimum_digits;
    const bool _clear_line;
    long _current_value;
};

class Strings : public DynamicPart {
   public:
    template <size_t LENGTH>
    Strings(size_t colum_pos, size_t line_offset, const char (&default_string)[LENGTH]) : Strings(colum_pos, line_offset, LENGTH - 1, default_string) {
    }

    Strings(size_t colum_pos, size_t line_offset, size_t default_len = 0, const char* default_string = nullptr)
        : DynamicPart(true, colum_pos, line_offset), _string{default_string}, _length{default_len} {
    }

    template <size_t LENGTH>
    Strings(DynamicPart* next, size_t colum_pos, size_t line_offset, const char (&default_string)[LENGTH])
        : Strings(next, colum_pos, line_offset, LENGTH - 1, default_string) {
    }

    Strings(DynamicPart* next, size_t colum_pos, size_t line_offset, size_t default_len = 0, const char* default_string = nullptr)
        : DynamicPart(next, true, colum_pos, line_offset), _string{default_string}, _length{default_len} {
    }

    template <size_t LENGTH>
    void set_string(const char (&string)[LENGTH]) {
        set_string(string, LENGTH - 1);
    }

    void set_string(const char* string, size_t length) {
        if (string != _string || length != _length) {
            _string = string;
            _length = length;
            _data_to_send = true;
        }
    }

    size_t write_to_buff(char* buffer, size_t line_num, size_t data_index = 0) override {
        size_t num_of_written{0};
        if (_string == nullptr || _length == 0) {
            return 0;
        }
        num_of_written += comm::write_escape_position(&buffer[num_of_written], line_num + _line_offset, _column_pos);
        num_of_written += comm::copy_to_buffer(&buffer[num_of_written], _string, _length);
        _data_to_send = false;
        return num_of_written;
    }

   private:
    const char* _string;
    size_t _length;
};

}  // namespace dt
