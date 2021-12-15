if(NOT CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    return()
endif()

set(srcs "ArduinoBluepad32.cpp"
         "ArduinoGamepad.cpp")

set(requires "bluepad32" "arduino")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
