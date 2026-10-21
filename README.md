# Rene

Rene is a small, dynamically typed scripting language written in C++17.
Programs are stored in **`.rene`** files and run by a **tree-walking interpreter**,
which executes the syntax tree directly. Rene has no compiler and no bytecode.

```rene
fn greet(name) {
    return "Hello, " + name + "!";
}

for name in ["Ada", "Linus", "Grace"] {
    print greet(name);
}
```

## Building

You need CMake 3.16+ and a C++17 compiler (MSVC, GCC or Clang).

```sh
cmake -S . -B build
cmake --build build --config Release
```

The executable is written to `build/` (with MSVC, `build/Release/`). Run it
from there with `.\build\Release\rene.exe`, or install it so `rene` works in
any terminal:

```powershell
cmake --install build --config Release --prefix "$env:LOCALAPPDATA\Programs\rene"
```

Then add `%LOCALAPPDATA%\Programs\rene\bin` to your user `Path` and open a new
terminal. After each rebuild, run the install command again so the installed
copy is up to date.

## Running

```sh
rene examples/hello.rene   # run a program
rene                       # start the interactive REPL (type 'exit' to quit)
rene --help
rene --version
```

Files must end in `.rene`. To change the extension, see
[docs/CHANGING_FILE_EXTENSION.md](docs/CHANGING_FILE_EXTENSION.md).

## Language tour

### Values

| Type       | Examples                    |
|------------|-----------------------------|
| `number`   | `42`, `3.14` (all numbers are doubles) |
| `string`   | `"hi"`, `"line\n"` (escapes: `\n \t \" \\`) |
| `bool`     | `true`, `false`             |
| `nil`      | `nil`                       |
| `list`     | `[1, "two", [3]]`           |
| `function` | `fn (x) { return x * 2; }`  |

Only `nil` and `false` are falsy. Everything else, including `0` and `""`, is truthy.

### Variables

```rene
let count = 0;      // declare
count = count + 1;  // assign (the variable must be declared first)
let empty;          // nil
```

Blocks `{ ... }` create a new scope.

### Operators

| Precedence (low → high) | Operators                |
|-------------------------|--------------------------|
| assignment              | `=`                      |
| logical                 | `or`, `and`              |
| equality                | `==`, `!=`               |
| comparison              | `<`, `<=`, `>`, `>=`     |
| additive                | `+`, `-`                 |
| multiplicative          | `*`, `/`, `%`            |
| unary                   | `-`, `not`               |
| call / index            | `f(x)`, `list[i]`        |

`+` adds numbers, concatenates strings (if either side is a string), and joins lists.
`and`/`or` short-circuit and return one of their operands.

### Control flow

```rene
if score >= 90 {
    print "A";
} else if score >= 80 {
    print "B";
} else {
    print "C";
}

while n > 0 { n = n - 1; }

for item in [1, 2, 3] { print item; }   // lists
for ch in "abc" { print ch; }           // strings, one character at a time
for i in range(5) { print i; }          // 0..4
```

`break` and `continue` work in both loop kinds.

### Functions

```rene
fn add(a, b) { return a + b; }

let square = fn (x) { return x * x; };   // anonymous function

fn makeCounter() {                       // closures capture their scope
    let n = 0;
    return fn () { n = n + 1; return n; };
}
```

A function without a `return` returns `nil`.

### Lists

```rene
let xs = [10, 20, 30];
print xs[0], xs[-1];   // 10 30 (negative indexes count from the end)
xs[1] = 99;
push(xs, 40);
print len(xs);         // 4
```

### Output and comments

```rene
print "a", 1, true;   // prints values separated by spaces: a 1 true
// this is a comment
```

### Built-in functions

| Function            | Description                                       |
|---------------------|---------------------------------------------------|
| `len(x)`            | Length of a list or string                        |
| `push(list, value)` | Appends to a list and returns the list            |
| `pop(list)`         | Removes and returns the last element              |
| `range(n)` / `range(a, b)` | List of numbers `0..n-1` / `a..b-1`        |
| `str(x)`            | Converts any value to a string                    |
| `num(s)`            | Converts a string to a number                     |
| `type(x)`           | Type name: `"number"`, `"string"`, `"list"`, ...  |
| `input(prompt?)`    | Reads a line from standard input                  |
| `clock()`           | Seconds elapsed, for timing                       |

### Errors

Errors report the line number and stop the program:

```
[line 4] RuntimeError: division by zero
[line 1] SyntaxError: expected ';' after expression (at end of file)
```

| Exit code | Meaning                  |
|-----------|--------------------------|
| 0         | Success                  |
| 64        | Bad usage or wrong file extension |
| 65        | Syntax error             |
| 66        | File cannot be opened    |
| 70        | Runtime error            |

## Project layout

```
src/
  config.hpp       language name, version, file extension
  token.hpp        token types
  lexer.hpp        source text → tokens
  ast.hpp          syntax-tree node definitions
  parser.hpp       tokens → syntax tree (recursive descent)
  value.hpp        runtime values
  interpreter.hpp  environments, built-ins, tree-walking evaluator
  errors.hpp       error type
  main.cpp         command-line entry point and REPL
examples/          sample .rene programs with expected output
tests/run_tests.sh runs every example and compares its output
docs/              extra documentation
```

Each file is documented in detail in [docs/files/](docs/files/README.md):
what it does, what it contains, and how to change it.

## Tests

```sh
tests/run_tests.sh             # uses build/rene
tests/run_tests.sh path/to/rene
```
