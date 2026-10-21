# The mylang backend: bytecode compiler and virtual machine

This document explains the backend of mylang: what it does, how each part works, and how to extend it. It assumes you have read the [README](../README.md) and know the language's syntax. For a tour of every source file, including the front end and the tree-walker, see the [code guide](code-guide.md).

## 1. What the backend is

A language implementation is a pipeline. The **front end** reads text and works out its structure, and the **backend** executes that structure.

```
                 front end (shared)                          backend (pick one)
 source ──► Lexer ──► tokens ──► Parser ──► AST ──┬──► Interpreter               (tree-walker, default)
 (.rene)  lexer.c             parser.c    ast.h   │    interpreter.c
                                                  │
                                                  └──► Compiler ──► bytecode ──► VM   (--vm)
                                                       vm/compiler.c  vm/chunk.c  vm/vm.c
```

mylang has two backends that run the same programs and produce the same output:

| Backend | Flag | How it runs a program | Strength |
|---|---|---|---|
| Tree-walking interpreter | *(default)* | Visits AST nodes recursively and evaluates each one | Short, easy to change, the reference for correct behavior |
| **Bytecode VM** | `--vm` | Compiles the AST once into a flat list of simple instructions, then runs them in a loop | Faster (about 2× on the benchmark below) |

The bytecode VM is the new part. It follows the design of `clox` from *Crafting Interpreters*, Part III, and Section 7 ("Strategy 2: a stack-based bytecode VM") of the project guide.

## 2. Using it

```bash
gcc -std=c11 -O2 -Wall -Wextra src/*.c src/vm/*.c -lm -o mylang

./mylang program.rene          # tree-walking interpreter
./mylang --vm program.rene     # bytecode VM
./mylang --dump program.rene   # print the compiled bytecode, then run it on the VM
./mylang --vm                  # REPL on the VM (globals persist between lines)
tests/run_tests.sh             # run every example on both backends
```

Exit codes are the same for both backends: `0` success, `65` syntax or compile error, `70` runtime error, `74` file not found, `64` bad command-line usage.

## 3. The files

| File | Role | Main types |
|---|---|---|
| [src/vm/chunk.h](../src/vm/chunk.h), [chunk.c](../src/vm/chunk.c) | The instruction set and the compiled form of a function. Also the disassembler behind `--dump`. | `Op`, `Instr`, `Proto`, `UpvalueRef` |
| [src/vm/compiler.h](../src/vm/compiler.h), [compiler.c](../src/vm/compiler.c) | Walks the AST once and emits bytecode. Resolves every variable name to a local, upvalue or global. | `compile`, `FunctionState` |
| [src/vm/vm.h](../src/vm/vm.h), [vm.c](../src/vm/vm.c) | Runs bytecode on a value stack with call frames. Owns the globals table. | `VM`, `Frame` |
| [src/value.h](../src/value.h), [value.c](../src/value.c) | The `Value` type and heap objects, shared with the tree-walker. The VM adds `ObjClosure` and `ObjUpvalue`. | `Value`, `ObjClosure`, `ObjUpvalue` |

## 4. Values

```c
typedef struct {
    ValueType type;  // VAL_NIL, VAL_NUMBER, VAL_BOOL or VAL_OBJ
    union { double number; bool boolean; Obj* obj; } as;  // an Obj is a string, or for the VM a closure
} Value;
```

Truthiness, equality, `+` on strings, and number printing follow exactly the same rules as the tree-walker. Only `nil` and `false` are falsy, and `"x" + 1` gives `"x1"`.

## 5. The instruction set

Every instruction is an `Instr { Op op; int operand; int line; }`. The `line` is kept so runtime errors can say where they happened. The VM is a **stack machine**: instructions take their inputs from the top of a value stack and push their results back onto it.

| Opcode | Operand | Stack effect | Meaning |
|---|---|---|---|
| `Constant` | constant index | → v | Push a number or string from the constant pool |
| `Nil` `True` `False` | – | → v | Push a literal |
| `Pop` | – | v → | Discard the top value |
| `GetLocal` / `SetLocal` | slot | → v / v → v | Read or write a local variable in the current frame |
| `GetGlobal` / `SetGlobal` | name constant | → v / v → v | Read or write a global by name (error if it doesn't exist) |
| `DefineGlobal` | name constant | v → | Create or overwrite a global |
| `GetUpvalue` / `SetUpvalue` | upvalue index | → v / v → v | Read or write a variable captured by the running closure |
| `Equal` `NotEqual` `Greater` `GreaterEqual` `Less` `LessEqual` | – | a b → bool | Comparisons |
| `Add` `Subtract` `Multiply` `Divide` `Modulo` | – | a b → r | Arithmetic (`Add` also joins strings) |
| `Not` `Negate` | – | a → r | `!a` and `-a` |
| `Print` | – | v → | Print a value and a newline |
| `Jump` | target | – | Continue at instruction `target` |
| `JumpIfFalse` / `JumpIfTrue` | target | v → v | Jump if the top value is falsy or truthy. **Does not pop**, so `and`/`or` can keep it as their result. |
| `Call` | argument count | f args… → result | Call the function sitting under the arguments |
| `Closure` | function index | → closure | Create a function value and capture its upvalues |
| `CloseUpvalue` | – | v → | The local on top is going out of scope and a closure captured it: move it to the heap, then pop it |
| `Return` | – | v → | Leave the current function and hand `v` to the caller |

Set instructions leave the value on the stack because assignment is an expression (`a = b = 3`). An expression statement is followed by a `Pop`.

## 6. The compiler

`compile` takes the parsed program and returns a `Proto` named `<script>`. A `Proto` is a compiled function: its code, its constants, its arity, the functions declared directly inside it, and the list of variables it captures.

### 6.1 Every expression leaves exactly one value

This is the rule that makes code generation a simple post-order walk. For example, `a + b * 2` becomes:

```
GetGlobal  a
GetGlobal  b
Constant   2
Multiply
Add
```

### 6.2 Name resolution happens at compile time

When the compiler meets a name, it decides once, before the program runs, where that name lives:

1. **Local.** The name was declared in a block of the current function. It compiles to `GetLocal slot`. A slot is a fixed index into the function's part of the stack, so no lookup by name is needed at run time. This is one of the main reasons the VM is fast.
2. **Upvalue.** The name is a local of an *enclosing* function. It compiles to `GetUpvalue index` (see 6.5).
3. **Global.** Anything else. It compiles to `GetGlobal "name"` and is looked up in a hash table at run time. Globals are *late-bound*: a function can refer to a global that is defined later in the file, which is how top-level recursion and mutual recursion work.

Top-level `let` and `fn` create globals. Any declaration inside `{ }` or inside a function creates a local.

The compiler tracks locals in a list (`FunctionState.locals`) that mirrors the runtime stack. Slot 0 of every function is reserved for the function being called. Parameters take slots 1..arity, and each `let` takes the next slot. When a block ends, `end_scope` emits one `Pop` per local declared in it, or a `CloseUpvalue` if a closure captured that local.

### 6.3 Control flow and backpatching

Jump targets are absolute instruction indices. A forward jump is emitted before its target is known, with a placeholder operand, and patched once the target is reached (`patch_jump`). This is called *backpatching*.

```
if (cond) A else B             while (cond) body
──────────────────             ─────────────────
    <cond>                     top: <cond>
    JumpIfFalse else                JumpIfFalse exit
    Pop                             Pop
    <A>                             <body>
    Jump end                        Jump top
else: Pop                      exit: Pop
    <B>
end:
```

There's a `Pop` on both paths because `JumpIfFalse` leaves the condition on the stack.

`a and b` and `a or b` short-circuit the same way: evaluate `a`, and if it already decides the answer (`JumpIfFalse` for `and`, `JumpIfTrue` for `or`), jump past `b` and keep `a` as the result. Otherwise pop it and evaluate `b`.

### 6.4 Functions

A `fn` declaration is compiled into a new `Proto` with its own `FunctionState`. The enclosing function then gets a `Closure` instruction that creates the function value at run time. Every function ends with `Nil; Return`, so falling off the end returns `nil`. A local function's name is declared *before* its body is compiled, so it can call itself.

### 6.5 Closures and upvalues

When `resolve_upvalue` finds a name in an enclosing function, it does three things:

- It marks that local as `captured`, so the enclosing scope will emit `CloseUpvalue` instead of `Pop` when it ends.
- It records an `UpvalueRef { isLocal, index }` in the inner function's `Proto`: "capture slot `index` of my parent" or "reuse my parent's upvalue `index`".
- If the variable is several functions away, each function in between gets its own upvalue. This builds a chain, so a closure can reach a variable that the middle function never mentions. `examples/scopes.rene` tests this.

## 7. The virtual machine

### 7.1 State

```c
Value* stack;             // one value stack shared by every call
Frame* frames;            // one Frame per active call
ObjUpvalue** openUpvalues; // captured variables that are still on the stack
Table globals;            // global name -> value
```

Each array is a pointer plus a count and a capacity, grown with `realloc`. A `Frame` holds the running `ObjClosure`, its instruction pointer `ip`, and `base`, the stack index of its slot 0. `GetLocal n` reads `stack[base + n]`.

### 7.2 The dispatch loop

`execute` in `vm.c` is a `for (;;)` loop around one `switch`. Each iteration fetches `code[ip++]` from the current frame and runs it. That's the whole engine. Compared with the tree-walker, there are no recursive C calls per AST node, no scope object allocated per block, and no hash-table lookup per local variable.

### 7.3 Calls and returns

Before `Call n` runs, the stack holds the callee and then its `n` arguments:

```
           base
            ↓
stack: … | fib | 30 |
          slot0 slot1
```

`call_value` checks that the callee is a function and the argument count matches, then pushes a frame whose `base` points at the callee. The arguments are now the callee's parameters, and nothing is copied. `Return` pops the result, closes any upvalues that point into the frame, truncates the stack back to `base`, removes the frame, and pushes the result for the caller. When the `<script>` frame returns, the program is finished.

The call depth is capped at 1000, the same as the tree-walker. Deeper recursion reports `Stack overflow: too many nested calls.`

### 7.4 Upvalues: open and closed

A closure can outlive the function call that created the variables it uses:

```
fn makeCounter() {
  let count = 0;
  fn next() { count = count + 1; return count; }
  return next;       // makeCounter's stack slots vanish, but next still needs `count`
}
```

The VM handles this with `ObjUpvalue` objects:

- **Open.** While `makeCounter` is running, `count` lives on the stack. The upvalue stores its stack index (`slot`), so reads and writes go straight to the stack. If two closures capture the same variable, `capture_upvalue` gives them the *same* `ObjUpvalue`, so they see each other's changes.
- **Closed.** When the variable's scope ends (`CloseUpvalue` at the end of a block, or `Return` at the end of a function), `close_upvalues` copies the value into `ObjUpvalue.closed` and marks it closed. From then on the closure reads and writes the heap copy.

The upvalue stores a stack *index*, not a pointer, because the stack array may move when `realloc` grows it.

## 8. Reading `--dump` output

Here is `examples/closures.rene` compiled (real output):

```
== makeCounter (arity 0, upvalues 0) ==
0000  line   3  Constant     0  ; 0
0001  line   4  Closure      0  ; <fn next>  local 1
0002  line   8  GetLocal     2
0003  line   8  Return
0004  line   2  Nil
0005  line   2  Return

== next (arity 0, upvalues 1) ==
0000  line   5  GetUpvalue   0
0001  line   5  Constant     0  ; 1
0002  line   5  Add
0003  line   5  SetUpvalue   0
0004  line   5  Pop
0005  line   6  GetUpvalue   0
0006  line   6  Return
0007  line   4  Nil
0008  line   4  Return
```

Each line shows the instruction index, the source line, the opcode, and the operand, with a comment after `;` where it helps. Read it like this:

- `Constant 0` pushes `0`. Because the next statement starts a declaration, that value simply *stays* in slot 1 and becomes the local `count`. No store instruction is needed.
- `Closure 0 ; <fn next> local 1` creates `next` and tells it to capture slot 1 (`count`). The new closure lands in slot 2, which is the local `next`.
- `GetLocal 2; Return` is `return next;`.
- Inside `next`, `count` is `GetUpvalue 0`. The `Pop` after `SetUpvalue` discards the result of the assignment expression statement.
- `Nil; Return` at the end is the implicit `return nil;` and isn't reached here.

And the recursive call in `fib` (from `examples/fib.rene`):

```
0009  line   4  GetGlobal    1  ; "fib"     callee
0010  line   4  GetLocal     1              n
0011  line   4  Constant     2  ; 1
0012  line   4  Subtract                    argument n - 1
0013  line   4  Call         1              fib(n - 1)
...
0019  line   4  Add
0020  line   4  Return
```

## 9. Errors

Errors are reported in the same format as the tree-walker: `[line N] Kind: message`. Since C has no exceptions, the lexer, parser, compiler and VM raise them with `lang_error` (`src/errors.c`), which `longjmp`s back to the `setjmp` in `compile_source` or `vm_run`.

| Kind | When | Example |
|---|---|---|
| Syntax error | Lexer or parser, before anything runs | `[line 1] Syntax error: Expect ';' after value. (reached end of file)` |
| Compile error | The program parses but can't be compiled | `[line 2] Compile error: Can't return from top-level code.` |
| Runtime error | Raised by the VM while running | `[line 3] Runtime error: Undefined variable 'missing'.` |

After a runtime error, the VM clears its stack, frames and open upvalues, but keeps the globals. The REPL relies on this to continue after an error.

## 10. Differences from the tree-walker

The test suite runs every example on both backends and expects identical output. Two behaviors differ on purpose, both because the VM resolves names before running:

1. **Top-level `return`** is a *compile* error in the VM, so nothing in the file runs. The tree-walker reports a runtime error when it reaches the `return`, after running the lines above it.
2. **A local function can't call a local declared after it.** Inside a function or block, this works in the tree-walker but fails in the VM:

   ```
   fn main() {
     fn isEven(n) { if (n == 0) return true; return isOdd(n - 1); }   // isOdd isn't declared yet
     fn isOdd(n)  { if (n == 0) return false; return isEven(n - 1); }
     print isEven(4);
   }
   ```

   When `isEven` is compiled, `isOdd` doesn't exist yet, so the compiler treats it as a global, and at run time the VM reports `Undefined variable 'isOdd'`. Top-level functions aren't affected because globals are late-bound. `clox` has the same limitation. A fix is to pre-declare all `fn` names in a block before compiling the block's statements.

## 11. Performance

`fib(30)` (1.6 million calls), built with `gcc -O2`, typical of three runs on the development machine:

| Backend | Time |
|---|---|
| Tree-walker | 0.41 s |
| Bytecode VM | 0.22 s |

These are single-machine timings to show scale, not a rigorous benchmark.

## 12. Known limitations and next steps

- **No garbage collector.** Strings, closures and upvalues are freed only at exit (the tree-walker does free scopes that no function captured). A program that builds millions of strings keeps growing. The real fix is a tracing garbage collector (mark-and-sweep): every object is already on the list in `value.c`, and the roots are the stack, the globals and the open upvalues.
- **Strings are copied** on every push. Interning strings, or storing them behind a shared pointer, would make string-heavy code faster.
- **Instructions are 12-byte structs.** Real VMs pack them into bytes (a `uint8_t` array) for better cache use. The `Instr` struct was kept because it is easier to read and debug.

Suggested extensions, in a sensible order:

1. Add an instruction-level trace flag (`--trace`) that prints the stack before each instruction. It's the most useful VM debugging tool.
2. Pre-declare local functions in a block to fix limitation 2 in section 10.
3. Add native functions (`clock()`, `len(s)`): a new `ObjType` for native functions, plus a case in `call_value`.
4. Add `for`, `break` and `continue`. `break` needs a list of exit jumps per loop to patch.
5. Simple optimizations in the compiler: constant folding (`2 * 3` → `Constant 6`) and a `GetLocal`+`Constant`+`Add` superinstruction.
6. Garbage collection (see above).

When you add a language feature, add an `examples/*.rene` file and its `.expected` output. The test runner checks it on both backends automatically, which keeps them in agreement.
