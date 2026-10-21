# `src/config.hpp`

**Role:** the settings that define the language. Values that might change
later live here, so they can be changed in one place.

## Contents

All values are in the `rene::config` namespace and are `inline constexpr`,
which makes them compile-time constants.

| Constant | Value | Used by | Purpose |
|----------|-------|---------|---------|
| `LANGUAGE_NAME` | `"Rene"` | `main.cpp` | Shown in `--version`, `--help`, the REPL banner and the wrong-extension error |
| `VERSION` | `"0.1.0"` | `main.cpp` | Printed by `rene --version` and in the REPL banner |
| `FILE_EXTENSION` | `".rene"` | `main.cpp` | `rene` refuses to run files that don't end with this |
| `MAX_CALL_DEPTH` | `512` | `interpreter.hpp` | How deeply functions may call each other before Rene reports `stack overflow` |

## Notes

- **Changing the extension:** edit `FILE_EXTENSION` and rebuild. The full
  checklist (renaming examples, updating the test script) is in
  [CHANGING_FILE_EXTENSION.md](../CHANGING_FILE_EXTENSION.md).
- **`MAX_CALL_DEPTH`:** every Rene call uses several C++ stack frames. The limit
  makes deep recursion stop with a readable Rene error instead of crashing the
  interpreter. `CMakeLists.txt` raises the MSVC stack to 8 MB so 512 levels
  fit comfortably. If you raise this limit a lot, raise the stack size too.
- **Bumping the version:** change `VERSION` when you release a new version of
  the language.

## Depends on

Nothing.
