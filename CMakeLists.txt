cmake_minimum_required(VERSION 3.16)
project(rene LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(rene src/main.cpp)

if(MSVC)
    target_compile_options(rene PRIVATE /W4 /utf-8 /EHsc)
    # Deeply recursive Rene programs need more than the default 1 MB stack.
    target_link_options(rene PRIVATE /STACK:8388608)
else()
    target_compile_options(rene PRIVATE -Wall -Wextra -Wpedantic)
endif()

# `cmake --install build --prefix <dir>` copies the interpreter to <dir>/bin.
install(TARGETS rene RUNTIME DESTINATION bin)
