# Define which platform
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUNI_PLATFORM_UNIJOYSTICLE")

set(COMPONENT_SRCS "uni_bluetooth.c"
                   "uni_circular_buffer.c"
                   "uni_gamepad.c"
                   "uni_hid_device.c"
                   "uni_hid_parser.c"
                   "uni_hid_parser_8bitdo.c"
                   "uni_hid_parser_android.c"
                   "uni_hid_parser_ds3.c"
                   "uni_hid_parser_ds4.c"
                   "uni_hid_parser_ds5.c"
                   "uni_hid_parser_generic.c"
                   "uni_hid_parser_icade.c"
                   "uni_hid_parser_nimbus.c"
                   "uni_hid_parser_ouya.c"
                   "uni_hid_parser_smarttvremote.c"
                   "uni_hid_parser_switch.c"
                   "uni_hid_parser_wii.c"
                   "uni_hid_parser_xboxone.c"
                   "uni_joystick.c"
                   "uni_main.c"
                   "uni_main_esp32.c"
                   "uni_platform.c"
                   "uni_platform_nina.c"
                   "uni_platform_pc_debug.c"
                   "uni_platform_unijoysticle.c"
                   "uni_platform_mightymiggy.c"
                   )
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()

