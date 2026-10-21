#include "interpreter.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"

#define MAX_CALL_DEPTH 1000

static void* heap_array_reserve(void* array, int count, int* cap, size_t elemSize) {
    if (count < *cap) return array;
    *cap = *cap < 16 ? 16 : *cap * 2;
    array = realloc(array, (size_t)*cap * elemSize);
    if (!array) {
        fputs("Out of memory.\n", stderr);
        exit(EXIT_FAILURE);
    }
    return array;
}

static Value evaluate(Interpreter* in, const Expr* e);
static bool execute(Interpreter* in, const Stmt* s);

// ---- statements ----

// Runs `count` statements in `scope`, then frees the scope unless a function captured it.
// Returns true if one of the statements executed a `return`.
// The previous scope is restored when the block ends (after a runtime error, interpreter_run resets it).
static bool execute_block(Interpreter* in, Stmt* const* stmts, int count, ObjEnv* scope) {
    ObjEnv* saved = in->env;
    in->env = scope;
    in->live = heap_array_reserve(in->live, in->liveCount, &in->liveCap, sizeof(ObjEnv*));
    in->live[in->liveCount++] = scope;
    bool returned = false;
    for (int i = 0; i < count && !returned; i++) returned = execute(in, stmts[i]);
    in->liveCount--;
    in->env = saved;
    env_release(scope);
    return returned;
}

// Returns true if a `return` statement ran, so the enclosing loops and blocks stop and the call unwinds.
static bool execute(Interpreter* in, const Stmt* s) {
    switch (s->kind) {
        case STMT_EXPRESSION: evaluate(in, s->expr); break;
        case STMT_PRINT: {
            ObjString* text = value_to_string(evaluate(in, s->expr));
            fwrite(text->chars, 1, text->length, stdout);
            fputc('\n', stdout);
            break;
        }
        case STMT_LET: env_define(in->env, s->name, s->expr ? evaluate(in, s->expr) : nil_val()); break;
        case STMT_BLOCK: return execute_block(in, s->body, s->bodyCount, env_new(in->env));
        case STMT_IF:
            if (is_truthy(evaluate(in, s->expr))) return execute(in, s->thenBranch);
            if (s->elseBranch) return execute(in, s->elseBranch);
            break;
        case STMT_WHILE:
            while (is_truthy(evaluate(in, s->expr))) {
                if (execute(in, s->thenBranch)) return true;
            }
            break;
        case STMT_FUNCTION: env_define(in->env, s->name, obj_val(function_new(s, in->env))); break;
        case STMT_RETURN:
            if (in->callDepth == 0) lang_error(ERR_RUNTIME, s->line, "Can't return from top-level code.");
            in->returnValue = s->expr ? evaluate(in, s->expr) : nil_val();
            return true;
    }
    return false;
}

// ---- expressions ----

static Value eval_unary(Interpreter* in, const Expr* e) {
    Value operand = evaluate(in, e->left);
    if (e->op == TOK_BANG) return bool_val(!is_truthy(operand));
    if (operand.type != VAL_NUMBER) lang_error(ERR_RUNTIME, e->line, "Operand of '-' must be a number.");
    return number_val(-operand.as.number);
}

static Value eval_binary(Interpreter* in, const Expr* e) {
    Value l = evaluate(in, e->left);
    Value r = evaluate(in, e->right);

    if (e->op == TOK_EQUAL_EQUAL) return bool_val(values_equal(l, r));
    if (e->op == TOK_BANG_EQUAL) return bool_val(!values_equal(l, r));

    if (e->op == TOK_PLUS) {
        if (l.type == VAL_NUMBER && r.type == VAL_NUMBER) return number_val(l.as.number + r.as.number);
        if (is_obj_type(l, OBJ_STRING) || is_obj_type(r, OBJ_STRING)) {
            // string + anything joins them as text
            return obj_val(string_concat(value_to_string(l), value_to_string(r)));
        }
        lang_error(ERR_RUNTIME, e->line, "Operands of '+' must be two numbers or include a string.");
    }

    if (l.type != VAL_NUMBER || r.type != VAL_NUMBER) lang_error(ERR_RUNTIME, e->line, "Operands must be numbers.");
    double a = l.as.number, b = r.as.number;

    switch (e->op) {
        case TOK_MINUS: return number_val(a - b);
        case TOK_STAR: return number_val(a * b);
        case TOK_SLASH:
            if (b == 0) lang_error(ERR_RUNTIME, e->line, "Division by zero.");
            return number_val(a / b);
        case TOK_PERCENT:
            if (b == 0) lang_error(ERR_RUNTIME, e->line, "Division by zero.");
            return number_val(fmod(a, b));
        case TOK_GREATER: return bool_val(a > b);
        case TOK_GREATER_EQUAL: return bool_val(a >= b);
        case TOK_LESS: return bool_val(a < b);
        case TOK_LESS_EQUAL: return bool_val(a <= b);
        default: break;
    }
    lang_error(ERR_RUNTIME, e->line, "Unknown operator.");
}

static Value eval_call(Interpreter* in, const Expr* e) {
    Value callee = evaluate(in, e->left);
    Value args[e->argCount > 0 ? e->argCount : 1];
    for (int i = 0; i < e->argCount; i++) args[i] = evaluate(in, e->args[i]);

    if (!is_obj_type(callee, OBJ_FUNCTION)) lang_error(ERR_RUNTIME, e->line, "Can only call functions.");
    const ObjFunction* fn = (const ObjFunction*)callee.as.obj;
    const Stmt* decl = fn->decl;

    if (e->argCount != decl->paramCount) {
        lang_error(ERR_RUNTIME, e->line, "Expected %d arguments but got %d.", decl->paramCount, e->argCount);
    }
    if (in->callDepth >= MAX_CALL_DEPTH) lang_error(ERR_RUNTIME, e->line, "Stack overflow: too many nested calls.");

    ObjEnv* scope = env_new(fn->closure);
    for (int i = 0; i < e->argCount; i++) env_define(scope, decl->params[i], args[i]);

    in->callDepth++;
    in->returnValue = nil_val();  // functions without a return give nil
    execute_block(in, decl->body, decl->bodyCount, scope);
    in->callDepth--;
    Value result = in->returnValue;
    in->returnValue = nil_val();
    return result;
}

static Value evaluate(Interpreter* in, const Expr* e) {
    switch (e->kind) {
        case EXPR_NUMBER: return number_val(e->number);
        case EXPR_STRING: return obj_val(string_copy(e->text, e->textLength));
        case EXPR_BOOL: return bool_val(e->boolean);
        case EXPR_NIL: return nil_val();
        case EXPR_VARIABLE: {
            const Value* v = env_lookup(in->env, e->text);
            if (!v) lang_error(ERR_RUNTIME, e->line, "Undefined variable '%s'.", e->text);
            return *v;
        }
        case EXPR_ASSIGN: {
            Value v = evaluate(in, e->left);
            if (!env_assign(in->env, e->text, v)) lang_error(ERR_RUNTIME, e->line, "Undefined variable '%s'.", e->text);
            return v;
        }
        case EXPR_UNARY: return eval_unary(in, e);
        case EXPR_BINARY: return eval_binary(in, e);
        case EXPR_LOGICAL: {
            Value left = evaluate(in, e->left);
            if (e->op == TOK_OR) {
                if (is_truthy(left)) return left;
            } else {
                if (!is_truthy(left)) return left;
            }
            return evaluate(in, e->right);
        }
        case EXPR_CALL: return eval_call(in, e);
    }
    return nil_val();
}

// ---- running source ----

void interpreter_init(Interpreter* in) {
    *in = (Interpreter){.globals = env_new(NULL), .returnValue = nil_val()};
    env_capture(in->globals);  // globals live until exit
    in->env = in->globals;
}

void interpreter_free(Interpreter* in) { free(in->live); }

// Lexes and parses. Returns the program, or NULL after reporting a syntax error.
static Program* parse_source(const char* source, size_t length) {
    if (setjmp(g_err.env) != 0) {
        report_error();
        return NULL;
    }
    int count;
    Token* tokens = lex(source, length, &count);
    return parse(tokens, count);  // the AST stays alive in the arena: functions point into it
}

int interpreter_run(Interpreter* in, const char* source, size_t length) {
    Program* program = parse_source(source, length);
    if (!program) return EXIT_COMPILE_ERROR;

    if (setjmp(g_err.env) != 0) {
        report_error();
        for (int i = 0; i < in->liveCount; i++) env_release(in->live[i]);
        in->liveCount = 0;
        in->env = in->globals;
        in->callDepth = 0;
        return EXIT_RUNTIME_ERROR;
    }
    for (int i = 0; i < program->count; i++) execute(in, program->stmts[i]);
    return EXIT_OK;
}
