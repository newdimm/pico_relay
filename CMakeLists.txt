cmake_minimum_required(VERSION 3.13)

set(HAVE_WIRELESS 0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (HAVE_WIRELESS)
set(PICO_BOARD pico2_w CACHE STRING "Board type")
add_compile_options(-DHAVE_WIRELESS)
else()
set(PICO_BOARD pico CACHE STRING "Board type")
endif()

include(pico_sdk_import.cmake)
project(relay C CXX ASM)

pico_sdk_init()

add_executable(relay)

target_sources(relay PRIVATE
    relay.c
    network.c
    network.h
)

target_include_directories(relay PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

target_link_libraries(relay pico_stdlib)
if (HAVE_WIRELESS)
target_link_libraries(relay pico_cyw43_arch_lwip_threadsafe_background)
endif()
target_link_libraries(relay hardware_adc)

pico_enable_stdio_uart(relay 1)

pico_add_extra_outputs(relay)

