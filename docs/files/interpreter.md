# `src/interpreter.hpp`

**Role:** stage 3 of running a program, and the largest file. The
`Interpreter` walks the [syntax tree](ast.md) and carries out each node: it
evaluates expressions, runs statements, manages variables and scopes, calls
functions and provides the built-in functions.

This is a **tree-walking interpreter**: it runs the AST directly. There is no
compilation to bytecode or machine code.

## Public interface

```cpp
rene::Interpreter interpreter;   // creates the global scope and built-ins
interpreter.run(program);        // executes top-level statements in order
```

The REPL reuses one `Interpreter` for every line, so variables persist between
lines.

## Parts of the file

### 1. `Environment`: scopes

An `Environment` is one scope: a hash map from names to values, plus a pointer
to its **parent** (enclosing) scope.

| Method | Behaviour |
|--------|-----------|
| `define(name, value)` | Creates a variable in this scope (`let`). Redefining a name in the same scope simply replaces it. |
| `get(name, line)` | Looks in this scope, then the parent, and so on outward. Not found → `undefined variable 'x'`. |
| `assign(name, value, line)` | Updates the nearest existing variable. Not found → `cannot assign to undeclared variable` (Rene requires `let` first). |

A new scope is created for every block `{ }`, every function call, and every
`for … in` iteration.

### 2. Control signals

| Struct | Thrown by | Caught by |
|--------|-----------|-----------|
| `ReturnSignal { value }` | `return` | `callUserFunction` |
| `BreakSignal` | `break` | the enclosing `while` / `for` loop |
| `ContinueSignal` | `continue` | the enclosing loop, which moves on to the next iteration |

Using C++ exceptions lets a `return` deep inside nested blocks and loops unwind
straight to the function call. `ScopeGuard` makes sure the current scope is
restored while that happens.

### 3. Functions

| Struct | What it is |
|--------|------------|
| `NativeFunction` | A built-in, implemented as a C++ lambda, with a minimum and maximum argument count |
| `UserFunction` | A function written in Rene: its `FunctionStmt` (parameters and body) plus its **closure**, the scope it was created in |

**Closures:** because a `UserFunction` keeps the `Environment` where it was
defined, an inner function can still read and change the outer function's
variables after the outer function returns. That is how `makeCounter` in
`examples/functions.rene` works.

`callUserFunction`:
1. Checks the call depth against `config::MAX_CALL_DEPTH` (`stack overflow`
   error if exceeded).
2. Creates a new scope whose parent is the closure, not the caller's scope, and
   binds the parameters.
3. Runs the body and catches `ReturnSignal`. If no `return` runs, the result is `nil`.

### 4. `execute(stmt)`: running statements

A chain of `dynamic_cast` checks, one per statement type:

| Statement | Behaviour |
|-----------|-----------|
| expression | Evaluate and discard the result |
| `print` | Evaluate each value, `stringify`, join with spaces, add a newline |
| `let` | `define` in the current scope (nil if no initializer) |
| block | Run the statements in a new child scope |
| `if` | Uses `isTruthy`: only `nil` and `false` are false |
| `while` | Loops while truthy; handles break/continue signals |
| `for … in` | `executeForIn`: copies the list (or splits the string into one-character strings) first, so changing the list inside the loop is safe |
| `fn` declaration | Creates a `UserFunction` capturing the current scope and defines it by name |
| `return` / `break` / `continue` | Throws the matching signal |

### 5. `evaluate(expr)`: computing values

| Expression | Behaviour |
|------------|-----------|
| literal / variable / assign | Return the value / look up / `assign` |
| unary `-` / `not` | Negate a number / invert truthiness |
| `and` / `or` | Short-circuit and return the deciding operand itself (`nil or 5` → `5`) |
| binary | `binary()`, see below |
| call | `callExpr()`: callee must be a function; checks the argument count, then calls it |
| list literal | New `ListObject` |
| index `xs[i]` | Lists and strings. Index must be a whole number; negative counts from the end; out of range → error. |
| index assign | Lists only |
| lambda | New `UserFunction` capturing the current scope |

`binary()` operator rules:

| Operator | Allowed operands |
|----------|------------------|
| `+` | number + number; if **either** side is a string, both are stringified and joined; list + list makes a new joined list |
| `- * /` | numbers only; `/` by zero is an error |
| `%` | numbers only (`fmod`, so it works with decimals); `% 0` is an error |
| `< <= > >=` | two numbers, or two strings (alphabetical order) |
| `== !=` | any values. Different types are never equal; lists and functions are equal only if they are the same object. |

### 6. `defineBuiltins()`: built-in functions

Each built-in is registered with `native(name, minArgs, maxArgs, lambda)`:

| Name | Args | Does |
|------|------|------|
| `len` | 1 | Length of a list or string |
| `push` | 2 | Append to a list; returns the list |
| `pop` | 1 | Remove and return the last element (error if empty) |
| `str` | 1 | Convert to string |
| `type` | 1 | Type name |
| `num` | 1 | String → number (the whole string must be numeric) |
| `range` | 1-2 | `range(n)` is `[0 … n-1]`; `range(a, b)` is `[a … b-1]` |
| `input` | 0-1 | Print an optional prompt and read one line (nil at end of input) |
| `clock` | 0 | Seconds from a steady clock; subtract two readings to time code |

**Adding a built-in** only takes one call here, for example:

```cpp
native("upper", 1, 1, [](std::vector<Value>& args, int line) -> Value {
    if (!isString(args[0])) fail(line, "upper() expects a string");
    std::string s = std::get<std::string>(args[0]);
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
});
```

Then document it in `README.md`.

## Known limitations

- **Memory cycles:** a closure stored in the scope it captures (e.g. a
  recursive function) forms a `shared_ptr` cycle and is never freed. This is
  harmless for short-running scripts.
- **Speed:** the `dynamic_cast` chains are simple but slower than a visitor or a
  bytecode VM. That's fine for a teaching language.

## Depends on

`ast.hpp`, `config.hpp`, `errors.hpp`, `value.hpp`.
