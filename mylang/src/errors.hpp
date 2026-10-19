#pragma once
#include <stdexcept>
#include <string>

// All errors carry the source line so messages can point at the problem.
struct LangError : std::runtime_error {
    int line;
    LangError(int l, const std::string& message) : std::runtime_error(message), line(l) {}
};

// Thrown by the lexer and parser.
struct SyntaxError : LangError {
    using LangError::LangError;
};

// Thrown by the bytecode compiler for programs that parse but can't be compiled.
struct CompileError : LangError {
    using LangError::LangError;
};

// Thrown while a program is running.
struct RuntimeError : LangError {
    using LangError::LangError;
};
