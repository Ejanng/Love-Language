# `src/errors.hpp`

**Role:** defines `ReneError`, the only exception type the language throws for
problems in a Rene program, including syntax errors and runtime errors.

## Contents

### `class ReneError : public std::runtime_error`

| Member | Description |
|--------|-------------|
| `ReneError(kind, line, message)` | Builds an error. `kind` is `"SyntaxError"` or `"RuntimeError"`. `line` is the line in the `.rene` file. |
| `kind()` | The error kind |
| `line()` | The line number |
| `what()` | The message (inherited from `std::runtime_error`) |
| `format()` | The full text shown to the user: `[line 4] RuntimeError: division by zero` |
| `isSyntaxError()` | `true` when `kind` is `"SyntaxError"`; `main.cpp` uses it to pick the exit code |

## Who throws it

| Thrown by | Kind | Examples |
|-----------|------|----------|
| `lexer.hpp` | `SyntaxError` | unterminated string, unexpected character `@` |
| `parser.hpp` | `SyntaxError` | missing `;`, `break` outside a loop |
| `interpreter.hpp` | `RuntimeError` | undefined variable, division by zero, index out of range |

`main.cpp` catches it in `runSource`, prints `format()` to standard error, and
returns exit code **65** for syntax errors or **70** for runtime errors.

## Not to be confused with

`ReturnSignal`, `BreakSignal` and `ContinueSignal` in `interpreter.hpp` are
also thrown as exceptions, but they are normal control flow, not errors. See
[interpreter.md](interpreter.md).

## Adding a new error kind

Pass a different `kind` string, for example `ReneError("TypeError", line, "…")`.
If it should have its own exit code, update `runSource` in `main.cpp`.

## Depends on

Only the standard library.
