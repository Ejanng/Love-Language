# `src/value.hpp`

**Role:** defines what a value looks like while a Rene program runs, and the
helpers that inspect and print values.

## The `Value` type

```cpp
using Value = std::variant<std::monostate,             // nil
                           bool,                       // true / false
                           double,                     // all numbers
                           std::string,                // strings
                           std::shared_ptr<ListObject>,// lists
                           std::shared_ptr<Callable>>; // functions
```

A `std::variant` holds exactly one of these alternatives at a time. Its
`index()` (0-5) is the position in the list above; `typeName` and `stringify`
use it.

**Lists and functions are shared, not copied.** They are stored behind
`shared_ptr`, so two variables can refer to the same list:

```rene
let a = [1, 2];
let b = a;
push(b, 3);
print a;   // [1, 2, 3]
```

Numbers, strings, booleans and nil are copied when assigned.

## Supporting types

| Type | Purpose |
|------|---------|
| `ListObject` | Wraps a `std::vector<Value>` named `items` |
| `Callable` | Abstract base class for anything you can call with `f(...)`. Implemented by `NativeFunction` (built-ins) and `UserFunction` (functions written in Rene), both in `interpreter.hpp`. |

`Callable` interface:

| Method | Meaning |
|--------|---------|
| `name()` | Used in printing (`<fn fib>`) and in error messages |
| `minArity()` / `maxArity()` | Allowed number of arguments, checked before every call |
| `call(interpreter, args, line)` | Runs the function |

## Helper functions

| Function | Returns |
|----------|---------|
| `isNil`, `isNumber`, `isString`, `isList`, `isCallable` | Whether a value is that type |
| `isTruthy(v)` | `false` only for `nil` and `false`; everything else, including `0` and `""`, is truthy |
| `typeName(v)` | `"nil"`, `"bool"`, `"number"`, `"string"`, `"list"` or `"function"`; also what the built-in `type()` returns |
| `formatNumber(d)` | Whole numbers print without decimals (`3`, not `3.000000`); others print with up to 14 significant digits (`0.3`, `3.5`) |
| `stringify(v, quoteStrings)` | The text `print` shows. Inside lists, strings are quoted: `["a", 1]`. |

## Adding a value type (e.g. maps)

1. Add a struct (e.g. `MapObject`) and a `std::shared_ptr<MapObject>` alternative
   to `Value`, at the end of the list.
2. Add a case to `typeName` and `stringify`.
3. Add syntax to create it (`ast.hpp`, `parser.hpp`) and behaviour
   (`interpreter.hpp`: indexing, `len`, `for … in`).

## Known limitation

A list that contains itself (`push(a, a)`) makes `stringify` recurse forever
when printed.

## Depends on

Only the standard library. `Interpreter` is forward-declared, not included.
