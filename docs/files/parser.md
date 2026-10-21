# `src/parser.hpp`

**Role:** stage 2 of running a program. The `Parser` reads the list of
[tokens](token.md) and builds the [syntax tree](ast.md). It also defines the
language's **grammar**: which token sequences are valid Rene.

## Public interface

```cpp
rene::Parser parser(tokens);
std::vector<rene::StmtPtr> program = parser.parse();
```

`parse()` returns the program's top-level statements. At the first mistake it
throws a `ReneError` of kind `SyntaxError`, for example:

```
[line 3] SyntaxError: expected ';' after expression (at '}')
```

## Technique: recursive descent

Each grammar rule is a C++ function that calls the functions for the rules
inside it. Precedence comes from the order of those calls: lower-precedence
rules call higher-precedence ones, so higher-precedence operators end up deeper
in the tree and are evaluated first.

| Function | Grammar rule | Handles |
|----------|--------------|---------|
| `assignment` | lowest | `x = …`, `xs[i] = …` (right-associative: `a = b = 1`) |
| `logicOr` | | `or` |
| `logicAnd` | | `and` |
| `equality` | | `==` `!=` |
| `comparison` | | `<` `<=` `>` `>=` |
| `term` | | `+` `-` |
| `factor` | | `*` `/` `%` |
| `unary` | | `-x`, `not x` |
| `call` | | `f(…)`, `xs[i]`, and chains like `f(1)(2)[0]` |
| `primary` | highest | literals, names, `( … )`, `[ … ]` lists, `fn (…) { … }` lambdas |

The full grammar is in the comment at the top of the file.

## Statements

| Function | Parses |
|----------|--------|
| `declaration` | `let …;`, `fn name(…) { … }`, or falls through to `statement` |
| `statement` | Chooses by the first token: `print`, `if`, `while`, `for`, `return`, `break`, `continue`, `{`, or an expression followed by `;` |
| `letDeclaration` | `let name = expr;` (initializer optional) |
| `function(name)` | Parameter list and body; shared by named functions and lambdas |
| `ifStatement` | `if cond { … } else if … else { … }` |
| `whileStatement`, `forStatement` | Loops |
| `printStatement` | `print a, b, c;` (comma-separated values) |
| `returnStatement`, `loopControl` | `return`, `break`, `continue` |
| `blockContents` / `block` | `{ … }` |

### Language rules decided here

- **Braces are required** after `if`, `else`, `while` and `for`, and
  parentheses around conditions are optional: `if x > 1 { … }`.
- **`fn` followed by a name** is a declaration. **`fn` followed by `(`** is an
  anonymous function expression. `declaration()` uses `checkNext` to tell them apart.
- **`return` outside a function** is a syntax error (tracked by `functionDepth_`).
- **`break`/`continue` outside a loop** is a syntax error (tracked by
  `loopDepth_`). The loop depth resets inside a function body, so a `break`
  inside a function can't jump out of a loop that encloses the function.
- **Assignment targets** must be a variable or an index expression. Anything
  else, such as `1 = 2`, gives `invalid assignment target`.

## Helpers

| Function | Purpose |
|----------|---------|
| `peek()` / `previous()` | Current token / the one just consumed |
| `check(type)` / `checkNext(type)` | Tests the current or next token's type without consuming it |
| `match(type)` | Consumes the token if it has that type |
| `consume(type, msg)` | Requires that token, otherwise throws `msg` |
| `arguments(closer, msg)` | Comma-separated expressions up to `)` or `]` |
| `fail(token, msg)` | Throws a `SyntaxError` naming the offending token |

## Common changes

- **New binary operator** at an existing level: add `match(TokenType::X)` to
  that level's loop and handle `X` in `Interpreter::binary`.
- **New precedence level** (e.g. `**` above `*`): write a new function between
  `factor` and `unary`, and call it from `factor`.
- **New statement:** add a `match` in `statement()` and a parsing function that
  returns your new `Stmt` node.

## Depends on

`ast.hpp`, `errors.hpp`, `token.hpp`.
