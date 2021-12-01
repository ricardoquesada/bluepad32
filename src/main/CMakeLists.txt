# Define which platform
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUNI_PLATFORM_ARDUINO")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DUNI_PLATFORM_ARDUINO")

set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_SRCS "ArduinoBluepad32.cpp"
                   "ArduinoGamepad.cpp"
                   "arduino_main.cpp"
                   "main.c"
                   )

register_component()
