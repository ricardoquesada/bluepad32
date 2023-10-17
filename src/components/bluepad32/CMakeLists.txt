set(srcs "uni_balance_board.c"
         "uni_bt.c"
         "uni_bt_allowlist.c"
         "uni_bt_conn.c"
         "uni_bt_le.c"
         "uni_bt_setup.c"
         "uni_circular_buffer.c"
         "uni_console.c"
         "uni_controller.c"
         "uni_esp32.c"
         "uni_gamepad.c"
         "uni_gpio.c"
         "uni_hci_cmd.c"
         "uni_hid_device.c"
         "uni_hid_parser.c"
         "uni_hid_parser_8bitdo.c"
         "uni_hid_parser_android.c"
         "uni_hid_parser_ds3.c"
         "uni_hid_parser_ds4.c"
         "uni_hid_parser_ds5.c"
         "uni_hid_parser_generic.c"
         "uni_hid_parser_icade.c"
         "uni_hid_parser_keyboard.c"
         "uni_hid_parser_mouse.c"
         "uni_hid_parser_nimbus.c"
         "uni_hid_parser_ouya.c"
         "uni_hid_parser_psmove.c"
         "uni_hid_parser_smarttvremote.c"
         "uni_hid_parser_steam.c"
         "uni_hid_parser_switch.c"
         "uni_hid_parser_wii.c"
         "uni_hid_parser_xboxone.c"
         "uni_joystick.c"
         "uni_keyboard.c"
         "uni_log.c"
         "uni_main.c"
         "uni_mouse.c"
         "uni_mouse_quadrature.c"
         "uni_platform.c"
         "uni_platform_arduino.c"
         "uni_platform_arduino_bootstrap.cpp"
         "uni_platform_pc_debug.c"
         "uni_property.c"
         "uni_property_nvs.c"
         "uni_utils.c"
         "uni_virtual_device.c")

if(CONFIG_IDF_TARGET_ESP32)
    list(APPEND srcs
         # BR/EDR code only gets compiled on ESP32
         "uni_bt_bredr.c"
         "uni_bt_sdp.c"

        # Place here files that are ESP32-only
         "uni_platform_mightymiggy.c"
         "uni_platform_nina.c"
         "uni_platform_unijoysticle.c"
         "uni_platform_unijoysticle_2.c"
         "uni_platform_unijoysticle_2plus.c"
         "uni_platform_unijoysticle_800xl.c"
         "uni_platform_unijoysticle_a500.c"
         "uni_platform_unijoysticle_c64.c"
         "uni_platform_unijoysticle_singleport.c")
endif()

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

# adding uni_platform_custom_create to the undefined symbol list, as it
# needs to be provided by the user externally to this component
set(ext_symbols "-u uni_platform_custom_create")

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
target_link_libraries(${COMPONENT_LIB} INTERFACE ${ext_symbols})
