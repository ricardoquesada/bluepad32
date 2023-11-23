set(srcs
         "bt/uni_bt.c"
         "bt/uni_bt_allowlist.c"
         "bt/uni_bt_conn.c"
         "bt/uni_bt_hci_cmd.c"
         "bt/uni_bt_le.c"
         "bt/uni_bt_setup.c"
         "controller/uni_balance_board.c"
         "controller/uni_controller.c"
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
         "parser/uni_hid_parser_steam.c"
         "parser/uni_hid_parser_switch.c"
         "parser/uni_hid_parser_wii.c"
         "parser/uni_hid_parser_xboxone.c"
         "platform/uni_platform.c"
         "platform/uni_platform_arduino.c"
         "platform/uni_platform_arduino_bootstrap.cpp"
         "uni_circular_buffer.c"
         "uni_hid_device.c"
         "uni_init.c"
         "uni_joystick.c"
         "uni_log.c"
         "uni_property.c"
         "uni_utils.c"
         "uni_virtual_device.c")

if(CONFIG_IDF_TARGET_ESP32 OR PICO_SDK_VERSION_STRING)
    # Bluetooth Classic files used by:
    # - ESP32
    # - Pico W
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
         "uni_console.c"
         "uni_esp32.c"
         "uni_gpio.c"
         "uni_mouse_quadrature.c"
         "uni_property_nvs.c")
elseif(PICO_SDK_VERSION_STRING)
    list(APPEND srcs
         "uni_property_mem.c")
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
         "platform/uni_platform_unijoysticle_singleport.c")
endif()

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

    # See: https://gitlab.com/ricardoquesada/bluepad32/-/issues/9
    if(DEFINED ENV{BLUEPAD32_ARDUINO})
        list(APPEND requires "arduino")
    endif()

    idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "include"
                        REQUIRES ${requires})
elseif(PICO_SDK_VERSION_STRING)
    # Valid for Pico W
    add_library(bluepad32 ${srcs})
    target_include_directories(bluepad32 PUBLIC ./include)
else()
    message(FATAL_ERROR "Define target")
endif()

# See: https://gitlab.com/ricardoquesada/bluepad32/-/issues/9
if(DEFINED ENV{BLUEPAD32_ARDUINO})
    # Bluepad32 contains reference to setup() and loop().  If we add main
    # component to priv_requires then we create a large circular dependency
    # (bluepad32 -> main -> bluepad32) and can get linker errors, so
    # instead we add setup() and loop() to the undefined symbols list so the
    # linker will always include them.
    #
    # (As they are C++ symbol, we need to add the C++ mangled names.)
    list(APPEND ext_symbols "-u _Z6setupv -u _Z4loopv")
endif()

string(REPLACE ";" " " ext_symbols "${ext_symbols}")
if(IDF_TARGET)
    # ESP-IDF
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${ext_symbols})
elseif(PICO_SDK_VERSION_STRING)
    target_link_libraries(bluepad32
            pico_stdlib
            pico_cyw43_arch_none
            pico_btstack_ble
            pico_btstack_classic
            pico_btstack_cyw43
            )
else()
    message(FATAL_ERROR "Define target")
endif()
