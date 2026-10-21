#include "vm.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "../errors.h"
#include "../lexer.h"
#include "../parser.h"
#include "compiler.h"

#define MAX_CALL_DEPTH 1000

void vm_init(VM* vm, bool dumpBytecode) {
    *vm = (VM){.dumpBytecode = dumpBytecode};
}

void vm_free(VM* vm) {
    free(vm->stack);
    free(vm->frames);
    free(vm->openUpvalues);
    table_free(&vm->globals);
}

static void* grow(void* array, size_t* cap, size_t elemSize) {
    *cap = *cap < 64 ? 64 : *cap * 2;
    void* grown = realloc(array, *cap * elemSize);
    if (!grown) {
        fputs("Out of memory.\n", stderr);
        exit(EXIT_FAILURE);
    }
    return grown;
}

// ---- stack helpers ----

static void push(VM* vm, Value v) {
    if (vm->stackCount == vm->stackCap) vm->stack = grow(vm->stack, &vm->stackCap, sizeof(Value));
    vm->stack[vm->stackCount++] = v;
}

static Value pop(VM* vm) { return vm->stack[--vm->stackCount]; }

static Value* peek(VM* vm) { return &vm->stack[vm->stackCount - 1]; }

static void push_frame(VM* vm, ObjClosure* closure, size_t base) {
    if (vm->frameCount == vm->frameCap) vm->frames = grow(vm->frames, &vm->frameCap, sizeof(Frame));
    vm->frames[vm->frameCount++] = (Frame){closure, 0, base};
}

// ---- upvalues ----

// Returns the open upvalue for a stack slot, creating it if needed,
// so two closures that capture the same variable share it.
static ObjUpvalue* capture_upvalue(VM* vm, size_t slot) {
    for (size_t i = 0; i < vm->openCount; i++) {
        if (vm->openUpvalues[i]->slot == slot) return vm->openUpvalues[i];
    }
    ObjUpvalue* up = upvalue_new(slot);
    if (vm->openCount == vm->openCap) vm->openUpvalues = grow(vm->openUpvalues, &vm->openCap, sizeof(ObjUpvalue*));
    vm->openUpvalues[vm->openCount++] = up;
    return up;
}

// Closes every open upvalue at or above `fromSlot`: those stack slots are about to disappear.
static void close_upvalues(VM* vm, size_t fromSlot) {
    size_t kept = 0;
    for (size_t i = 0; i < vm->openCount; i++) {
        ObjUpvalue* up = vm->openUpvalues[i];
        if (up->slot < fromSlot) {
            vm->openUpvalues[kept++] = up;
        } else {
            up->closed = vm->stack[up->slot];
            up->open = false;
        }
    }
    vm->openCount = kept;
}

// ---- the dispatch loop ----

// The callee sits below its `argc` arguments. It becomes slot 0 of the new frame.
static void call_value(VM* vm, int argc, int line) {
    size_t calleeSlot = vm->stackCount - (size_t)argc - 1;
    Value callee = vm->stack[calleeSlot];
    if (!is_obj_type(callee, OBJ_CLOSURE)) lang_error(ERR_RUNTIME, line, "Can only call functions.");
    ObjClosure* closure = (ObjClosure*)callee.as.obj;
    if (argc != closure->proto->arity) {
        lang_error(ERR_RUNTIME, line, "Expected %d arguments but got %d.", closure->proto->arity, argc);
    }
    if (vm->frameCount > MAX_CALL_DEPTH) lang_error(ERR_RUNTIME, line, "Stack overflow: too many nested calls.");
    push_frame(vm, closure, calleeSlot);
}

static void arithmetic(VM* vm, const Instr* in) {
    Value b = pop(vm);
    Value a = pop(vm);
    if (a.type != VAL_NUMBER || b.type != VAL_NUMBER) lang_error(ERR_RUNTIME, in->line, "Operands must be numbers.");
    double x = a.as.number, y = b.as.number;
    switch (in->op) {
        case OP_GREATER: push(vm, bool_val(x > y)); break;
        case OP_GREATER_EQUAL: push(vm, bool_val(x >= y)); break;
        case OP_LESS: push(vm, bool_val(x < y)); break;
        case OP_LESS_EQUAL: push(vm, bool_val(x <= y)); break;
        case OP_SUBTRACT: push(vm, number_val(x - y)); break;
        case OP_MULTIPLY: push(vm, number_val(x * y)); break;
        case OP_DIVIDE:
            if (y == 0) lang_error(ERR_RUNTIME, in->line, "Division by zero.");
            push(vm, number_val(x / y));
            break;
        case OP_MODULO:
            if (y == 0) lang_error(ERR_RUNTIME, in->line, "Division by zero.");
            push(vm, number_val(fmod(x, y)));
            break;
        default: break;
    }
}

static const char* global_name(const Proto* proto, int operand) {
    return AS_STRING(proto->constants[operand])->chars;
}

static void execute(VM* vm, Proto* script) {
    ObjClosure* closure = closure_new(script);
    push(vm, obj_val(closure));
    push_frame(vm, closure, 0);

    for (;;) {
        Frame* frame = &vm->frames[vm->frameCount - 1];  // re-read every step: calls and returns change the frames
        const Proto* proto = frame->closure->proto;
        const Instr* in = &proto->code[frame->ip++];
        switch (in->op) {
            case OP_CONSTANT: push(vm, proto->constants[in->operand]); break;
            case OP_NIL: push(vm, nil_val()); break;
            case OP_TRUE: push(vm, bool_val(true)); break;
            case OP_FALSE: push(vm, bool_val(false)); break;
            case OP_POP: vm->stackCount--; break;

            case OP_GET_LOCAL: push(vm, vm->stack[frame->base + (size_t)in->operand]); break;
            case OP_SET_LOCAL: vm->stack[frame->base + (size_t)in->operand] = *peek(vm); break;

            case OP_GET_GLOBAL: {
                const char* name = global_name(proto, in->operand);
                Value* v = table_get(&vm->globals, name);
                if (!v) lang_error(ERR_RUNTIME, in->line, "Undefined variable '%s'.", name);
                push(vm, *v);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                Value v = pop(vm);
                table_set(&vm->globals, global_name(proto, in->operand), v);
                break;
            }
            case OP_SET_GLOBAL: {
                const char* name = global_name(proto, in->operand);
                Value* v = table_get(&vm->globals, name);
                if (!v) lang_error(ERR_RUNTIME, in->line, "Undefined variable '%s'.", name);
                *v = *peek(vm);
                break;
            }

            case OP_GET_UPVALUE: {
                const ObjUpvalue* up = frame->closure->upvalues[in->operand];
                push(vm, up->open ? vm->stack[up->slot] : up->closed);
                break;
            }
            case OP_SET_UPVALUE: {
                ObjUpvalue* up = frame->closure->upvalues[in->operand];
                *(up->open ? &vm->stack[up->slot] : &up->closed) = *peek(vm);
                break;
            }

            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, bool_val(values_equal(a, b)));
                break;
            }
            case OP_NOT_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, bool_val(!values_equal(a, b)));
                break;
            }
            case OP_ADD: {
                Value b = pop(vm);
                Value a = pop(vm);
                if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
                    push(vm, number_val(a.as.number + b.as.number));
                } else if (is_obj_type(a, OBJ_STRING) || is_obj_type(b, OBJ_STRING)) {
                    // string + anything joins them as text
                    push(vm, obj_val(string_concat(value_to_string(a), value_to_string(b))));
                } else {
                    lang_error(ERR_RUNTIME, in->line, "Operands of '+' must be two numbers or include a string.");
                }
                break;
            }
            case OP_GREATER:
            case OP_GREATER_EQUAL:
            case OP_LESS:
            case OP_LESS_EQUAL:
            case OP_SUBTRACT:
            case OP_MULTIPLY:
            case OP_DIVIDE:
            case OP_MODULO: arithmetic(vm, in); break;

            case OP_NOT: push(vm, bool_val(!is_truthy(pop(vm)))); break;
            case OP_NEGATE: {
                Value* v = peek(vm);
                if (v->type != VAL_NUMBER) lang_error(ERR_RUNTIME, in->line, "Operand of '-' must be a number.");
                v->as.number = -v->as.number;
                break;
            }

            case OP_PRINT: {
                ObjString* text = value_to_string(pop(vm));
                fwrite(text->chars, 1, text->length, stdout);
                fputc('\n', stdout);
                break;
            }

            case OP_JUMP: frame->ip = (size_t)in->operand; break;
            case OP_JUMP_IF_FALSE:
                if (!is_truthy(*peek(vm))) frame->ip = (size_t)in->operand;
                break;
            case OP_JUMP_IF_TRUE:
                if (is_truthy(*peek(vm))) frame->ip = (size_t)in->operand;
                break;

            case OP_CALL: call_value(vm, in->operand, in->line); break;

            case OP_CLOSURE: {
                ObjClosure* fn = closure_new(proto->functions[in->operand]);
                for (int i = 0; i < fn->upvalueCount; i++) {
                    const UpvalueRef* ref = &fn->proto->upvalues[i];
                    fn->upvalues[i] = ref->isLocal ? capture_upvalue(vm, frame->base + (size_t)ref->index)
                                                   : frame->closure->upvalues[ref->index];
                }
                push(vm, obj_val(fn));
                break;
            }
            case OP_CLOSE_UPVALUE:
                close_upvalues(vm, vm->stackCount - 1);
                vm->stackCount--;
                break;

            case OP_RETURN: {
                Value result = pop(vm);
                size_t base = frame->base;
                vm->frameCount--;
                close_upvalues(vm, base);
                vm->stackCount = base;  // drop the callee, its arguments and its locals
                if (vm->frameCount == 0) return;  // the top-level script finished
                push(vm, result);
                break;
            }
        }
    }
}

// Lexes, parses and compiles. Returns the script, or NULL after reporting a syntax or compile error.
static Proto* compile_source(const char* source, size_t length) {
    if (setjmp(g_err.env) != 0) {
        report_error();
        return NULL;
    }
    int count;
    Token* tokens = lex(source, length, &count);
    return compile(parse(tokens, count));
}

int vm_run(VM* vm, const char* source, size_t length) {
    Proto* script = compile_source(source, length);
    if (!script) return EXIT_COMPILE_ERROR;

    if (vm->dumpBytecode) {
        disassemble(script, stdout);
        fputc('\n', stdout);
    }
    if (setjmp(g_err.env) != 0) {
        report_error();
        vm->stackCount = 0;
        vm->frameCount = 0;
        vm->openCount = 0;
        return EXIT_RUNTIME_ERROR;
    }
    execute(vm, script);
    return EXIT_OK;
}
