#pragma once
#include <etl/string.h>
#include <etl/type_traits.h>
#include "posc_comms.hpp"

namespace comm {

class DataPlotterStream {
   private:
    const USBStream& _usb_stream;

   public:
    DataPlotterStream(const USBStream& usb_stream) : _usb_stream{usb_stream} {
    }

    void flush() const {
        _usb_stream.flush();
    }

    template <size_t ARRAY_SIZE>
    void send(const char (&buff)[ARRAY_SIZE]) const {
        _usb_stream.send(buff, ARRAY_SIZE);
    }

    void send(const char& symbol) const {
        _usb_stream.send(symbol);
    }

    void send(const char* buff, size_t count) const {
        _usb_stream.send(buff, count);
    }

    void send_terminal(const char* data, const size_t len) const {
        send_char_cmd(_cmd_terminal, data, len, false);
    }

    template <size_t SIZE>
    void send_terminal(const char (&data)[SIZE]) const {
        send_char_cmd(_cmd_terminal, data, SIZE - 1, false);
    }

    void send_setting(const char* data, const size_t len) const {
        send_char_cmd(_cmd_settings, data, len);
    }

    template <size_t SIZE>
    void send_setting(const char (&data)[SIZE]) const {
        send_char_cmd(_cmd_settings, data, SIZE - 1);
    }

    void send_info(const char* data, const size_t len) const {
        send_char_cmd(_cmd_info, data, len, false);
    }

    template <size_t SIZE>
    void send_info(const char (&data)[SIZE]) const {
        send_char_cmd(_cmd_info, data, SIZE - 1, false);
    }

    void send_warning(const char* data, const size_t len) const {
        send_char_cmd(_cmd_warning, data, len, false);
    }

    template <size_t SIZE>
    void send_warning(const char (&data)[SIZE]) const {
        send_char_cmd(_cmd_warning, data, SIZE - 1, false);
    }

    void send_error(const char* data, const size_t len) const {
        send_char_cmd(_cmd_error, data, len);
    }

    template <size_t SIZE>
    void send_error(const char (&data)[SIZE]) const {
        send_char_cmd(_cmd_error, data, SIZE - 1);
    }

    void send_echo(const char* data, const size_t len) const {
        send_char_cmd(_cmd_echo, data, len);
    }

    template <size_t SIZE>
    void send_echo(const char (&data)[SIZE]) const {
        send_char_cmd(_cmd_echo, data, SIZE - 1);
    }

    template <typename T>
    void send_channel_data(const etl::istring& channel, const float time_step, const uint32_t length, const uint8_t useful_bits, const float min,
                           const float max, const uint32_t zero_index, const T* data) const {
        constexpr char start[]{_cmd[0], _cmd[1], _cmd_channel};
        _usb_stream.send(start, 3);
        _usb_stream.send(channel.c_str(), channel.size());
        send_channel_data_numbers(time_step, length, useful_bits, min, max, zero_index, data);
    }

    template <typename T, size_t CHANNEl_CHAR_SIZE>
    void send_channel_data(const char (&channel)[CHANNEl_CHAR_SIZE], const float time_step, const uint32_t length, const uint8_t useful_bits, const float min,
                           const float max, const uint32_t zero_index, const T* data) const {
        constexpr char start[]{_cmd[0], _cmd[1], _cmd_channel};
        _usb_stream.send(start, 3);
        _usb_stream.send(channel, CHANNEl_CHAR_SIZE - 1);
        send_channel_data_numbers(time_step, length, useful_bits, min, max, zero_index, data);
    }

    template <typename T>
    void send_channel_data_two(const etl::istring& channel, const float time_step, const uint32_t length1, const uint32_t length2, const uint8_t useful_bits,
                               const float min, const float max, const uint32_t zero_index, const T* data1, const T* data2) const {
        constexpr char start[]{_cmd[0], _cmd[1], _cmd_channel};
        _usb_stream.send(start, 3);
        _usb_stream.send(channel.c_str(), channel.size());
        send_channel_data_numbers_two(time_step, length1, length2, useful_bits, min, max, zero_index, data1, data2);
    }

    template <typename T, size_t CHANNEl_CHAR_SIZE>
    void send_channel_data_two(const char (&channel)[CHANNEl_CHAR_SIZE], const float time_step, const uint32_t length1, const uint32_t length2,
                               const uint8_t useful_bits, const float min, const float max, const uint32_t zero_index, const T* data1, const T* data2) const {
        constexpr char start[]{_cmd[0], _cmd[1], _cmd_channel};
        _usb_stream.send(start, 3);
        _usb_stream.send(channel, CHANNEl_CHAR_SIZE - 1);
        send_channel_data_numbers_two(time_step, length1, length2, useful_bits, min, max, zero_index, data1, data2);
    }

    template <typename T>
    void send_channel_data_numbers(const float& time_step, const uint32_t& length, const uint8_t& useful_bits, const float& min, const float& max,
                                   const uint32_t& zero_index, const T*& data) const {
        send_number_bin(time_step, ',');
        send_number_dec(length, ',');
        send_number_dec(useful_bits, ',');
        send_number_bin(min, ',');
        send_number_bin(max, ',');
        send_number_dec(zero_index, ';');
        send_number_array_bin(data, length, ';');
        flush();
    }

    template <typename T>
    void send_channel_data_numbers_two(const float& time_step, const uint32_t& length1, const uint32_t& length2, const uint8_t& useful_bits, const float& min,
                                       const float& max, const uint32_t& zero_index, const T*& data1, const T*& data2) const {
        send_number_bin(time_step, ',');
        send_number_dec(length1 + length2, ',');
        send_number_dec(useful_bits, ',');
        send_number_bin(min, ',');
        send_number_bin(max, ',');
        send_number_dec(zero_index, ';');
        send_number_array_bin_two(data1, data2, length1, length2, ';');
        flush();
    }

    void send_char_cmd(const char cmd, const char* data, const size_t len, bool end_semicolon = true) const {
        const char start[]{_cmd[0], _cmd[1], cmd};
        _usb_stream.send(start, 3);
        _usb_stream.send(data, len);
        if (end_semicolon) _usb_stream.send(';');
        _usb_stream.flush();
    }

    void send_number_bin_base(const char type, const uint8_t bytes_per_number, const uint8_t* bytes, const size_t array_length, const char end) const {
        const char buff[]{type, static_cast<const char>(bytes_per_number + '0')};
        _usb_stream.send(buff, sizeof(buff));
        _usb_stream.send(bytes, array_length);
        if (end > 0) {
            _usb_stream.send(end);
        }
    }

    void send_number_bin_base_two(const char type, const uint8_t bytes_per_number, const uint8_t* bytes1, const uint8_t* bytes2, const size_t array_length1,
                                  const size_t array_length2, const char end) const {
        const char buff[]{type, static_cast<const char>(bytes_per_number + '0')};
        _usb_stream.send(buff, sizeof(buff));
        _usb_stream.send(bytes1, array_length1);
        _usb_stream.send(bytes2, array_length2);
        if (end > 0) {
            _usb_stream.send(end);
        }
    }

    int send_escape_position(uint32_t line, uint32_t column) const {
        char buff[4 + 3 + 2];
        size_t index = write_escape_position(buff, line, column);
        _usb_stream.send(buff, index);
        return 0;
    }

    void send_number_dec(long number, const char end = 0) const {
        char buff[3 * sizeof(long) + 2];
        size_t str_size{0};
        char* out_str = &buff[sizeof(buff) - 1];
        bool minus{false};

        if (number < 0) {
            minus = true;
            number = -number;
        }

        if (end > 0) {
            *out_str = end;
            --out_str;
            ++str_size;
        }

        do {
            *out_str = (number % 10) + '0';
            number /= 10;
            --out_str;
            ++str_size;
        } while (number);

        if (minus) {
            *out_str = '-';
            --out_str;
            ++str_size;
        }
        _usb_stream.send(&out_str[1], str_size);
    }

    template <typename T>
    static constexpr char get_type_character() {
        if constexpr (sizeof(T) >= 8)
            return 0;
        else if constexpr (etl::is_integral_v<T> && etl::is_unsigned_v<T> && sizeof(T) <= 4)
            return 'u';
        else if constexpr (etl::is_integral_v<T> && sizeof(T) <= 4)
            return 'i';
        else if constexpr (etl::is_floating_point_v<T>)
            return 'f';
        else
            return 0;
    }

    template <typename T>
    void send_number_bin(const T num, const char end = 0) const {
        send_number_array_bin(&num, 1, end);
    }

    template <typename T>
    void send_number_array_bin(const T* array, const size_t length, const char end = 0) const {
        constexpr char type{get_type_character<T>()};
        static_assert(type, "Type not supported");
        constexpr uint8_t bytes_per_number{sizeof(T)};

        const size_t number_of_bytes{length * sizeof(T)};
        const uint8_t* ptr_u8{reinterpret_cast<const uint8_t*>(array)};
        send_number_bin_base(type, bytes_per_number, ptr_u8, number_of_bytes, end);
    }

    template <typename T>
    void send_number_array_bin_two(const T* array1, const T* array2, const size_t length1, const size_t length2, const char end = 0) const {
        constexpr char type{get_type_character<T>()};
        static_assert(type, "Type not supported");
        constexpr uint8_t bytes_per_number{sizeof(T)};

        const size_t number_of_bytes1{length1 * sizeof(T)};
        const size_t number_of_bytes2{length2 * sizeof(T)};
        const uint8_t* ptr1_u8{reinterpret_cast<const uint8_t*>(array1)};
        const uint8_t* ptr2_u8{reinterpret_cast<const uint8_t*>(array2)};
        send_number_bin_base_two(type, bytes_per_number, ptr1_u8, ptr2_u8, number_of_bytes1, number_of_bytes2, end);
    }

   private:
    static constexpr size_t max_line_width{14};
    static constexpr char _cmd[]{"$$"};
    static constexpr char _cmd_point{'P'};
    static constexpr char _cmd_channel{'C'};
    static constexpr char _cmd_logic_channel{'L'};
    static constexpr char _cmd_logic_point{'B'};
    static constexpr char _cmd_terminal{'T'};
    static constexpr char _cmd_settings{'S'};
    static constexpr char _cmd_info{'I'};
    static constexpr char _cmd_warning{'W'};
    static constexpr char _cmd_error{'X'};
    static constexpr char _cmd_echo{'E'};
    static constexpr char _cmd_unknown{'U'};
};

}  // namespace comm