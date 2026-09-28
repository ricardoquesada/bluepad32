cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

project(bluepad32_test C ASM)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# To use it from Bluepad32 (up-to-date, with custom patches for controllers):
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)

# Define "posix" as target "microcontroller"
set(BLUEPAD32_TARGET_POSIX "true")

# Define "Custom" as target platform
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_BLUEPAD32_PLATFORM_CUSTOM")

option(ENABLE_SANITIZERS "Enable ASan and UBSan" ON)
option(ENABLE_ASAN_ON_APPLE "Enable AddressSanitizer on macOS (disabled by default due to macOS 26 dyld ASan init deadlock)" OFF)
if (ENABLE_SANITIZERS AND ("${CMAKE_C_COMPILER_ID}" MATCHES ".*Clang.*" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU"))
    if (APPLE AND NOT ENABLE_ASAN_ON_APPLE)
        set(SANITIZER_FLAGS "-fsanitize=undefined -fno-sanitize=alignment -fno-omit-frame-pointer -fno-sanitize-recover=all -g -O1")
    else()
        set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -fno-sanitize-recover=all -g -O1")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

# extra compiler warnings
if ("${CMAKE_C_COMPILER_ID}" MATCHES ".*Clang.*")
	# using Clang
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-variable -Wswitch-default -Wno-deprecated-declarations -Werror")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
	# using GCC
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-but-set-variable -Wunused-variable -Wswitch-default -Werror")
endif()

add_compile_definitions(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(btstack_import.cmake)

enable_testing()

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(.)

macro(register_bluepad32_test target_name source_file)
    add_executable(${target_name}
        ${source_file}
    )

    target_include_directories(${target_name} PRIVATE
        .
        ${BLUEPAD32_ROOT}/src/components/bluepad32/include)

    target_link_libraries(${target_name}
        bluepad32
        btstack
        m
    )

    add_test(NAME ${target_name} COMMAND ${target_name})
endmacro()

register_bluepad32_test(test_uni_hid_device test_uni_hid_device.c)
register_bluepad32_test(test_uni_hid_parser test_uni_hid_parser.c)
register_bluepad32_test(test_uni_hid_parser_synthetic test_uni_hid_parser_synthetic.c)
register_bluepad32_test(test_uni_bt_packet_handlers test_uni_bt_packet_handlers.c)
register_bluepad32_test(test_uni_property_and_bb test_uni_property_and_bb.c)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)
