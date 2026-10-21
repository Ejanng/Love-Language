#pragma once
// Stage 3: a tree-walking interpreter. It evaluates the AST directly.
#include <stddef.h>

#include "value.h"

typedef struct {
    ObjEnv* globals;
    ObjEnv* env;  // the scope currently executing
    ObjEnv** live;  // scopes of the calls and blocks in progress, so an error can free them
    int liveCount, liveCap;
    int callDepth;
    Value returnValue;  // set by a `return` statement while the call unwinds
} Interpreter;

void interpreter_init(Interpreter* in);
void interpreter_free(Interpreter* in);

// Runs a piece of source. Returns 0 on success, 65 for a syntax error, 70 for a runtime error.
// Global variables and functions persist between calls, which is what the REPL relies on.
int interpreter_run(Interpreter* in, const char* source, size_t length);
