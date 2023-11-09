cmake_minimum_required(VERSION 3.13)

#Defualt to pico_w
if(NOT DEFINED PICO_BOARD)
    message(FATAL_ERROR "This program is for Pico W board, please define PICO_BOARD to pico_w")
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)

project(bluepad32_picow_app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(bluepad32_picow_app
    main.c
    my_platform.c
)

target_include_directories(bluepad32_picow_app PRIVATE
    src
    ../../src/components/bluepad32/include)

# Needed for btstack_config.h / sdkconfig.h
# so that libblupad32 can include them
include_directories(bluepad32_picow_app .)

target_link_libraries(bluepad32_picow_app
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
)

add_subdirectory(../../src/components/bluepad32 libbluepad32)

pico_enable_stdio_usb(bluepad32_picow_app 1)
pico_enable_stdio_uart(bluepad32_picow_app 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(bluepad32_picow_app)
