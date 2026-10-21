# `tests/run_tests.sh`

**Role:** an automated regression test. It runs every example program and
checks that its output still matches the saved expected output. Run it after
changing the interpreter to confirm nothing broke.

## Running it

It is a Bash script. On Windows, use **Git Bash** or call it through Bash:

```sh
tests/run_tests.sh                                  # finds the built rene automatically
tests/run_tests.sh build/Release/rene.exe           # or name the executable
```

From PowerShell:

```powershell
& "C:\Program Files\Git\bin\bash.exe" tests/run_tests.sh
```

Sample output:

```
PASS  error.rene
PASS  fizzbuzz.rene
PASS  functions.rene
PASS  hello.rene
PASS  lists.rene
5 passed, 0 failed
```

The script exits with code 0 if every test passes and 1 if any fail, so it can
be used in CI.

## How it works

1. **Finds the interpreter:** uses the first argument if given; otherwise tries
   `build/rene`, `build/rene.exe` and `build/Release/rene.exe`.
2. **Loops over `examples/*.rene`** (the extension comes from the `EXT`
   variable).
3. For each program with a matching `.expected` file (`hello.rene` →
   `hello.expected`), it runs the program and captures **stdout and stderr
   together**, so expected error messages are tested too.
4. Removes `\r` from both sides, so Windows line endings don't cause false
   failures.
5. Prints `PASS`, or `FAIL` with a `diff` showing the difference (`<` lines are
   actual output, `>` lines are expected).

Programs without a `.expected` file are skipped.

## Adding a test

1. Write `examples/NAME.rene`.
2. Run it and check that the output is correct by hand.
3. Save the output as the expected result:
   ```sh
   build/Release/rene.exe examples/NAME.rene > examples/NAME.expected 2>&1
   ```
4. Run `tests/run_tests.sh`.

## If you change the file extension

Update `EXT="rene"` at the top of the script (no dot). See
[CHANGING_FILE_EXTENSION.md](../CHANGING_FILE_EXTENSION.md).
