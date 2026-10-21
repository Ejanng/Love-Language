#include "compiler.h"

#include <string.h>

#include "../arena.h"
#include "../errors.h"

typedef struct {
    const char* name;
    int depth;
    bool captured;  // an inner closure refers to it, so it must be moved off the stack when it ends
} Local;

// Compiler state for one function being compiled. They form a chain through `enclosing`.
typedef struct FunctionState {
    struct FunctionState* enclosing;
    Proto* proto;
    Local* locals;
    int localCount, localCap;
    int scopeDepth;
} FunctionState;

static FunctionState* current = NULL;

static void statement(const Stmt* s);
static void expression(const Expr* e);

static void add_local(const char* name) {
    FunctionState* fs = current;
    fs->locals = arena_array_reserve(fs->locals, fs->localCount, &fs->localCap, sizeof(Local));
    fs->locals[fs->localCount++] = (Local){name, fs->scopeDepth, false};
}

static void state_init(FunctionState* fs, FunctionState* enclosing, const char* name) {
    *fs = (FunctionState){.enclosing = enclosing, .proto = arena_alloc(sizeof(Proto))};
    fs->proto->name = name;
    current = fs;
    add_local("");  // slot 0 holds the function being called
}

// ---- emitting ----

static int emit(Op op, int operand, int line) {
    Proto* p = current->proto;
    p->code = arena_array_reserve(p->code, p->codeCount, &p->codeCap, sizeof(Instr));
    p->code[p->codeCount] = (Instr){op, operand, line};
    return p->codeCount++;
}

static int here(void) { return current->proto->codeCount; }

// Jumps are emitted before their target is known and patched once it is ("backpatching").
static void patch_jump(int at) { current->proto->code[at].operand = here(); }

static int add_constant(Value v) {
    Proto* p = current->proto;
    p->constants = arena_array_reserve(p->constants, p->constCount, &p->constCap, sizeof(Value));
    p->constants[p->constCount] = v;
    return p->constCount++;
}

static int add_name_constant(const char* name) { return add_constant(obj_val(string_copy(name, strlen(name)))); }

// ---- scopes and names ----

static void begin_scope(void) { current->scopeDepth++; }

static void end_scope(int line) {
    FunctionState* fs = current;
    fs->scopeDepth--;
    while (fs->localCount > 1 && fs->locals[fs->localCount - 1].depth > fs->scopeDepth) {
        emit(fs->locals[fs->localCount - 1].captured ? OP_CLOSE_UPVALUE : OP_POP, 0, line);
        fs->localCount--;
    }
}

static int resolve_local(const FunctionState* fs, const char* name) {
    for (int i = fs->localCount - 1; i > 0; i--) {
        if (strcmp(fs->locals[i].name, name) == 0) return i;
    }
    return -1;
}

static int add_upvalue(FunctionState* fs, bool isLocal, int index) {
    Proto* p = fs->proto;
    for (int i = 0; i < p->upvalueCount; i++) {
        if (p->upvalues[i].isLocal == isLocal && p->upvalues[i].index == index) return i;
    }
    p->upvalues = arena_array_reserve(p->upvalues, p->upvalueCount, &p->upvalueCap, sizeof(UpvalueRef));
    p->upvalues[p->upvalueCount] = (UpvalueRef){isLocal, index};
    return p->upvalueCount++;
}

// Looks for `name` in the enclosing functions, threading an upvalue through each level in between.
static int resolve_upvalue(FunctionState* fs, const char* name) {
    if (!fs->enclosing) return -1;
    int local = resolve_local(fs->enclosing, name);
    if (local != -1) {
        fs->enclosing->locals[local].captured = true;
        return add_upvalue(fs, true, local);
    }
    int up = resolve_upvalue(fs->enclosing, name);
    if (up != -1) return add_upvalue(fs, false, up);
    return -1;
}

static void named_variable(const char* name, bool assign, int line) {
    int slot = resolve_local(current, name);
    if (slot != -1) {
        emit(assign ? OP_SET_LOCAL : OP_GET_LOCAL, slot, line);
        return;
    }
    int up = resolve_upvalue(current, name);
    if (up != -1) {
        emit(assign ? OP_SET_UPVALUE : OP_GET_UPVALUE, up, line);
        return;
    }
    emit(assign ? OP_SET_GLOBAL : OP_GET_GLOBAL, add_name_constant(name), line);
}

static void define_variable(const char* name, int line) {
    if (current->scopeDepth > 0) {
        add_local(name);  // the new local takes the next stack slot, which is where its value already sits
    } else {
        emit(OP_DEFINE_GLOBAL, add_name_constant(name), line);
    }
}

// ---- statements ----

// Compiles a function body into its own Proto and emits a Closure instruction that creates it.
static void function(const Stmt* s) {
    FunctionState fs;
    state_init(&fs, current, s->name);
    fs.proto->arity = s->paramCount;
    fs.scopeDepth = 1;  // parameters and the body's declarations are locals
    for (int i = 0; i < s->paramCount; i++) add_local(s->params[i]);
    for (int i = 0; i < s->bodyCount; i++) statement(s->body[i]);
    emit(OP_NIL, 0, s->line);  // falling off the end returns nil
    emit(OP_RETURN, 0, s->line);
    current = fs.enclosing;

    Proto* outer = current->proto;
    outer->functions = arena_array_reserve(outer->functions, outer->functionCount, &outer->functionCap, sizeof(Proto*));
    outer->functions[outer->functionCount++] = fs.proto;
    emit(OP_CLOSURE, outer->functionCount - 1, s->line);
}

static void statement(const Stmt* s) {
    switch (s->kind) {
        case STMT_EXPRESSION:
            expression(s->expr);
            emit(OP_POP, 0, s->line);
            break;
        case STMT_PRINT:
            expression(s->expr);
            emit(OP_PRINT, 0, s->line);
            break;
        case STMT_LET:
            // The initializer is compiled before the name exists, so `let x = x;` reads the outer x.
            if (s->expr) {
                expression(s->expr);
            } else {
                emit(OP_NIL, 0, s->line);
            }
            define_variable(s->name, s->line);
            break;
        case STMT_BLOCK:
            begin_scope();
            for (int i = 0; i < s->bodyCount; i++) statement(s->body[i]);
            end_scope(s->line);
            break;
        case STMT_IF: {
            expression(s->expr);
            int toElse = emit(OP_JUMP_IF_FALSE, 0, s->line);
            emit(OP_POP, 0, s->line);  // drop the condition on the "then" path
            statement(s->thenBranch);
            int toEnd = emit(OP_JUMP, 0, s->line);
            patch_jump(toElse);
            emit(OP_POP, 0, s->line);  // drop the condition on the "else" path
            if (s->elseBranch) statement(s->elseBranch);
            patch_jump(toEnd);
            break;
        }
        case STMT_WHILE: {
            int loopStart = here();
            expression(s->expr);
            int toExit = emit(OP_JUMP_IF_FALSE, 0, s->line);
            emit(OP_POP, 0, s->line);
            statement(s->thenBranch);
            emit(OP_JUMP, loopStart, s->line);
            patch_jump(toExit);
            emit(OP_POP, 0, s->line);
            break;
        }
        case STMT_FUNCTION:
            // A local function is declared before its body is compiled so it can call itself.
            if (current->scopeDepth > 0) add_local(s->name);
            function(s);
            if (current->scopeDepth == 0) emit(OP_DEFINE_GLOBAL, add_name_constant(s->name), s->line);
            break;
        case STMT_RETURN:
            if (!current->enclosing) lang_error(ERR_COMPILE, s->line, "Can't return from top-level code.");
            if (s->expr) {
                expression(s->expr);
            } else {
                emit(OP_NIL, 0, s->line);
            }
            emit(OP_RETURN, 0, s->line);
            break;
    }
}

// ---- expressions ----
// Every expression leaves exactly one value on the stack.

static Op binary_op(TokenType t) {
    switch (t) {
        case TOK_EQUAL_EQUAL: return OP_EQUAL;
        case TOK_BANG_EQUAL: return OP_NOT_EQUAL;
        case TOK_GREATER: return OP_GREATER;
        case TOK_GREATER_EQUAL: return OP_GREATER_EQUAL;
        case TOK_LESS: return OP_LESS;
        case TOK_LESS_EQUAL: return OP_LESS_EQUAL;
        case TOK_PLUS: return OP_ADD;
        case TOK_MINUS: return OP_SUBTRACT;
        case TOK_STAR: return OP_MULTIPLY;
        case TOK_SLASH: return OP_DIVIDE;
        default: return OP_MODULO;
    }
}

static void expression(const Expr* e) {
    switch (e->kind) {
        case EXPR_NUMBER: emit(OP_CONSTANT, add_constant(number_val(e->number)), e->line); break;
        case EXPR_STRING: emit(OP_CONSTANT, add_constant(obj_val(string_copy(e->text, e->textLength))), e->line); break;
        case EXPR_BOOL: emit(e->boolean ? OP_TRUE : OP_FALSE, 0, e->line); break;
        case EXPR_NIL: emit(OP_NIL, 0, e->line); break;
        case EXPR_VARIABLE: named_variable(e->text, false, e->line); break;
        case EXPR_ASSIGN:
            expression(e->left);
            named_variable(e->text, true, e->line);
            break;
        case EXPR_UNARY:
            expression(e->left);
            emit(e->op == TOK_BANG ? OP_NOT : OP_NEGATE, 0, e->line);
            break;
        case EXPR_BINARY:
            expression(e->left);
            expression(e->right);
            emit(binary_op(e->op), 0, e->line);
            break;
        case EXPR_LOGICAL: {
            // Short-circuit: if the left side decides the answer, skip the right side and keep the left value.
            expression(e->left);
            int skip = emit(e->op == TOK_OR ? OP_JUMP_IF_TRUE : OP_JUMP_IF_FALSE, 0, e->line);
            emit(OP_POP, 0, e->line);
            expression(e->right);
            patch_jump(skip);
            break;
        }
        case EXPR_CALL:
            expression(e->left);
            for (int i = 0; i < e->argCount; i++) expression(e->args[i]);
            emit(OP_CALL, e->argCount, e->line);
            break;
    }
}

Proto* compile(const Program* program) {
    FunctionState script;
    state_init(&script, NULL, "<script>");
    for (int i = 0; i < program->count; i++) statement(program->stmts[i]);
    int lastLine = program->count == 0 ? 1 : program->stmts[program->count - 1]->line;
    emit(OP_NIL, 0, lastLine);
    emit(OP_RETURN, 0, lastLine);
    current = NULL;
    return script.proto;
}
