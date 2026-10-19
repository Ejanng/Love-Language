# mylang

A small programming language written in C++17, with variables, functions, closures, `if`/`while`, strings and numbers. It has two backends that share one front end: a tree-walking interpreter (the default) and a bytecode compiler plus stack-based virtual machine (`--vm`). The backend is explained in [docs/backend.md](docs/backend.md). Use it as the base for your own language: rename it, change the syntax, and swap pieces out.

## Build and run

```
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp -o mylang
./mylang examples/hello.my         # run a file on the tree-walking interpreter
./mylang --vm examples/hello.my    # run it on the bytecode VM
./mylang --dump examples/hello.my  # show the compiled bytecode, then run it on the VM
./mylang                           # start the REPL (add --vm to use the VM)
tests/run_tests.sh                 # run every example on both backends and compare with its .expected file
```

CMake also works: `cmake -B build && cmake --build build`.

## The language

```
// comments run to the end of the line
let x = 10;                      // variables
x = x + 1;
print "x is " + x;               // string + anything joins as text

fn square(n) { return n * n; }   // functions
print square(7);

if (x > 5) { print "big"; } else { print "small"; }

let i = 0;
while (i < 3) { print i; i = i + 1; }
```

Values: numbers, strings, `true`/`false`, `nil`, functions. Operators: `+ - * / %`, `< <= > >= == !=`, `and or !`. Only `nil` and `false` are falsy. Functions are closures and can be passed around and returned.

## Where each pipeline stage lives

| Stage | File | What it does |
|---|---|---|
| Tokens | `src/token.hpp` | the list of token types |
| Lexer | `src/lexer.hpp` | source text to tokens |
| AST | `src/ast.hpp` | the tree node types |
| Parser | `src/parser.hpp` | tokens to AST (recursive descent, grammar in the header comment) |
| Interpreter | `src/interpreter.hpp` | walks the AST and runs it (default backend) |
| Bytecode | `src/vm/chunk.hpp` | instruction set, compiled functions, disassembler |
| Compiler | `src/vm/compiler.hpp` | AST to bytecode, resolves names to locals, upvalues and globals |
| VM | `src/vm/vm.hpp` | runs bytecode on a value stack with call frames (`--vm` backend) |
| Entry point | `src/main.cpp` | file runner and REPL |

Errors carry a line number: `[line 3] Runtime error: Undefined variable 'missing'.`

## Good first experiments

1. Add `for` loops and `else if`, then add `break`.
2. Add built-in functions such as `clock()` and `len(s)`.
3. Add lists and indexing, which means a new `Value` type, new AST nodes and parser rules.
4. Print the AST as text and diff it against expected output in your tests.
5. Report several syntax errors at once instead of stopping at the first.
6. Resolve variables to scope depths before running, which also fixes edge cases with closures.
7. Fix the memory leak: functions stored in their own scope form a `shared_ptr` cycle. Write a mark-and-sweep garbage collector.
8. ~~Compile the AST to bytecode and run it on a stack-based VM.~~ Done: see [docs/backend.md](docs/backend.md), which also lists next steps for the VM.
