# Define which platform
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUNI_PLATFORM_ARDUINO")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DUNI_PLATFORM_ARDUINO")

set(COMPONENT_SRCS "arduino_main.cpp"
                   "ArduinoBluepad32.cpp"
                   "ArduinoGamepad.cpp"
                   )
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
