cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

project(bluepad32_posix_example_app C ASM)

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# To use it from Bluepad32 (up-to-date, with custom patches for controllers):
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)

# Define "posic" as target "microcontroller"
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
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
	# using Intel C++
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(btstack_import.cmake)

add_executable(bluepad32_posix_example_app
		src/main.c
		src/my_platform.c
)

target_include_directories(bluepad32_posix_example_app PRIVATE
    src
    ${BLUEPAD32_ROOT}/src/components/bluepad32/include)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(bluepad32_posix_example_app src)

target_link_libraries(bluepad32_posix_example_app
    bluepad32
    btstack
    m
)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)
