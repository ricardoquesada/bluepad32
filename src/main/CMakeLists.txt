set(srcs "main.c")

set(requires "bluepad32")

if(CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    list(APPEND srcs "arduino_main.cpp")
    list(APPEND requires "bluepad32_arduino" "arduino")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
