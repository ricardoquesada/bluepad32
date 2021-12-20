
if(DEFINED ENV{BLUEPAD32_ARDUINO})

    set(srcs "ArduinoBluepad32.cpp"
            "ArduinoGamepad.cpp")

    set(requires "bluepad32" "arduino")

    idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS "include"
                        REQUIRES ${requires})

endif()
