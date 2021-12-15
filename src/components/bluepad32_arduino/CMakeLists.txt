set(srcs "ArduinoBluepad32.cpp"
         "ArduinoGamepad.cpp")

set(requires "bluepad32")

if(CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    list(APPEND requires "arduino")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES "${requires}")
