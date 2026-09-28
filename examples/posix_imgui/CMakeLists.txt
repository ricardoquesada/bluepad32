# ==============================================================================
# Build Configuration for Bluepad32 POSIX Dear ImGui Controller Tester
# ==============================================================================
#
# This CMakeLists.txt configures a hybrid C11 / C++17 build producing:
#   1. `btstack` (C11 static library via btstack_import.cmake using POSIX + libusb-1.0)
#   2. `bluepad32` (C11 static library from src/components/bluepad32 with custom platform)
#   3. `glfw` (System libglfw3 if installed, or GLFW 3.4 static library via FetchContent)
#   4. `imgui` (C++17 static library from external/imgui + GLFW/OpenGL3 backends)
#   5. `bluepad32_posix_imgui_example_app` (Main desktop GUI controller tester)
#   6. `test_posix_imgui` (Headless CTest unit test suite)
#
# Key Build Invariants:
#   - `include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)` MUST appear before
#     `include(btstack_import.cmake)` and `add_subdirectory(... libbluepad32)` so
#     both libraries resolve `src/btstack_config.h` and `src/sdkconfig.h`.
#   - `-Wall -Wextra -Werror` is scoped strictly to first-party targets via
#     `target_compile_options` so third-party targets (`glfw`, `imgui`) do not
#     fail on benign upstream warnings.
# ==============================================================================

cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(bluepad32_posix_imgui_example_app C CXX ASM)

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)
set(IMGUI_ROOT ${BLUEPAD32_ROOT}/external/imgui)

# Define "posix" as target platform for Bluepad32 and enable custom uni_platform
set(BLUEPAD32_TARGET_POSIX "true")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_BLUEPAD32_PLATFORM_CUSTOM")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCONFIG_BLUEPAD32_PLATFORM_CUSTOM")

# Must precede btstack_import.cmake and add_subdirectory(libbluepad32) so both
# targets resolve src/btstack_config.h and src/sdkconfig.h (where
# CONFIG_BLUEPAD32_MAX_DEVICES = 4 is defined).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

include(btstack_import.cmake)
add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)

# Find OpenGL and POSIX Threads
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# Probe for system GLFW3 quietly first; if absent on the host workstation,
# automatically fetch and build GLFW 3.4 statically via FetchContent.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW QUIET glfw3)
if(NOT GLFW_FOUND)
    find_package(glfw3 QUIET)
endif()

if(GLFW_FOUND)
    set(GLFW_LINK_TARGET ${GLFW_LIBRARIES})
    set(GLFW_INC_DIRS ${GLFW_INCLUDE_DIRS})
elseif(glfw3_FOUND)
    set(GLFW_LINK_TARGET glfw)
    set(GLFW_INC_DIRS "")
else()
    message(STATUS "System glfw3 not found; fetching GLFW 3.4 via FetchContent...")
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
    set(GLFW_LINK_TARGET glfw)
    set(GLFW_INC_DIRS "")
endif()

# Static Dear ImGui library (compiled from external/imgui + GLFW/OpenGL3 backends).
# Linking `${GLFW_LINK_TARGET}` PUBLIC propagates `<GLFW/glfw3.h>` include paths
# to `imgui_impl_glfw.cpp` regardless of whether GLFW came from PkgConfig or FetchContent.
add_library(imgui STATIC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_glfw.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_ROOT}
    ${IMGUI_ROOT}/backends
    ${GLFW_INC_DIRS}
)
target_link_libraries(imgui PUBLIC
    ${GLFW_LINK_TARGET}
    OpenGL::GL
)

# Headless automated unit test target (Layer 2 verification via CTest)
enable_testing()
add_executable(test_posix_imgui
    src/test_posix_imgui.cpp
    src/posix_imgui_platform.cpp
)
target_include_directories(test_posix_imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BLUEPAD32_ROOT}/src/components/bluepad32/include
)
target_compile_options(test_posix_imgui PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_posix_imgui PRIVATE
    bluepad32
    btstack
    imgui
    OpenGL::GL
    Threads::Threads
    m
)
add_test(NAME test_posix_imgui COMMAND test_posix_imgui)

# Main POSIX Dear ImGui Controller Tester GUI executable target
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/demo_scene.cpp
    src/posix_imgui_platform.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BLUEPAD32_ROOT}/src/components/bluepad32/include
)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
target_link_libraries(${PROJECT_NAME} PRIVATE
    bluepad32
    btstack
    imgui
    ${GLFW_LINK_TARGET}
    OpenGL::GL
    Threads::Threads
    m
)
