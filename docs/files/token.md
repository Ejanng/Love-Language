# `src/token.hpp`

**Role:** defines a *token*, the smallest meaningful piece of Rene source code
(a keyword, a number, an operator, …). The lexer produces tokens and the
parser consumes them.

## Contents

### `enum class TokenType`

Every kind of token Rene understands:

| Group | Token types | Source text |
|-------|-------------|-------------|
| Single characters | `LeftParen` `RightParen` `LeftBrace` `RightBrace` `LeftBracket` `RightBracket` `Comma` `Semicolon` `Plus` `Minus` `Star` `Slash` `Percent` | `( ) { } [ ] , ; + - * / %` |
| One or two characters | `Equal` `EqualEqual` `BangEqual` `Less` `LessEqual` `Greater` `GreaterEqual` | `= == != < <= > >=` |
| Literals | `Identifier` `String` `Number` | `count`, `"hi"`, `3.14` |
| Keywords | `Let` `Fn` `Return` `If` `Else` `While` `For` `In` `Break` `Continue` `Print` `True` `False` `Nil` `And` `Or` `Not` | `let fn return if else …` |
| End marker | `EndOfFile` | (added after the last token) |

### `struct Token`

| Field | Meaning |
|-------|---------|
| `type` | One of the `TokenType` values |
| `lexeme` | The exact source text, e.g. `"while"` or `"<="`. For **string** tokens it holds the decoded value, with escapes such as `\n` already converted and no surrounding quotes. |
| `number` | The numeric value, for `Number` tokens only |
| `line` | Source line, used in error messages |

## Example

`let x = 10;` becomes:

```
Let "let"   Identifier "x"   Equal "="   Number "10" (number=10)   Semicolon ";"   EndOfFile
```

## Adding a token

1. Add the name to `TokenType`.
2. Make the lexer produce it (`lexer.hpp`). For a keyword, add it to the
   keyword table.
3. Make the parser use it (`parser.hpp`).

## Depends on

Only `<string>`.
