cmake_minimum_required(VERSION 3.16)
project(mylang CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(mylang src/main.cpp)
target_compile_options(mylang PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME examples COMMAND ${CMAKE_SOURCE_DIR}/tests/run_tests.sh ${CMAKE_BINARY_DIR}/mylang)
