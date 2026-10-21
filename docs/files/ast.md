# `src/ast.hpp`

**Role:** defines the nodes of the **abstract syntax tree (AST)**, the tree the
parser builds from tokens. The interpreter then walks this tree.

For example, `print 1 + 2 * 3;` becomes:

```
PrintStmt
└── BinaryExpr (+)
    ├── LiteralExpr 1
    └── BinaryExpr (*)
        ├── LiteralExpr 2
        └── LiteralExpr 3
```

The tree's shape encodes operator precedence (`*` binds tighter than `+`), so
the interpreter doesn't need to know about precedence.

## Two families of nodes

- **`Expr`** (expression): produces a value, e.g. `1 + 2`, `f(x)`, `list[0]`.
- **`Stmt`** (statement): performs an action, e.g. `print`, `let`, `while`.

Both base classes store a `line` for error messages and have a virtual
destructor, so the interpreter can tell node types apart with `dynamic_cast`.
Nodes are held through `ExprPtr` / `StmtPtr` (`std::shared_ptr`). Shared
pointers matter because function bodies must outlive parsing: a closure keeps
its body alive.

## Expression nodes

| Node | Rene syntax | Fields |
|------|-------------|--------|
| `LiteralExpr` | `42`, `"hi"`, `true`, `nil` | `value` (a ready-made `Value`) |
| `VariableExpr` | `count` | `name` |
| `AssignExpr` | `count = 5` | `name`, `value` |
| `UnaryExpr` | `-x`, `not done` | `op`, `right` |
| `BinaryExpr` | `a + b`, `a < b`, `a == b` | `left`, `op`, `right` |
| `LogicalExpr` | `a and b`, `a or b` | `left`, `op`, `right`. Separate from `BinaryExpr` because these short-circuit and may skip evaluating `right`. |
| `CallExpr` | `f(1, 2)` | `callee`, `args` |
| `ListExpr` | `[1, 2, 3]` | `elements` |
| `IndexExpr` | `xs[0]` | `object`, `index` |
| `IndexAssignExpr` | `xs[0] = 9` | `object`, `index`, `value` |
| `LambdaExpr` | `fn (x) { … }` | `function` (a `FunctionStmt` named `<anonymous>`) |

## Statement nodes

| Node | Rene syntax | Fields |
|------|-------------|--------|
| `ExpressionStmt` | `f(x);` | `expression` |
| `PrintStmt` | `print a, b;` | `values` |
| `LetStmt` | `let x = 1;` / `let x;` | `name`, `initializer` (null means nil) |
| `BlockStmt` | `{ … }` | `statements` |
| `IfStmt` | `if c { … } else { … }` | `condition`, `thenBranch`, `elseBranch` (null when there is no `else`; an `else if` is a nested `IfStmt`) |
| `WhileStmt` | `while c { … }` | `condition`, `body` |
| `ForInStmt` | `for x in list { … }` | `variable`, `iterable`, `body` |
| `BreakStmt` / `ContinueStmt` | `break;` / `continue;` | none |
| `FunctionStmt` | `fn name(a, b) { … }` | `name`, `params`, `body` |
| `ReturnStmt` | `return x;` / `return;` | `value` (null means nil) |

## Adding a node

1. Add a struct here that inherits from `Expr` or `Stmt`, with a constructor
   that takes the line number last (matching the others).
2. Create it in `parser.hpp`.
3. Handle it in `interpreter.hpp`: add a branch in `evaluate` for an `Expr`, or
   in `execute` for a `Stmt`. Otherwise the interpreter reports
   `unknown expression` or `unknown statement`.

## Depends on

`token.hpp` (for `TokenType` in operator fields), `value.hpp` (for `LiteralExpr`).
