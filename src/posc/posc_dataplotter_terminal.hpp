#pragma once
#include <etl/iterator.h>
#include <etl/string_view.h>
#include <etl/optional.h>
#include <stdint.h>
#include "hardware/structs/iobank0.h"
#include "posc_dataplotter_stream.hpp"
#include "posc_dterminal_dynamic.hpp"

namespace dt {

class Terminal;

class StaticPart {
    friend Terminal;

   public:
    template <size_t STATIC_PART_SIZE>
    constexpr StaticPart(size_t number_of_lines, const char (&static_part)[STATIC_PART_SIZE], DynamicPart* dynamic_part = nullptr)
        : StaticPart(number_of_lines, static_part, STATIC_PART_SIZE - 1, dynamic_part) {
    }

    constexpr StaticPart(size_t number_of_lines, const char* static_part, size_t static_part_size, DynamicPart* dynamic_part = nullptr)
        : _number_of_lines{number_of_lines},
          _static_part{static_part},
          _static_part_size{static_part_size},
          _next{nullptr},
          _previous{nullptr},
          _dynamic_part{dynamic_part} {
    }

    void set_static_part(etl::string_view str, etl::optional<size_t> number_of_lines = etl::nullopt) {
        _static_part = str.begin();
        _static_part_size = str.size();
        if (number_of_lines.has_value()) {
            _number_of_lines = number_of_lines.value();
        }
    }

   private:
    const char* _static_part;
    size_t _static_part_size;
    size_t _number_of_lines;
    StaticPart* _next;
    StaticPart* _previous;
    DynamicPart* _dynamic_part;
};

class Terminal {
   public:
    class iterator {
       public:
        using iterator_category = etl::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = StaticPart;
        using refrence = StaticPart&;
        using pointer = StaticPart*;

        iterator(refrence node) : _node{&node} {
        }
        iterator(pointer node) : _node{node} {
        }
        iterator(const iterator& other) : _node{other._node} {
        }

        iterator& operator++() {
            _node = _node->_next;
            return *this;
        }
        iterator operator++(int) {
            iterator temp(*this);
            _node = _node->_next;
            return temp;
        }
        iterator& operator=(const iterator& other) {
            _node = other._node;
            return *this;
        }
        refrence operator*() const {
            return *_node;
        }
        pointer operator->() {
            return _node;
        }
        friend bool operator==(const iterator& x, const iterator& y) {
            return x._node == y._node;
        }
        friend bool operator!=(const iterator& x, const iterator& y) {
            return !(x == y);
        }

       private:
        StaticPart* _node;
    };

    Terminal(const comm::DataPlotterStream& dataplotter_stream)
        : _dataplotter_stream{dataplotter_stream}, _head{nullptr, nullptr, nullptr}, _tail{nullptr, nullptr, nullptr}, _current_screen{1} {
    }

    iterator begin() {
        return iterator(_head[_current_screen]);
    }
    iterator end() {
        return iterator(nullptr);
    }

    uint8_t set_screen(uint8_t screen_num) {
        if (screen_num <= number_of_screens) {
            _current_screen = screen_num;
        }
        return _current_screen;
    }

    uint8_t next_screen() {
        if (_current_screen == help_screen) {
            _current_screen = start_screen;
        } else {
            if (_current_screen == number_of_screens) {
                _current_screen = start_screen;
            } else {
                _current_screen = _current_screen + 1;
            }
        }
        return _current_screen;
    }

    uint8_t previous_screen() {
        if (_current_screen == help_screen) {
            _current_screen = number_of_screens;
        } else {
            if (_current_screen == start_screen) {
                _current_screen = number_of_screens;
            } else {
                _current_screen = _current_screen - 1;
            }
        }
        return _current_screen;
    }

    uint8_t get_current_screen() {
        return _current_screen;
    }

    void push_terminal_part(StaticPart& terminal_part) {
        push_terminal_part(&terminal_part);
    }

    void push_terminal_part(StaticPart* terminal_part) {
        if (_head[_current_screen] == nullptr) {
            _head[_current_screen] = terminal_part;
            terminal_part->_previous = nullptr;
        } else {
            terminal_part->_previous = _tail[_current_screen];
            _tail[_current_screen]->_next = terminal_part;
        }
        terminal_part->_next = nullptr;
        _tail[_current_screen] = terminal_part;
    }

    void print_static_elements(bool clear_terminal = true) {
        if (clear_terminal) {
            _dataplotter_stream.send_terminal("\e[2J");
        } else {
            _dataplotter_stream.send_terminal("");
        }

        uint32_t linenum{1};
        size_t number_of_chars;
        for (StaticPart& dpart : *this) {
            if (dpart._static_part != nullptr && dpart._static_part_size > 0) {
                _dataplotter_stream.send_escape_position(linenum, 1);
                _dataplotter_stream.send(dpart._static_part, dpart._static_part_size);
            }
            linenum += dpart._number_of_lines;
        }
        _dataplotter_stream.flush();
    }

    void print_dynamic_elements(bool forced = false) {
        uint32_t linenum{1};
        size_t number_of_chars{0};
        bool first_change{true};
        for (StaticPart& dpart : *this) {
            DynamicPart* dynamic_part{dpart._dynamic_part};
            while (dynamic_part != nullptr) {
                if (forced) {
                    dynamic_part->foce_data_send();
                }
                if (dynamic_part->data_to_send()) {
                    if (first_change) {
                        _dataplotter_stream.send_terminal("");
                        first_change = false;
                    }

                    for (size_t i{0}; dynamic_part->data_to_send(); ++i) {
                        number_of_chars = dynamic_part->write_to_buff(tx_buffer, linenum, i);
                        _dataplotter_stream.send(tx_buffer, number_of_chars);
                    }
                }
                dynamic_part = dynamic_part->get_next();
            }
            linenum += dpart._number_of_lines;
        }
        _dataplotter_stream.flush();
    }

   public:
    static constexpr size_t tx_buffer_size{200};
    static constexpr uint8_t help_screen{0};
    static constexpr uint8_t start_screen{1};
    static constexpr uint8_t number_of_screens{4};

   private:
    char tx_buffer[tx_buffer_size];
    const comm::DataPlotterStream& _dataplotter_stream;
    uint8_t _current_screen;
    StaticPart* _head[number_of_screens + 1];
    StaticPart* _tail[number_of_screens + 1];
};

}  // namespace dt
