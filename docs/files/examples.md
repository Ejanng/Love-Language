# `examples/`

**Role:** sample Rene programs. They serve as a tutorial (read them in the order
below) and as the test suite: each `NAME.rene` has a matching `NAME.expected`
holding its exact output, which [`tests/run_tests.sh`](run_tests.md) checks.

Run any of them with:

```sh
rene examples/hello.rene
```

## `hello.rene`

**Shows:** `print`, string literals, variables, string concatenation with `+`,
printing several values at once, and operator precedence.

```
Hello, Rene!
Hello, world!
1 + 2 * 3 = 7
```

## `fizzbuzz.rene`

**Shows:** `for … in range(1, 16)`, the `%` operator, `==`, and an
`if / else if / else` chain.

Prints the numbers 1-15, replacing multiples of 3 with `Fizz`, multiples of 5
with `Buzz`, and multiples of both with `FizzBuzz`.

## `functions.rene`

**Shows:** functions in three forms.

| Part | Concept |
|------|---------|
| `fib(n)` | **Recursion**: a function calling itself. `fib(20)` → `6765`. |
| `makeCounter()` | **Closures**: the returned anonymous function keeps access to `count` after `makeCounter` has returned, so each call adds 1. |
| `map(list, f)` | **First-class functions**: a function passed as an argument; here a lambda that squares each number. |

```
fib(20) = 6765
counter: 3
[1, 4, 9, 16]
```

## `lists.rene`

**Shows:** list literals, `push`, indexing (including negative indexes
counting from the end), `len`, index assignment, `while true` with `break` and
`continue`, looping over a string's characters, and the `type()` built-in.

```
["apple", "banana", "cherry", "date"]
first: apple last: date count: 4
["apple", "blueberry", "cherry", "date"]
sum of odd numbers 1..10: 25
r
e
n
e
list number nil function bool
```

## `error.rene`

**Shows:** what a **runtime error** looks like. It prints one line, then
divides by zero on line 4. Execution stops there, so the last `print` never
runs.

```
before the error
[line 4] RuntimeError: division by zero
```

The second line goes to standard error, and the program exits with code 70.
The test checks that the line number and message stay correct.

## `.expected` files

Plain text containing exactly what the matching program prints (stdout and
stderr combined). Don't edit them by hand. If a change to the interpreter
intentionally changes the output, regenerate the file (see
[run_tests.md](run_tests.md#adding-a-test)) and check the new output.
