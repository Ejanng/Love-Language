# `.gitignore`

**Role:** lists files Git should not track. These are build outputs that are
recreated from the source, so committing them would only bloat the repository.

| Pattern | Ignores |
|---------|---------|
| `build/` | The whole CMake build folder (project files, object files, `rene.exe`) |
| `*.exe` | Any compiled executable outside `build/` |
| `*.obj` | MSVC object files |
| `*.pdb` | MSVC debug symbol files |
| `*.ilk` | MSVC incremental-link files |

The installed copy in `%LOCALAPPDATA%\Programs\rene` is outside the project,
so Git never sees it.

To ignore something else (editor temp files, for example), add a line with its
pattern.
