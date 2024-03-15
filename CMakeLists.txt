cmake_minimum_required(VERSION 3.16)

# set(CMAKE_VERBOSE_MAKEFILE 1)

include(lib/pico-sdk/pico_sdk_init.cmake)

project(elascope VERSION 0.7.1)



set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

pico_sdk_init()

set(VERSION_SUFFIX "")
string(TIMESTAMP COMPILE_DATE "%d.%m.%Y")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/posc_version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/posc_version.h @ONLY)

add_subdirectory(lib/etl)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/terminal_variables.cpp
    src/core1_main.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    src
    src/posc
)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--print-memory-usage")

target_link_libraries(${PROJECT_NAME}
    etl
    pico_stdlib
    pico_stdio_usb
    pico_multicore
    hardware_pwm
    hardware_adc
    hardware_dma
)

pico_add_extra_outputs(${PROJECT_NAME})

add_custom_command(
    TARGET ${PROJECT_NAME}
    COMMAND "arm-none-eabi-objdump"
    ARGS "-S" "${PROJECT_NAME}.elf" ">" "${PROJECT_NAME}.lst"
)
