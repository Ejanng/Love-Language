#pragma once
// Bytecode backend, part 3: the virtual machine that runs compiled bytecode.
//
// One value stack is shared by every call. A call frame records which closure is running,
// its instruction pointer, and `base`, the stack index of its slot 0 (the callee itself).
// Arguments are already on the stack when a call starts, so they become locals 1..arity for free.
#include <stdbool.h>
#include <stddef.h>

#include "../value.h"
#include "chunk.h"

typedef struct {
    ObjClosure* closure;
    size_t ip;
    size_t base;
} Frame;

typedef struct {
    bool dumpBytecode;
    Value* stack;
    size_t stackCount, stackCap;
    Frame* frames;
    size_t frameCount, frameCap;
    ObjUpvalue** openUpvalues;  // captured variables that are still on the stack
    size_t openCount, openCap;
    Table globals;
} VM;

void vm_init(VM* vm, bool dumpBytecode);
void vm_free(VM* vm);

// Runs a piece of source. Returns 0 on success, 65 for a syntax or compile error, 70 for a runtime error.
// Globals persist between calls, which is what the REPL relies on.
int vm_run(VM* vm, const char* source, size_t length);
