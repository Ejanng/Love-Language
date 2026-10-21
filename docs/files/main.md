# `src/main.cpp`

**Role:** the program's entry point. It reads the command line, loads a
`.rene` file or starts the REPL, and passes the source through the
lexer → parser → interpreter pipeline. It is the only `.cpp` file, so it is
what CMake compiles; all other code comes in through `#include`.

## Command-line usage

| Command | Behaviour |
|---------|-----------|
| `rene` | Starts the interactive REPL |
| `rene file.rene` | Runs the file |
| `rene --help` / `-h` | Prints usage |
| `rene --version` / `-v` | Prints `Rene 0.1.0` |
| `rene a.rene b.rene` | Too many arguments: prints usage, exit code 64 |

## Functions

All helpers are in an anonymous namespace, so they are private to this file.

| Function | What it does |
|----------|--------------|
| `main(argc, argv)` | Chooses REPL, help, version or file mode based on the arguments |
| `runFile(path)` | 1. Rejects files whose extension isn't `config::FILE_EXTENSION`. 2. Reads the whole file. 3. Removes a UTF-8 byte-order mark if present (Notepad and PowerShell often add one). 4. Runs it in a fresh `Interpreter`. |
| `runRepl()` | Prints a banner, then reads lines at the `rene>` prompt until `exit` or end of input. One `Interpreter` is shared, so `let` variables persist between lines. Errors are printed and the REPL continues. |
| `runSource(interpreter, source)` | Lexes, parses and runs one piece of source. Catches `ReneError`, prints it to standard error, and returns the exit code. |
| `printUsage()` | Help text; the extension shown comes from `config.hpp` |

## Exit codes

Based on the BSD `sysexits` convention, so scripts and the test runner can
tell failures apart:

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | | Success |
| 64 | `EXIT_USAGE` | Bad arguments or wrong file extension |
| 65 | `EXIT_SYNTAX` | Syntax error in the program |
| 66 | `EXIT_NO_INPUT` | File could not be opened |
| 70 | `EXIT_RUNTIME` | Runtime error |

## Output streams

Program output (`print`) goes to **standard output**. Error messages go to
**standard error**. `std::cout` is flushed before an error is printed, so the
two appear in the right order in a terminal.

## REPL notes

- Each line must be a complete statement, including the `;`: `print 1 + 1;`
- Multi-line input (such as a function spread over several lines) isn't
  supported in the REPL. Put it on one line, or use a file.

## Depends on

`config.hpp`, `errors.hpp`, `interpreter.hpp`, `lexer.hpp`, `parser.hpp`.
