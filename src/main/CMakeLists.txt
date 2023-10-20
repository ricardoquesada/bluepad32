set(srcs "main.c")

set(requires "bluepad32")

# Should only be added when CONFIG_BLUEPAD32_PLATFORM_CUSTOM is selected
# But not possible. Similar to Arduino
list(APPEND srcs "my_platform.c" "uni_platform_custom.c")

# See: https://gitlab.com/ricardoquesada/bluepad32/-/issues/9
if(DEFINED ENV{BLUEPAD32_ARDUINO})
    list(APPEND srcs "sketch.cpp")
    list(APPEND requires "bluepad32_arduino" "arduino")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
