# mylang

A small programming language written in C11, with variables, functions, closures, `if`/`while`, strings and numbers. It has two backends that share one front end: a tree-walking interpreter (the default) and a bytecode compiler plus stack-based virtual machine (`--vm`). Every source file is walked through in [docs/code-guide.md](docs/code-guide.md), and the backend is explained in [docs/backend.md](docs/backend.md). Use it as the base for your own language: rename it, change the syntax, and swap pieces out.

## Build and run

```
gcc -std=c11 -O2 -Wall -Wextra src/*.c src/vm/*.c -lm -o mylang
./mylang examples/hello.rene         # run a file on the tree-walking interpreter
./mylang --vm examples/hello.rene    # run it on the bytecode VM
./mylang --dump examples/hello.rene  # show the compiled bytecode, then run it on the VM
./mylang                             # start the REPL (add --vm to use the VM)
tests/run_tests.sh                   # run every example on both backends and compare with its .expected file
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
| Memory and errors | `src/arena.*`, `src/errors.*` | an arena for tokens, AST and bytecode; `setjmp`/`longjmp` error handling |
| Tokens | `src/token.h` | the list of token types |
| Lexer | `src/lexer.*` | source text to tokens |
| AST | `src/ast.h` | the tree node types |
| Parser | `src/parser.*` | tokens to AST (recursive descent, grammar in the header comment) |
| Values | `src/value.*` | tagged `Value`, heap objects (strings, scopes, functions, closures), string hash table |
| Interpreter | `src/interpreter.*` | walks the AST and runs it (default backend) |
| Bytecode | `src/vm/chunk.*` | instruction set, compiled functions, disassembler |
| Compiler | `src/vm/compiler.*` | AST to bytecode, resolves names to locals, upvalues and globals |
| VM | `src/vm/vm.*` | runs bytecode on a value stack with call frames (`--vm` backend) |
| Entry point | `src/main.c` | file runner and REPL |

Errors carry a line number: `[line 3] Runtime error: Undefined variable 'missing'.`

## Memory model

There is no garbage collector yet. The lexer, parser and compiler allocate from an arena that is released at exit. At run time, strings, closures and upvalues are heap objects linked into one global list and freed at exit, so a long-running program that builds many strings keeps growing. The tree-walker frees each call's and block's scope when it ends unless a function captured it. Adding a mark-and-sweep collector means walking that object list (see the first lines of `src/value.h`).

## Good first experiments

1. Add `for` loops and `else if`, then add `break`.
2. Add built-in functions such as `clock()` and `len(s)`.
3. Add lists and indexing, which means a new `Value` type, new AST nodes and parser rules.
4. Print the AST as text and diff it against expected output in your tests.
5. Report several syntax errors at once instead of stopping at the first.
6. Resolve variables to scope depths before running, which also fixes edge cases with closures.
7. Add a garbage collector: objects are only freed at exit today. Write a mark-and-sweep collector rooted in the stack, the globals and the open upvalues.
8. ~~Compile the AST to bytecode and run it on a stack-based VM.~~ Done: see [docs/backend.md](docs/backend.md), which also lists next steps for the VM.
