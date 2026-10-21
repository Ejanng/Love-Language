# Code Guide: every file, one by one

This guide walks through every file in the project, explains what it does, and explains why it is written that way. It assumes you know basic C but have never built a language before.

For a deeper look at the bytecode VM only, see [backend.md](backend.md).

---

## 1. The big picture

A programming language implementation turns text into behavior. Your program goes through these stages:

```
 hello.rene (text)
      │
      ▼
 ┌─────────┐   tokens    ┌─────────┐    AST     ┌───────────────────────────────┐
 │  Lexer  │ ──────────▶ │ Parser  │ ─────────▶ │ Backend (pick one):           │
 │lexer.c  │             │parser.c │            │  A) Tree-walking interpreter  │
 └─────────┘             └─────────┘            │     interpreter.c             │
                                                │  B) Compiler → bytecode → VM  │
                                                │     vm/compiler.c, vm/vm.c    │
                                                └───────────────────────────────┘
```

1. **Lexer** (scanner): turns raw characters into **tokens**, meaning words like `let`, `42`, `+` and `"hi"`.
2. **Parser**: turns the flat token list into an **AST** (abstract syntax tree), a tree that shows the structure of the program, such as "this `+` has these two operands".
3. **Backend**: actually runs the program. This project has **two** backends that must behave identically:
   - **Tree-walking interpreter**: walks the AST directly. It is simple and easy to understand.
   - **Bytecode compiler + VM**: compiles the AST into a list of small instructions, then runs them on a virtual machine (a stack machine). It is closer to how Python and Lua work.

The `--vm` flag picks the second backend. Having both lets you check one against the other. The test script runs every example on both.

### The language itself

```
let x = 10;
fn add(a, b) { return a + b; }
print add(x, 5);          // 15
if (x > 5) { print "big"; } else { print "small"; }
while (x > 0) { x = x - 1; }
```

- **Types:** numbers (all `double`), strings, booleans (`true`/`false`), `nil`, and functions.
- **Statements:** `let`, `print`, `if`/`else`, `while`, `return`, `fn`, blocks `{ }`, and expression statements. Every simple statement ends with `;`.
- **Operators:** `+ - * / %`, `== != < <= > >=`, `and`, `or`, `!`, unary `-`, and `=` for assignment.
- **Functions** are first-class values and **closures** (they remember the variables around where they were defined).
- Only `nil` and `false` are falsy. `0` and `""` count as true.
- `"a" + 1` produces `"a1"`. If either side of `+` is a string, the other side is converted to text.

### File map

| File | Stage | One-line job |
|---|---|---|
| `src/token.h` | front end | Defines the token types |
| `src/lexer.c/.h` | front end | Text → tokens |
| `src/ast.h` | front end | Defines the tree node shapes |
| `src/parser.c/.h` | front end | Tokens → AST |
| `src/arena.c/.h` | support | Fast memory allocator for the front end |
| `src/errors.c/.h` | support | Error reporting with `setjmp`/`longjmp` |
| `src/value.c/.h` | runtime | Values, strings, hash table, scopes, closures |
| `src/interpreter.c/.h` | backend A | Tree-walking interpreter |
| `src/vm/chunk.c/.h` | backend B | Bytecode instruction set + disassembler |
| `src/vm/compiler.c/.h` | backend B | AST → bytecode |
| `src/vm/vm.c/.h` | backend B | Runs bytecode |
| `src/main.c` | driver | Command line + REPL |
| `CMakeLists.txt` | build | Build config |
| `tests/run_tests.sh` | tests | Runs the examples on both backends |
| `examples/*.rene` | tests | Sample programs + `.expected` output |

---

## 2. The front end

### `src/token.h`: the vocabulary

A **token** is the smallest meaningful piece of source code. `let x = 5;` becomes: `LET`, `IDENTIFIER(x)`, `EQUAL`, `NUMBER(5)`, `SEMICOLON`.

`token.h` has two things:
- `TokenType`, an enum of every kind of token (`TOK_PLUS`, `TOK_IF`, `TOK_NUMBER`, ...), grouped into single-character, one-or-two-character (`!` vs `!=`), literals/names, and keywords.
- `Token`, a struct holding the type, the text (`lexeme`), its length, the numeric value (for numbers), and the **line number**. The line number is carried through every stage so error messages can say where the problem is.

### `src/lexer.c` / `lexer.h`: text → tokens

The public API is one function: `Token* lex(source, length, &count)`.

**How it works:** a `Lexer` struct tracks `start` (where the current token began) and `cur` (where we are now). The main loop in `lex()`:

```
while not at end:
    start = cur
    scan_token()      // reads one token, pushes it onto the list
push an EOF token
```

`scan_token()` reads one character and decides what to do:
- **Single characters** like `(`, `+`, `;` → emit that token immediately.
- **Two-character operators**: for `!`, `=`, `<`, `>` it calls `match(lx, '=')`. That checks whether the next character is `=` and consumes it if so. This is how `=` vs `==` is told apart.
- **`/`**: if the next character is also `/`, it is a comment, so skip to the end of the line. Otherwise it is division.
- **Whitespace**: skipped. A newline bumps the line counter.
- **`"`**: calls `string()`, which reads until the closing quote and handles escapes (`\n`, `\t`, `\"`, `\\`). It tracks newlines inside strings and reports "Unterminated string" if the file ends first.
- **Digits**: `number()` reads digits, then an optional `.` followed by more digits, and converts with `strtod`. It requires a digit after the `.`, so `5.` is a number followed by a dot.
- **Letters or `_`**: `identifier()` reads the whole word, then checks it against a keyword table. Match → keyword token, no match → `TOK_IDENTIFIER`.

**Design notes**
- The lexer works on the whole file at once and returns an array. The parser can then look ahead freely with no re-scanning.
- Token text is copied into the arena (see below) so it stays valid after the source buffer is freed.

### `src/ast.h`: the tree shape

The AST has two node families:

- **`Expr`**: things that produce a value: numbers, strings, variables, `a + b`, calls.
- **`Stmt`**: things that do something: `print`, `let`, `if`, `while`, blocks, function declarations.

Each node is a struct with a `kind` tag (an enum). The "kind" tells you which fields are meaningful. For example, `EXPR_BINARY` uses `op`, `left` and `right`, while `EXPR_NUMBER` only uses `number`.

The header comment is honest about the trade-off. One struct that fits all node kinds wastes some memory but is very easy to read. A tagged union would be tighter.

`Program` is just the list of top-level statements.

**Example:** `print 1 + 2 * 3;` becomes

```
STMT_PRINT
  └─ EXPR_BINARY (+)
       ├─ EXPR_NUMBER 1
       └─ EXPR_BINARY (*)
            ├─ EXPR_NUMBER 2
            └─ EXPR_NUMBER 3
```

The tree shape is what encodes "multiply first".

### `src/parser.c` / `parser.h`: tokens → AST

The public API is `Program* parse(tokens, count)`.

This is a **recursive descent parser**. The grammar is written as a comment at the top of `parser.h`, and each grammar rule is one C function. The functions call each other in the same order as the grammar.

**Operator precedence** is handled by the call chain. Lowest precedence is at the top and highest at the bottom:

```
expression → assignment → logic_or → logic_and → equality
           → comparison → term (+ -) → factor (* / %) → unary (! -) → call → primary
```

To parse `1 + 2 * 3`: `term()` calls `factor()` for its left side. `factor()` parses `1` and sees no `*`, so it returns. `term()` sees `+`, then calls `factor()` again for the right side. That call parses `2 * 3` as one unit. So `*` binds tighter, because it is parsed deeper in the chain.

**`left_assoc()`** is a helper that does the same loop for all the binary levels (`or`, `and`, `==`, `<`, `+`, `*`):

```
expr = next()
while we see one of our operators:
    op = that operator
    right = next()
    expr = Binary(expr, op, right)    // the old expr becomes the left child
```

Building left-to-right makes `10 - 3 - 2` parse as `(10 - 3) - 2`, which is **left-associative** (the correct, expected result).

**Assignment** is the exception. It is **right-associative** (`a = b = 3` means `a = (b = 3)`), so `assignment()` calls itself recursively. It parses the left side as a normal expression first, then, if it sees `=`, checks that the left side is a plain variable. If it is not (like `1 = 2`), it throws "Invalid assignment target".

**Statements:** `declaration()` checks for `fn` or `let`, otherwise falls to `statement()`, which checks for `print`/`if`/`while`/`return`/`{`, otherwise it is an expression statement.

**Helpers:** `peek` (look at the current token), `advance` (consume it), `check` (is it this type?), `match` (if it is this type, consume it and say yes), `consume` (must be this type, or throw a syntax error with a message). Almost the whole parser is built out of these five.

**Errors:** `error_at()` reports the problem and the offending token ("Expect ';' after value. (found 'print')"). It does not try to recover, so the **first** syntax error stops everything.

---

## 3. Support code

### `src/arena.c` / `arena.h`: the memory arena

**Problem:** the lexer and parser create thousands of small objects (tokens, AST nodes, strings). Freeing each one individually is tedious and easy to get wrong, especially when a syntax error aborts parsing halfway.

**Solution:** an **arena allocator**. You ask it for memory (`arena_alloc`), and you **never free individual pieces**. At exit, `arena_free_all()` releases everything at once.

**How it works:** memory comes in 64 KB blocks. Each allocation just "bumps" a pointer forward within the current block (rounded to 16 bytes for alignment). When the block is full, it allocates a new one and links it into a list. A request bigger than 16 KB gets its own dedicated block. Blocks are `calloc`'d, so arena memory always starts zeroed. That is why new AST nodes have all fields at `0`/`NULL` by default.

Other pieces:
- `arena_strndup`: copies a string into the arena.
- `arena_array_reserve`: a growable array. When full, it allocates a bigger array (double the size) and copies the old data. The old array is not freed, because the arena just keeps it until exit. That is a small waste, but a simple design.
- `PtrVec` / `ptrvec_push`: a growable list of pointers, which the parser uses for "list of statements / arguments / parameters".

**Why it matters:** it makes the front end leak-free by construction, and fast, because allocation is a pointer bump.

### `src/errors.c` / `errors.h`: errors without exceptions

C has no `try/catch`. This project uses **`setjmp`/`longjmp`** to get the same effect.

- A phase that can fail calls `setjmp(g_err.env)`. This marks "if something goes wrong, jump back here". It returns 0 normally.
- Anywhere deep in the code, `lang_error(kind, line, "message...")` fills in the global `g_err` (kind, line, message) and calls `longjmp`. Execution teleports back to the `setjmp` call, which now returns non-zero.
- The code at the `setjmp` site then calls `report_error()` and returns an exit code.

There are three kinds of error:
- `ERR_SYNTAX`: from the lexer/parser
- `ERR_COMPILE`: from the bytecode compiler (e.g. `return` at the top level)
- `ERR_RUNTIME`: while the program runs (e.g. division by zero, undefined variable)

`lang_error` is `_Noreturn`, which tells the compiler it never comes back. That is why functions like `error_at` do not need a return value afterwards.

`report_error()` prints `[line N] Syntax error: message` to stderr. It flushes stdout first so that, when both go to the same terminal, output appears in the right order.

Exit codes are defined here in an enum: 0 OK, 64 usage, 65 syntax/compile error, 70 runtime error, 74 file not found. They are shared by both backends.

**Caveat of `longjmp`:** it skips any cleanup in between. That is fine here because the front end uses the arena (nothing to clean up), but the interpreter needs to manually clean up scopes after a runtime error (see below).

---

## 4. The runtime

### `src/value.h` / `value.c`: what values look like at run time

Both backends share this.

**`Value`** is a small tagged union:

```c
typedef struct {
    ValueType type;           // NIL, NUMBER, BOOL, or OBJ
    union { double number; bool boolean; Obj* obj; } as;
} Value;
```

Numbers, booleans and nil live directly inside the `Value`. Anything bigger (strings, functions, scopes) is a **heap object**, and the `Value` just holds a pointer to it.

**Heap objects** all begin with an `Obj` header (a type tag and a `next` pointer). Because the header is first, a pointer to any object can be treated as an `Obj*` (this is C's version of inheritance). Every object is added to one global linked list, `all_objects`.

**Memory management:** objects are **never freed while the program runs**. At exit, `objects_free_all()` walks the list and frees everything. This is a deliberate simplification. A real garbage collector (mark-and-sweep) would walk that same list and free the unreachable ones. The header comment points this out as the natural next step. The trade-off is that a long-running program that creates many strings uses more and more memory.

**What's in value.c**
- **Strings** (`ObjString`): the length plus the characters, stored inline after the header (a "flexible array member"). `string_copy` and `string_concat` create them.
- **Hash table** (`Table`): string keys → Values. Uses **open addressing** with **linear probing** (if a slot is taken, try the next). The hash function is FNV-1a. Capacity is always a power of two, so `hash & (cap - 1)` replaces a slow modulo. Used for variable scopes and VM globals.
- **Scopes** (`ObjEnv`, tree-walker only): a table of variables plus a `parent` pointer. Looking up a variable checks this scope, then the parent, and so on up the chain.
- **Functions** (`ObjFunction`, tree-walker): a pointer to the function's AST node, plus the scope it was defined in. Remembering that scope is exactly what makes a **closure** work.
- **Closures** (`ObjClosure`) and **upvalues** (`ObjUpvalue`) (VM): see section 6.
- **Operations:** `is_truthy` (only nil and false are falsy), `values_equal`, and `value_to_string` (how `print` shows a value). `format_number` prints whole numbers without a decimal point (`5`, not `5.000000`).

**Scope lifetime in the tree-walker (`env_new` / `env_release` / `env_capture`)**
This is the trickiest part of `value.c`. Each function call or block creates a scope. Most scopes die when the call ends, so freeing them right away keeps memory use low (this matters for loops and recursion). But if a closure was created inside, the scope must **stay alive**.

- `env_new` makes a scope that is *private*: not yet on the global list.
- `env_release` frees a private scope when its block ends.
- `env_capture` is called when a function is created. It marks the scope, and all its parents, as *captured* and moves them onto the global list, so they are no longer freed early. `env_release` then does nothing for captured scopes.

---

## 5. Backend A: the tree-walking interpreter

### `src/interpreter.c` / `interpreter.h`

The idea: **walk the AST and do what each node says.** It has two main functions that call each other:

- `execute(stmt)`: runs a statement.
- `evaluate(expr)`: computes an expression and returns a `Value`.

Each is a big `switch` on the node's `kind`.

**Expressions (`evaluate`)**
- Literals produce a Value directly.
- `EXPR_VARIABLE`: look the name up through the scope chain. If it is not found, it is a runtime error "Undefined variable".
- `EXPR_ASSIGN`: evaluate the right side, then update the existing variable.
- `EXPR_LOGICAL` (`and`/`or`): **short-circuits**. For `a or b`, if `a` is truthy it returns `a` without ever evaluating `b`. It returns the actual value, not a boolean, so `nil or "default"` gives `"default"`.
- `eval_binary`: evaluates both sides, then does the operation. `==` works on any values. `+` handles numbers or string joining. Everything else requires numbers. Division and modulo by zero are errors.
- `eval_call`: the most involved. Steps: evaluate the callee, evaluate the arguments, check it is a function and the argument count matches, check call depth (max 1000, to give a clean "Stack overflow" error instead of crashing C), create a new scope whose **parent is the function's saved closure scope** (not the caller's scope, which gives lexical scoping), bind parameters, run the body, and pick up the return value.

**Statements (`execute`)**
- `print`: converts to string, writes it, adds a newline.
- `let`: defines the variable in the current scope (nil if there is no initializer).
- `block`: makes a new child scope.
- `if`/`while`: evaluate the condition with `is_truthy`, then run the right branch.
- `fn`: creates a function object and captures the current scope.
- `return`: stores the value in `in->returnValue` and returns `true`.

**How `return` unwinds:** `execute` returns a bool, "did a return happen?". Every loop and block checks it and stops early if true. By the time control gets back to `eval_call`, the value is waiting in `returnValue`. This avoids needing exceptions for `return`.

**Error recovery:** `interpreter_run` sets up the `setjmp`. On a runtime error, `longjmp` skips the normal `env_release` calls. To avoid leaking, the interpreter keeps a `live` stack of every active scope. After an error it releases them all and resets to the global scope. This also lets the REPL continue after an error.

**`interpreter_init`** creates the global scope and captures it so it lives until exit.

---

## 6. Backend B: bytecode compiler + VM (`src/vm/`)

Why bother with a second backend? Walking a tree means a lot of pointer chasing and recursion. A VM runs a flat array of tiny instructions in a simple loop, which is the design used by real languages (CPython, Lua, the JVM).

### `src/vm/chunk.h` / `chunk.c`: the instruction set

An `Instr` is `{ op, operand, line }`. It has an opcode, one int operand (meaning depends on the opcode), and the source line for error messages.

The VM is a **stack machine**: instructions push and pop values on a stack instead of using named registers. For example, `1 + 2` compiles to:

```
CONSTANT 1    → stack: [1]
CONSTANT 2    → stack: [1, 2]
ADD           → pops both, pushes 3 → stack: [3]
```

Opcode groups:
- **Constants/literals:** `CONSTANT`, `NIL`, `TRUE`, `FALSE`
- **Stack:** `POP`
- **Variables:** `GET/SET_LOCAL`, `GET/DEFINE/SET_GLOBAL`, `GET/SET_UPVALUE`
- **Math/compare/logic:** `ADD`, `SUBTRACT`, ..., `EQUAL`, `LESS`, ..., `NOT`, `NEGATE`
- **Control flow:** `JUMP`, `JUMP_IF_FALSE`, `JUMP_IF_TRUE` (the operand is an absolute instruction index)
- **Functions:** `CALL`, `CLOSURE`, `CLOSE_UPVALUE`, `RETURN`
- **I/O:** `PRINT`

A **`Proto`** (prototype) is one compiled function: its code array, a constants table (numbers, strings, global names), the nested functions defined inside it, and its upvalue descriptions. The top-level script is also a `Proto`, called `<script>`.

`disassemble()` in `chunk.c` prints a readable listing. You see it with `./mylang --dump file.rene`. It is the best tool for understanding what the compiler is doing.

### `src/vm/compiler.c` / `compiler.h`: AST → bytecode

The compiler walks the AST once (like the interpreter) but, instead of computing results, it **emits instructions**. The rule for expressions: **every expression leaves exactly one value on the stack.**

**Variables: locals vs globals vs upvalues.** `named_variable()` decides, for every name:
1. Is it a **local** of the current function? → `GET_LOCAL slot`. The compiler tracks locals in a list; a local's position in the list is its stack slot. This is fast, since no name lookup happens at run time.
2. Is it a local of an **enclosing function**? → it becomes an **upvalue** (captured variable).
3. Otherwise → a **global**, looked up by name at run time in a hash table.

Variables declared at the top level (scope depth 0) are globals. Variables inside any block or function are locals.

**Scopes:** `begin_scope`/`end_scope` track nesting depth. At `end_scope`, it emits `POP` for each local going out of scope, to clean the stack (or `CLOSE_UPVALUE` if a closure captured it).

**Control flow with jumps:**

```
if (cond) A else B          while (cond) body
─────────────────           ─────────────────
  <cond>                    L: <cond>
  JUMP_IF_FALSE ─┐             JUMP_IF_FALSE ─┐
  POP            │             POP            │
  <A>            │             <body>         │
  JUMP ──────┐   │             JUMP L         │
  ┌──────────┼───┘          ┌──────────────────┘
  │ POP      │              │ POP
  │ <B>      │
  └──────────┴─ end
```

`JUMP_IF_FALSE` does *not* pop the condition, so the compiler explicitly emits `POP` on both paths. Forward jumps are emitted with a dummy target and fixed up later by `patch_jump()` once the destination is known. This is called **backpatching**.

**Short-circuit `and`/`or`** reuse the same trick: evaluate the left side, `JUMP_IF_FALSE` (for `and`) or `JUMP_IF_TRUE` (for `or`) past the right side, else `POP` the left and evaluate the right. If the jump is taken, the left value stays on the stack as the result.

**Functions:** `function()` creates a fresh `FunctionState` (with its own locals, code and constants) and compiles the body into its own `Proto`. It always appends `NIL; RETURN`, so a function that falls off the end returns nil. Then it emits `CLOSURE` in the *outer* function to create the function value at run time. Slot 0 of every function's locals is reserved (the callee itself), so parameters start at slot 1.

**Recursion:** a local function name is registered *before* its body is compiled, so the body can refer to itself.

**Closures:** `resolve_upvalue` walks outward through the enclosing functions. If the variable lives in the directly enclosing function, it is marked `captured` and recorded as a local-capture. If it lives further out, an upvalue is chained through each function in between. That is how a variable several levels up still works.

### `src/vm/vm.c` / `vm.h`: running the bytecode

**State (`VM`):**
- a **value stack** shared by everything,
- a **frame stack**: one `Frame` per active function call, holding the closure, its `ip` (instruction pointer) and `base` (where its slot 0 sits on the value stack),
- a list of **open upvalues**,
- the **globals** table.

**The main loop (`execute`)** is the heart of the VM:

```
loop:
    frame = top frame
    instr = frame.code[frame.ip++]
    switch (instr.op) { ... do the thing ... }
```

Each opcode is a few lines. For example `OP_ADD` pops two values, adds them (or joins strings), and pushes the result. `OP_JUMP` just sets `frame->ip`.

**Calls:** when `CALL n` runs, the callee and its `n` arguments are already sitting on the stack, in order. The VM just pushes a new frame whose `base` points at the callee. The arguments *already are* local slots 1..n. No copying is needed, which is the cleverest trick in the design.

**Return:** pops the result, closes any upvalues pointing into this frame, cuts the stack back to `base` (which removes the callee, arguments and locals in one step), pops the frame, then pushes the result for the caller. When the last frame returns, the script is done.

**Upvalues (how closures keep variables alive).** Consider:

```
fn counter() {
    let n = 0;
    fn inc() { n = n + 1; return n; }
    return inc;
}
```

`n` lives on the stack while `counter` runs. But `inc` is returned and used after `counter` is gone, so `n`'s stack slot would be destroyed. The solution is an **upvalue** with two states:

- **Open**: the variable is still on the stack. The upvalue stores the *stack index* (not a pointer, since the stack array can be reallocated and move).
- **Closed**: the variable's scope ended. `close_upvalues` copies the value into the upvalue object itself, so it survives.

`capture_upvalue` reuses an existing open upvalue for the same slot, so two closures that capture the same variable **share** it and see each other's changes.

**Error handling:** like the interpreter, `vm_run` uses `setjmp`. On a runtime error it resets the stacks so the REPL can continue.

**Persistence:** globals live in the VM between calls, so in the REPL `let x = 1;` on one line is visible on the next.

---

## 7. The driver and build

### `src/main.c`: the entry point

- Parses command-line flags: `--vm` (use the bytecode VM), `--dump` (print bytecode, implies `--vm`), and a file path. Anything else prints the usage message and exits with code 64.
- A `Runner` struct bundles both backends. `run_source()` picks the interpreter or the VM based on the flag.
- `run_file()` reads the entire file into a buffer and runs it. **The file extension is never checked**, so `.rene` is just a convention.
- `repl()` is an interactive loop: read a line, trim it, run it. It auto-adds a `;` if you forget one, so `print 1 + 2` works. Errors are printed but the session continues. `exit` or `quit` ends it.
- Cleanup at the end: `objects_free_all()` and `arena_free_all()`.

### `CMakeLists.txt`

Defines the executable from all the `.c` files (C11, with warnings). Build with `cmake -B build && cmake --build build`, or directly with:

```bash
gcc -std=c11 -O2 -Wall -Wextra src/*.c src/vm/*.c -lm -o mylang
```

`-lm` links the math library (needed for `fmod`).

### `tests/run_tests.sh`

For each backend (tree-walker, then VM), it runs every `examples/*.rene`, captures stdout+stderr, and compares it to the matching `.expected` file. It prints PASS/FAIL per test and a total. With 6 examples × 2 backends, you see `12 passed`.

### `examples/`

| File | What it tests |
|---|---|
| `hello.rene` | Basic printing |
| `fib.rene` | Recursion |
| `logic.rene` | `and` / `or` / truthiness |
| `closures.rene` | Functions capturing variables |
| `scopes.rene` | Nested scopes, shadowing, deep closures |
| `error_runtime.rene` | A runtime error and its message |

---

## 8. Tracing one program end to end

```
let x = 2;
print x * 3;
```

1. **Lexer** → `LET IDENT(x) EQUAL NUMBER(2) SEMICOLON PRINT IDENT(x) STAR NUMBER(3) SEMICOLON EOF`
2. **Parser** → `Program[ Let(x, Number 2), Print(Binary(*, Var x, Number 3)) ]`
3a. **Tree-walker:** `execute(Let)` stores `x = 2` in the global scope. `execute(Print)` evaluates the Binary: looks up `x` (2), evaluates 3, multiplies → 6, prints `6`.
3b. **VM** (`--dump` shows this). The compiler emits:
```
CONSTANT 2         push 2
DEFINE_GLOBAL "x"  pop → globals["x"]
GET_GLOBAL "x"     push 2
CONSTANT 3         push 3
MULTIPLY           pop both, push 6
PRINT              pop and print "6"
NIL
RETURN
```

Both produce `6`.

---

## 9. Where to go next

Ideas, roughly in order of difficulty:

1. **New operators or keywords**: add a token in `token.h`, lex it in `lexer.c`, parse it in `parser.c`, then handle it in **both** `interpreter.c` and `vm/compiler.c` + `vm/vm.c`. Add an example with an `.expected` file.
2. **`for` loops**: the parser can translate them into `while`, so neither backend changes.
3. **More built-in functions** (e.g. `clock()`, `len()`).
4. **Arrays or maps**: new object types in `value.h`.
5. **A garbage collector**: walk `all_objects`, mark everything reachable from the stack, globals and open upvalues, free the rest.
6. **Saving bytecode to a file**: serialize a `Proto`, and load it back.
7. **Better error recovery** in the parser, so it can report several syntax errors at once.

**The rule for any new feature:** it must work on both backends, and the test script will tell you if they disagree.
