#pragma once
// Bytecode backend, part 2: the compiler walks the AST once and emits stack bytecode.
//
// Name resolution happens here, at compile time:
//   - a name declared in an enclosing block of the current function is a local (a stack slot),
//   - a local of an enclosing function is an upvalue (captured by the closure),
//   - anything else is a global, looked up by name at run time.
// Top-level `let` and `fn` declarations are globals; declarations inside any block are locals.
#include "../ast.h"
#include "chunk.h"

// Compiles a whole program into the top-level "<script>" function. Throws ERR_COMPILE for
// programs that parse but can't be compiled.
Proto* compile(const Program* program);
