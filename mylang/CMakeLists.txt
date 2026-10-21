cmake_minimum_required(VERSION 3.16)
project(mylang C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(mylang
  src/main.c
  src/arena.c
  src/errors.c
  src/lexer.c
  src/parser.c
  src/value.c
  src/interpreter.c
  src/vm/chunk.c
  src/vm/compiler.c
  src/vm/vm.c)
target_link_libraries(mylang PRIVATE m)
target_compile_options(mylang PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME examples COMMAND ${CMAKE_SOURCE_DIR}/tests/run_tests.sh ${CMAKE_BINARY_DIR}/mylang)
