set(srcs "main.c")

set(requires "bluepad32"
             "bluepad32_arduino")

if(CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    list(APPEND srcs "arduino_main.cpp")
    list(APPEND requires "arduino")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
