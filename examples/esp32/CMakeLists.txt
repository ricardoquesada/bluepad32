# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)

set(EXTRA_COMPONENT_DIRS ${BLUEPAD32_ROOT}/src/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bluepad32_esp32_example_app)
