include(CMakePrintHelpers)

set(srcs
         "bt/uni_bt.c"
         "bt/uni_bt_allowlist.c"
         "bt/uni_bt_conn.c"
         "bt/uni_bt_hci_cmd.c"
         "bt/uni_bt_le.c"
         "bt/uni_bt_service.c"
         "bt/uni_bt_setup.c"
         "controller/uni_balance_board.c"
         "controller/uni_controller.c"
         "controller/uni_controller_type.c"
         "controller/uni_gamepad.c"
         "controller/uni_keyboard.c"
         "controller/uni_mouse.c"
         "parser/uni_hid_parser.c"
         "parser/uni_hid_parser_8bitdo.c"
         "parser/uni_hid_parser_android.c"
         "parser/uni_hid_parser_atari.c"
         "parser/uni_hid_parser_ds3.c"
         "parser/uni_hid_parser_ds4.c"
         "parser/uni_hid_parser_ds5.c"
         "parser/uni_hid_parser_generic.c"
         "parser/uni_hid_parser_icade.c"
         "parser/uni_hid_parser_keyboard.c"
         "parser/uni_hid_parser_mouse.c"
         "parser/uni_hid_parser_nimbus.c"
         "parser/uni_hid_parser_ouya.c"
         "parser/uni_hid_parser_psmove.c"
         "parser/uni_hid_parser_smarttvremote.c"
         "parser/uni_hid_parser_stadia.c"
         "parser/uni_hid_parser_steam.c"
         "parser/uni_hid_parser_switch.c"
         "parser/uni_hid_parser_wii.c"
         "parser/uni_hid_parser_xboxone.c"
         "platform/uni_platform.c"
         "uni_circular_buffer.c"
         "uni_hid_device.c"
         "uni_init.c"
         "uni_joystick.c"
         "uni_log.c"
         "uni_property.c"
         "uni_utils.c"
         "uni_version.c"
         "uni_virtual_device.c")

 if(CONFIG_IDF_TARGET_ESP32 OR PICO_SDK_VERSION_STRING OR BLUEPAD32_TARGET_POSIX)
    # Bluetooth Classic files used by:
    # - ESP32
    # - Pico W
    # - Linux (assuming BT dongle supports BR/EDR)
    list(APPEND srcs
         # BR/EDR code only gets compiled on ESP32
         "bt/uni_bt_bredr.c"
         "bt/uni_bt_sdp.c")
endif()

if(IDF_TARGET)
    # Files that are meant to be compiled for any ESP32 target like
    # - ESP32
    # - ESP32-S3
    # - etc...
    # This files call ESP-IDF functions and should be ported/converted
    # so that can be called from other targets like Pico W
    list(APPEND srcs
         "arch/uni_console_esp32.c"
         "arch/uni_system_esp32.c"
         "arch/uni_log_esp32.c"
         "arch/uni_property_esp32.c"
         "uni_gpio.c"
         "uni_mouse_quadrature.c")
elseif(PICO_SDK_VERSION_STRING)
    list(APPEND srcs
         "arch/uni_console_pico.c"
         "arch/uni_system_pico.c"
         "arch/uni_log_pico.c"
         "arch/uni_property_pico.c")
elseif(BLUEPAD32_TARGET_POSIX)
    list(APPEND srcs
         "arch/uni_console_posix.c"
         "arch/uni_system_posix.c"
         "arch/uni_log_posix.c"
         "arch/uni_property_posix.c")
else()
    message(FATAL_ERROR "Define target")
endif()

if(CONFIG_IDF_TARGET_ESP32)
    # Files that are only meant to be compiled on ESP32 (original)
    list(APPEND srcs
        # Place here files that are ESP32-only
         "platform/uni_platform_mightymiggy.c"
         "platform/uni_platform_nina.c"
         "platform/uni_platform_unijoysticle.c"
         "platform/uni_platform_unijoysticle_2.c"
         "platform/uni_platform_unijoysticle_2plus.c"
         "platform/uni_platform_unijoysticle_800xl.c"
         "platform/uni_platform_unijoysticle_a500.c"
         "platform/uni_platform_unijoysticle_c64.c"
         "platform/uni_platform_unijoysticle_msx.c"
         "platform/uni_platform_unijoysticle_singleport.c")
endif()

#
# Disabled since it depends on having `compile_gatt.py` somewhere to generate it.
# More difficult to distribute bluepad32 as a "component".
# BLE Service
# to generate .h from .gatt files
#if (NOT DEFINED BLUEPAD32_ROOT)
#    set (BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
#    message(WARNING "BLUEPAD32_ROOT not set. Setting to ${BLUEPAD32_ROOT}")
#endif()
#
#if (NOT DEFINED BTSTACK_ROOT)
#    set (BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)
#    message(WARNING "BTSTACK_ROOT not set. Setting to ${BTSTACK_ROOT}")
#endif()
#
#if(NOT CMAKE_BUILD_EARLY_EXPANSION)
#    # CMAKE_BUILD_EARLY_EXPANSION needed in ESP32 to prevent command is not scriptable.
#    # See: https://esp32.com/viewtopic.php?t=29402
#    find_package (Python REQUIRED COMPONENTS Interpreter)
#    include_directories(${CMAKE_CURRENT_BINARY_DIR})
#
#    set(GATT_FILES
#            "uni_bt_service.gatt")
#
#    cmake_print_variables(GATT_FILES)
#    cmake_print_variables(BLUEPAD32_ROOT)
#    cmake_print_variables(BTSTACK_ROOT)
#    foreach(GATT_FILE ${GATT_FILES})
#            message("Generating GATT DB file")
#            cmake_print_variables(GATT_FILE)
#            add_custom_command(
#                    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GATT_FILE}.h
#                    DEPENDS bt/${GATT_FILE}
#                    COMMAND ${Python_EXECUTABLE}
#                    ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${CMAKE_CURRENT_SOURCE_DIR}/bt/${GATT_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${GATT_FILE}.h
#            )
#            list(APPEND srcs ${CMAKE_CURRENT_BINARY_DIR}/${GATT_FILE}.h)
#    endforeach(GATT_FILE)
#endif()

# Valid for ESP-IDF only
if(IDF_TARGET)
    set(requires "nvs_flash" "btstack" "app_update" "esp_timer")

    if("${IDF_VERSION_MAJOR}" GREATER_EQUAL 5)
        # When using ESP-IDF v5+, use "normal" components
        list(APPEND requires "cmd_nvs" "cmd_system")
    else()
        # Use legacy components when compiled using ESP-IDF v4.4
        list(APPEND requires "cmd_nvs_4.4" "cmd_system_4.4")
    endif()

    idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "include"
                        REQUIRES ${requires})
elseif(PICO_SDK_VERSION_STRING OR BLUEPAD32_TARGET_POSIX)
    # Valid for Pico W and Linux
    add_library(bluepad32 ${srcs})
    target_include_directories(bluepad32 PUBLIC ./include)
else()
    message(FATAL_ERROR "Define target")
endif()

if(IDF_TARGET)
    # ESP-IDF
    # Nothing
elseif(PICO_SDK_VERSION_STRING)
    target_link_libraries(bluepad32
            pico_stdlib
            pico_cyw43_arch_none
            pico_btstack_ble
            pico_btstack_classic
            pico_btstack_cyw43
            )
elseif(BLUEPAD32_TARGET_POSIX)
    # Valid for Linux
    # TODO: Add dependencies here
else()
    message(FATAL_ERROR "Define target")
endif()
