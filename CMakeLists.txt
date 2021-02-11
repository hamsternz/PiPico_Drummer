cmake_minimum_required(VERSION 3.16)

include(pico_sdk_import.cmake)

project(drummer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(drummer drummer.c)

pico_generate_pio_header(drummer ${CMAKE_CURRENT_LIST_DIR}/pio_i2s.pio)

target_link_libraries(drummer
	pico_stdlib
        hardware_dma
        hardware_irq
        hardware_pio
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(drummer)
