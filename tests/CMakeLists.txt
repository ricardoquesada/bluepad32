cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

project(bluepad32_test C ASM)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# To use it from Bluepad32 (up-to-date, with custom patches for controllers):
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)

# Define "posix" as target "microcontroller"
set(BLUEPAD32_TARGET_POSIX "true")

# Define "Custom" as target platform
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_BLUEPAD32_PLATFORM_CUSTOM")

# extra compiler warnings
if ("${CMAKE_C_COMPILER_ID}" MATCHES ".*Clang.*")
	# using Clang
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-variable -Wswitch-default -Werror")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
	# using GCC
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-but-set-variable -Wunused-variable -Wswitch-default -Werror")
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(btstack_import.cmake)

add_executable(test_uni_hid_device
		test_uni_hid_device.c
)

target_include_directories(test_uni_hid_device PRIVATE
    .
    ${BLUEPAD32_ROOT}/src/components/bluepad32/include)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(test_uni_hid_device .)

target_link_libraries(test_uni_hid_device
    bluepad32
    btstack
    m
)

add_executable(test_uni_hid_parser
		test_uni_hid_parser.c
)

target_include_directories(test_uni_hid_parser PRIVATE
    .
    ${BLUEPAD32_ROOT}/src/components/bluepad32/include)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(test_uni_hid_parser .)

target_link_libraries(test_uni_hid_parser
    bluepad32
    btstack
    m
)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)
