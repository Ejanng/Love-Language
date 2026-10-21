# File reference

This folder has one page for each file in the Rene project. Each page explains
what the file does, what it contains, how it connects to the other files, and
how to change it.

## How a program runs

When you type `rene program.rene`, the source text passes through four stages:

```
 program.rene
      │  read by main.cpp (checks the extension, strips a BOM)
      ▼
 ┌──────────┐   source text    ┌──────────┐   tokens    ┌──────────┐   syntax tree   ┌──────────────┐
 │ main.cpp │ ───────────────► │  Lexer   │ ──────────► │  Parser  │ ──────────────► │ Interpreter  │ ──► output
 └──────────┘                  └──────────┘             └──────────┘                 └──────────────┘
                               lexer.hpp                parser.hpp                   interpreter.hpp
                               uses token.hpp           builds ast.hpp nodes         uses value.hpp
```

If any stage finds a problem, it throws a `ReneError` (`errors.hpp`).
`main.cpp` catches it, prints it, and exits with an error code.

Rene has no compiler. The interpreter walks the syntax tree and runs each node
directly.

## Pages

### Source code (`src/`)

| File | Page | One-line summary |
|------|------|------------------|
| `src/config.hpp` | [config.md](config.md) | Language name, version, file extension, call-depth limit |
| `src/errors.hpp` | [errors.md](errors.md) | The single error type used for syntax and runtime errors |
| `src/token.hpp` | [token.md](token.md) | The list of token kinds and the `Token` struct |
| `src/lexer.hpp` | [lexer.md](lexer.md) | Turns source text into tokens |
| `src/value.hpp` | [value.md](value.md) | Runtime values (numbers, strings, lists, functions, …) |
| `src/ast.hpp` | [ast.md](ast.md) | Syntax-tree node definitions |
| `src/parser.hpp` | [parser.md](parser.md) | Turns tokens into a syntax tree |
| `src/interpreter.hpp` | [interpreter.md](interpreter.md) | Runs the syntax tree: scopes, functions, built-ins |
| `src/main.cpp` | [main.md](main.md) | Command-line entry point and REPL |

### Build, tests and examples

| File | Page | One-line summary |
|------|------|------------------|
| `CMakeLists.txt` | [cmakelists.md](cmakelists.md) | Build and install instructions for CMake |
| `tests/run_tests.sh` | [run_tests.md](run_tests.md) | Runs every example and compares its output |
| `examples/*.rene`, `examples/*.expected` | [examples.md](examples.md) | Sample programs and their expected output |
| `.gitignore` | [gitignore.md](gitignore.md) | Files Git should not track |

### Other documentation

- [../../README.md](../../README.md): the language guide (syntax, built-ins, building).
- [../CHANGING_FILE_EXTENSION.md](../CHANGING_FILE_EXTENSION.md): how to switch from `.rene` to another extension.

## Header dependencies

Every C++ file except `main.cpp` is a header (`.hpp`), so the whole program
is compiled from `main.cpp` as a single translation unit. The `#include` graph:

```
main.cpp
 ├── config.hpp
 ├── errors.hpp
 ├── lexer.hpp ──────── errors.hpp, token.hpp
 ├── parser.hpp ─────── ast.hpp, errors.hpp, token.hpp
 └── interpreter.hpp ── ast.hpp, config.hpp, errors.hpp, value.hpp

ast.hpp ── token.hpp, value.hpp
```

`token.hpp`, `value.hpp`, `errors.hpp` and `config.hpp` depend on no other
project file.

## Common tasks: which files to edit

| Task | Files |
|------|-------|
| Add a built-in function (e.g. `upper(s)`) | `interpreter.hpp` (`defineBuiltins`) |
| Add an operator (e.g. `**`) | `token.hpp`, `lexer.hpp`, `parser.hpp`, `interpreter.hpp` (`binary`) |
| Add a keyword / statement (e.g. `do … while`) | `token.hpp`, `lexer.hpp` (keyword table), `ast.hpp`, `parser.hpp`, `interpreter.hpp` (`execute`) |
| Add a value type (e.g. maps) | `value.hpp`, plus syntax in `ast.hpp`/`parser.hpp` and behaviour in `interpreter.hpp` |
| Change an error message | Wherever it is thrown (`lexer.hpp`, `parser.hpp` or `interpreter.hpp`) |
| Change the file extension | See [CHANGING_FILE_EXTENSION.md](../CHANGING_FILE_EXTENSION.md) |
| Add a test | New `examples/NAME.rene` + `examples/NAME.expected` |
