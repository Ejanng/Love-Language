# `src/lexer.hpp`

**Role:** stage 1 of running a program. The `Lexer` (also called a *scanner* or
*tokenizer*) reads the source text one character at a time and groups the
characters into [tokens](token.md).

```
"print 1 + 2;"   ──►   Print  Number(1)  Plus  Number(2)  Semicolon  EndOfFile
```

## Public interface

```cpp
rene::Lexer lexer(sourceText);
std::vector<rene::Token> tokens = lexer.tokenize();
```

`tokenize()` returns every token, always ending with `EndOfFile`. On invalid
input it throws a `ReneError` of kind `SyntaxError`.

## How it works

The lexer keeps three positions:

- `start_`: index where the current token began
- `current_`: index of the next character to read
- `line_`: current line number (incremented on every `\n`)

`tokenize()` loops: set `start_ = current_`, then call `scanToken()`, which reads
one token (or skips whitespace or a comment).

### Private helpers

| Function | What it does |
|----------|--------------|
| `advance()` | Returns the current character and moves forward |
| `peek()` / `peekNext()` | Looks at the next one or two characters without consuming them |
| `match(c)` | Consumes the next character only if it equals `c`. Used for two-character operators such as `<=`. |
| `add(type)` | Records a token whose lexeme is the text from `start_` to `current_` |
| `fail(msg)` | Throws a `SyntaxError` on the current line |
| `scanToken()` | The main `switch` that decides what the next token is |
| `string()` | Reads a `"…"` literal and decodes the escapes `\n \t \" \\` |
| `number()` | Reads `123` or `12.5`, converting the text with `std::stod` |
| `identifier()` | Reads a name; if it is in the keyword table, emits the keyword token instead |

## Rules it enforces

| Input | Result |
|-------|--------|
| `// text` | Comment; skipped until end of line |
| spaces, tabs, `\r`, `\n` | Skipped (`\n` increments the line counter) |
| `!` not followed by `=` | Error: `unexpected '!' (use 'not' for negation)` |
| `"abc` with no closing quote | Error: `unterminated string` (reported at the line where the string started) |
| `"\q"` | Error: `unknown escape sequence '\q'` |
| `@`, `#`, `$`, … | Error: `unexpected character` |
| `1.` | Number `1` followed by an error at `.`; a decimal point must be followed by a digit |

Strings may span several lines.

## Common changes

- **New keyword:** add the `TokenType` in `token.hpp`, then one entry in the
  `keywords` map inside `identifier()`.
- **New operator:** add a `case` in `scanToken()`. Use `match()` for
  two-character operators.
- **New escape sequence:** add a `case` in `string()`.
- **Different comment style** (e.g. `#`): add a `case '#'` that skips to end of
  line, like the `//` branch does.

## Depends on

`errors.hpp`, `token.hpp`.
