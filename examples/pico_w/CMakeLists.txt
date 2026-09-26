cmake_minimum_required(VERSION 3.13)

# PICO_BOARD should be set to 'pico_w', 'pico2_w' or not set at all.
if(NOT DEFINED PICO_BOARD)
    message(VERBOSE "PICO_BOARD not defined. Using 'pico_w'")
    set(PICO_BOARD "pico_w")
else()
    message(DEBUG "Using PICO_BOARD ${PICO_BOARD}")
endif()

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# If you want to use latest BTstack (included in Bluepad32 repo),
# uncomment the following line (or pass -DBTSTACK_ROOT=... to cmake):
#set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)

if(DEFINED BTSTACK_ROOT)
    message(STATUS "Using custom BTSTACK_ROOT: ${BTSTACK_ROOT}")
    set(PICO_BTSTACK_PATH ${BTSTACK_ROOT})
    # Add missing files here. See:
    # https://github.com/bluekitchen/btstack/issues/649
    # https://github.com/raspberrypi/pico-sdk/issues/2142
    if(EXISTS "${BTSTACK_ROOT}/src/hci_event_builder.c")
        set(BTSTACK_MISSING_SOURCES
            ${BTSTACK_ROOT}/src/hci_event_builder.c
        )
    endif()
else()
    # Default: use BTstack bundled in Pico SDK
    set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(bluepad32_picow_example_app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()


add_executable(${PROJECT_NAME}
    src/main.c
    src/my_platform.c
    ${BTSTACK_MISSING_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${BLUEPAD32_ROOT}/src/components/bluepad32/include)

# Useful for debugging
#target_compile_definitions(${PROJECT_NAME} PRIVATE
#        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=3000
#        WANT_HCI_DUMP=1 # This enables btstack debug
#        ENABLE_SEGGER_RTT=1
#)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(${PROJECT_NAME} src)

# Needed when using BTstack from our branch
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
# Need for certain IDEs, like CLion. Otherwise it won't find btstack
include_directories(${BTSTACK_ROOT}/src)

target_link_libraries(${PROJECT_NAME} PUBLIC
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT_NAME})
