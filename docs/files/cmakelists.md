# `CMakeLists.txt`

**Role:** tells CMake how to build (and install) the `rene` executable. CMake
generates the build files for whatever compiler you have: Visual Studio
projects on Windows, Makefiles or Ninja on Linux and macOS.

## What each part does

| Line(s) | Meaning |
|---------|---------|
| `cmake_minimum_required(VERSION 3.16)` | Oldest CMake version this file supports |
| `project(rene LANGUAGES CXX)` | Project name; C++ only |
| `CMAKE_CXX_STANDARD 17` (+ `REQUIRED`, `EXTENSIONS OFF`) | Compile as standard C++17 without compiler-specific extensions. Needed for `std::variant`, `std::filesystem` and `inline constexpr`. |
| `add_executable(rene src/main.cpp)` | Build a program named `rene` from `main.cpp`. The headers are pulled in by `#include`, so they don't need to be listed. |
| `if(MSVC)` options | `/W4`: high warning level. `/utf-8`: treat sources as UTF-8. `/EHsc`: standard C++ exceptions, which the interpreter relies on. |
| `/STACK:8388608` | Raises the program's stack from the Windows default of 1 MB to 8 MB, so deeply recursive Rene programs reach `MAX_CALL_DEPTH` before the C++ stack runs out |
| `else()` options | GCC/Clang warnings: `-Wall -Wextra -Wpedantic` |
| `install(TARGETS rene RUNTIME DESTINATION bin)` | `cmake --install` copies `rene.exe` into `<prefix>/bin` |

## Commands

```powershell
cmake -S . -B build                    # configure: generate build files into build/
cmake --build build --config Release   # compile → build/Release/rene.exe (MSVC)
cmake --install build --config Release --prefix "$env:LOCALAPPDATA\Programs\rene"
                                       # copy to %LOCALAPPDATA%\Programs\rene\bin
```

- `--config Release` matters with Visual Studio, which builds `Debug` by default.
  Release builds are faster.
- With Make or Ninja, the executable is `build/rene` instead.
- The `build/` folder is generated output and is ignored by Git. Deleting it is
  always safe; configure again to recreate it.

## Common changes

- **Split the code into `.cpp` files:** list them in `add_executable`, e.g.
  `add_executable(rene src/main.cpp src/interpreter.cpp)`.
- **Rename the executable:** change `rene` in `add_executable`,
  `target_compile_options`, `target_link_options` and `install`.
- **Treat warnings as errors:** add `/WX` (MSVC) or `-Werror` (GCC/Clang).
