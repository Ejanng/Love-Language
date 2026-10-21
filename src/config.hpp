#pragma once

// Central configuration for the Rene language.
//
// The source-file extension lives here and ONLY here in the C++ code.
// To change it, see docs/CHANGING_FILE_EXTENSION.md.

namespace rene::config {

inline constexpr const char* LANGUAGE_NAME = "Rene";
inline constexpr const char* VERSION = "0.1.0";

// Extension (including the leading dot) that Rene source files must use.
inline constexpr const char* FILE_EXTENSION = ".rene";

// Maximum nesting of function calls before a "stack overflow" runtime error.
inline constexpr int MAX_CALL_DEPTH = 512;

}  // namespace rene::config
