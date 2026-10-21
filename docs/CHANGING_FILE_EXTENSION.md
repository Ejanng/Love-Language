# Changing the file extension

Rene source files use the **`.rene`** extension. The interpreter refuses to run
any file that does not end in it:

```
error: 'program.txt' is not a Rene file (expected a '.rene' extension)
```

Follow the steps below to switch to a different extension. The examples use
`.rn` as the new extension; substitute your own.

## 1. Change the constant in the C++ code (required)

The extension is defined in exactly one place in the C++ code,
[`src/config.hpp`](../src/config.hpp):

```cpp
inline constexpr const char* FILE_EXTENSION = ".rene";
```

Change it, keeping the leading dot:

```cpp
inline constexpr const char* FILE_EXTENSION = ".rn";
```

Everything else in the C++ code reads this constant: the extension check in
`main.cpp`, the error message, and the `--help` text. You don't need to edit
any other source file.

## 2. Rebuild the interpreter (required)

The extension is compiled into the program, so the change only takes effect
after a rebuild:

```sh
cmake -S . -B build
cmake --build build
```

## 3. Rename the example programs

**Git Bash / macOS / Linux:**

```sh
for f in examples/*.rene; do git mv "$f" "${f%.rene}.rn"; done
```

**PowerShell:**

```powershell
Get-ChildItem examples\*.rene | ForEach-Object { git mv $_.FullName ($_.FullName -replace '\.rene$', '.rn') }
```

The `.expected` files keep their names because the test runner pairs them with
programs by base name.

## 4. Update the test runner

In [`tests/run_tests.sh`](../tests/run_tests.sh), change the `EXT` variable.
It has no leading dot:

```sh
EXT="rn"
```

## 5. Update the documentation

Replace `.rene` with the new extension in [`README.md`](../README.md) and in
the comment header of [`src/main.cpp`](../src/main.cpp).

## 6. Optional: editor association

To get syntax colouring in VS Code, map the new extension to a similar
language in `.vscode/settings.json`:

```json
{
    "files.associations": {
        "*.rn": "javascript"
    }
}
```

## Check for leftovers

Search for any remaining mentions of the old extension:

```sh
grep -rn "\.rene\b" --exclude-dir=build --exclude-dir=.git .
```

Then run the tests:

```sh
tests/run_tests.sh
```

If every test passes, the switch is complete.

## Checklist

- [ ] `FILE_EXTENSION` changed in `src/config.hpp`
- [ ] Interpreter rebuilt
- [ ] `examples/*` renamed
- [ ] `EXT` changed in `tests/run_tests.sh`
- [ ] `README.md` and the `src/main.cpp` header updated
- [ ] Leftover search is clean and the tests pass
