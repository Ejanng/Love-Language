#include "parser.h"

#include <stdbool.h>
#include <string.h>

#include "arena.h"
#include "errors.h"

typedef struct {
    const Token* tokens;
    int count;
    int current;
} Parser;

static Stmt* declaration(Parser* p);
static Stmt* statement(Parser* p);
static Expr* expression(Parser* p);
static Expr* unary(Parser* p);

// ---- token helpers ----

static const Token* peek(const Parser* p) { return &p->tokens[p->current]; }
static const Token* previous(const Parser* p) { return &p->tokens[p->current - 1]; }
static bool is_at_end(const Parser* p) { return peek(p)->type == TOK_EOF; }
static bool check(const Parser* p, TokenType t) { return !is_at_end(p) && peek(p)->type == t; }

static const Token* advance(Parser* p) {
    if (!is_at_end(p)) p->current++;
    return previous(p);
}

static bool match(Parser* p, TokenType t) {
    if (!check(p, t)) return false;
    advance(p);
    return true;
}

static bool match_any(Parser* p, const TokenType* types, int n) {
    for (int i = 0; i < n; i++) {
        if (match(p, types[i])) return true;
    }
    return false;
}

_Noreturn static void error_at(const Token* token, const char* message) {
    if (token->type == TOK_EOF) {
        lang_error(ERR_SYNTAX, token->line, "%s (reached end of file)", message);
    }
    lang_error(ERR_SYNTAX, token->line, "%s (found '%s')", message, token->lexeme);
}

static const Token* consume(Parser* p, TokenType t, const char* message) {
    if (check(p, t)) return advance(p);
    error_at(peek(p), message);
}

// ---- nodes ----

static Expr* new_expr(ExprKind kind, int line) {
    Expr* e = arena_alloc(sizeof(Expr));
    e->kind = kind;
    e->line = line;
    e->op = TOK_EOF;
    return e;
}

static Stmt* new_stmt(StmtKind kind, int line) {
    Stmt* s = arena_alloc(sizeof(Stmt));
    s->kind = kind;
    s->line = line;
    return s;
}

// ---- statements ----

// Parses statements up to and including the closing '}' of a block.
static void block_statements(Parser* p, Stmt*** body, int* bodyCount) {
    PtrVec stmts = {0};
    while (!check(p, TOK_RIGHT_BRACE) && !is_at_end(p)) ptrvec_push(&stmts, declaration(p));
    consume(p, TOK_RIGHT_BRACE, "Expect '}' after block.");
    *body = (Stmt**)stmts.items;
    *bodyCount = stmts.count;
}

static Stmt* fn_declaration(Parser* p) {
    const Token* name = consume(p, TOK_IDENTIFIER, "Expect function name after 'fn'.");
    Stmt* s = new_stmt(STMT_FUNCTION, name->line);
    s->name = name->lexeme;
    consume(p, TOK_LEFT_PAREN, "Expect '(' after function name.");
    PtrVec params = {0};
    if (!check(p, TOK_RIGHT_PAREN)) {
        do {
            if (params.count >= 255) error_at(peek(p), "Can't have more than 255 parameters.");
            ptrvec_push(&params, (void*)consume(p, TOK_IDENTIFIER, "Expect parameter name.")->lexeme);
        } while (match(p, TOK_COMMA));
    }
    s->params = (const char**)params.items;
    s->paramCount = params.count;
    consume(p, TOK_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(p, TOK_LEFT_BRACE, "Expect '{' before function body.");
    block_statements(p, &s->body, &s->bodyCount);
    return s;
}

static Stmt* let_declaration(Parser* p) {
    const Token* name = consume(p, TOK_IDENTIFIER, "Expect variable name after 'let'.");
    Stmt* s = new_stmt(STMT_LET, name->line);
    s->name = name->lexeme;
    if (match(p, TOK_EQUAL)) s->expr = expression(p);
    consume(p, TOK_SEMICOLON, "Expect ';' after variable declaration.");
    return s;
}

static Stmt* print_statement(Parser* p) {
    Stmt* s = new_stmt(STMT_PRINT, previous(p)->line);
    s->expr = expression(p);
    consume(p, TOK_SEMICOLON, "Expect ';' after value.");
    return s;
}

static Stmt* if_statement(Parser* p) {
    Stmt* s = new_stmt(STMT_IF, previous(p)->line);
    consume(p, TOK_LEFT_PAREN, "Expect '(' after 'if'.");
    s->expr = expression(p);
    consume(p, TOK_RIGHT_PAREN, "Expect ')' after condition.");
    s->thenBranch = statement(p);
    if (match(p, TOK_ELSE)) s->elseBranch = statement(p);
    return s;
}

static Stmt* while_statement(Parser* p) {
    Stmt* s = new_stmt(STMT_WHILE, previous(p)->line);
    consume(p, TOK_LEFT_PAREN, "Expect '(' after 'while'.");
    s->expr = expression(p);
    consume(p, TOK_RIGHT_PAREN, "Expect ')' after condition.");
    s->thenBranch = statement(p);
    return s;
}

static Stmt* return_statement(Parser* p) {
    Stmt* s = new_stmt(STMT_RETURN, previous(p)->line);
    if (!check(p, TOK_SEMICOLON)) s->expr = expression(p);
    consume(p, TOK_SEMICOLON, "Expect ';' after return value.");
    return s;
}

static Stmt* expression_statement(Parser* p) {
    Stmt* s = new_stmt(STMT_EXPRESSION, peek(p)->line);
    s->expr = expression(p);
    consume(p, TOK_SEMICOLON, "Expect ';' after expression.");
    return s;
}

static Stmt* statement(Parser* p) {
    if (match(p, TOK_PRINT)) return print_statement(p);
    if (match(p, TOK_IF)) return if_statement(p);
    if (match(p, TOK_WHILE)) return while_statement(p);
    if (match(p, TOK_RETURN)) return return_statement(p);
    if (match(p, TOK_LEFT_BRACE)) {
        Stmt* s = new_stmt(STMT_BLOCK, previous(p)->line);
        block_statements(p, &s->body, &s->bodyCount);
        return s;
    }
    return expression_statement(p);
}

static Stmt* declaration(Parser* p) {
    if (match(p, TOK_FN)) return fn_declaration(p);
    if (match(p, TOK_LET)) return let_declaration(p);
    return statement(p);
}

// ---- expressions ----

// Parses a left-associative chain such as a + b - c using the next-higher precedence level.
static Expr* left_assoc(Parser* p, Expr* (*next)(Parser*), const TokenType* ops, int nops, ExprKind kind) {
    Expr* expr = next(p);
    while (match_any(p, ops, nops)) {
        const Token* op = previous(p);
        Expr* right = next(p);
        Expr* e = new_expr(kind, op->line);
        e->op = op->type;
        e->left = expr;
        e->right = right;
        expr = e;
    }
    return expr;
}

static Expr* factor(Parser* p) {
    static const TokenType ops[] = {TOK_SLASH, TOK_STAR, TOK_PERCENT};
    return left_assoc(p, unary, ops, 3, EXPR_BINARY);
}

static Expr* term(Parser* p) {
    static const TokenType ops[] = {TOK_MINUS, TOK_PLUS};
    return left_assoc(p, factor, ops, 2, EXPR_BINARY);
}

static Expr* comparison(Parser* p) {
    static const TokenType ops[] = {TOK_GREATER, TOK_GREATER_EQUAL, TOK_LESS, TOK_LESS_EQUAL};
    return left_assoc(p, term, ops, 4, EXPR_BINARY);
}

static Expr* equality(Parser* p) {
    static const TokenType ops[] = {TOK_BANG_EQUAL, TOK_EQUAL_EQUAL};
    return left_assoc(p, comparison, ops, 2, EXPR_BINARY);
}

static Expr* logic_and(Parser* p) {
    static const TokenType ops[] = {TOK_AND};
    return left_assoc(p, equality, ops, 1, EXPR_LOGICAL);
}

static Expr* logic_or(Parser* p) {
    static const TokenType ops[] = {TOK_OR};
    return left_assoc(p, logic_and, ops, 1, EXPR_LOGICAL);
}

static Expr* primary(Parser* p) {
    if (match(p, TOK_NUMBER)) {
        Expr* e = new_expr(EXPR_NUMBER, previous(p)->line);
        e->number = previous(p)->number;
        return e;
    }
    if (match(p, TOK_STRING)) {
        Expr* e = new_expr(EXPR_STRING, previous(p)->line);
        e->text = previous(p)->lexeme;
        e->textLength = previous(p)->length;
        return e;
    }
    if (match(p, TOK_TRUE) || match(p, TOK_FALSE)) {
        Expr* e = new_expr(EXPR_BOOL, previous(p)->line);
        e->boolean = previous(p)->type == TOK_TRUE;
        return e;
    }
    if (match(p, TOK_NIL)) return new_expr(EXPR_NIL, previous(p)->line);
    if (match(p, TOK_IDENTIFIER)) {
        Expr* e = new_expr(EXPR_VARIABLE, previous(p)->line);
        e->text = previous(p)->lexeme;
        e->textLength = previous(p)->length;
        return e;
    }
    if (match(p, TOK_LEFT_PAREN)) {
        Expr* e = expression(p);
        consume(p, TOK_RIGHT_PAREN, "Expect ')' after expression.");
        return e;
    }
    error_at(peek(p), "Expect expression.");
}

static Expr* call(Parser* p) {
    Expr* expr = primary(p);
    while (match(p, TOK_LEFT_PAREN)) {
        Expr* c = new_expr(EXPR_CALL, previous(p)->line);
        c->left = expr;
        PtrVec args = {0};
        if (!check(p, TOK_RIGHT_PAREN)) {
            do {
                if (args.count >= 255) error_at(peek(p), "Can't have more than 255 arguments.");
                ptrvec_push(&args, expression(p));
            } while (match(p, TOK_COMMA));
        }
        c->args = (Expr**)args.items;
        c->argCount = args.count;
        consume(p, TOK_RIGHT_PAREN, "Expect ')' after arguments.");
        expr = c;
    }
    return expr;
}

static Expr* unary(Parser* p) {
    static const TokenType ops[] = {TOK_BANG, TOK_MINUS};
    if (match_any(p, ops, 2)) {
        const Token* op = previous(p);
        Expr* e = new_expr(EXPR_UNARY, op->line);
        e->op = op->type;
        e->left = unary(p);
        return e;
    }
    return call(p);
}

static Expr* assignment(Parser* p) {
    Expr* expr = logic_or(p);
    if (match(p, TOK_EQUAL)) {
        const Token* equals = previous(p);
        Expr* value = assignment(p);  // right-associative: a = b = 3
        if (expr->kind == EXPR_VARIABLE) {
            Expr* a = new_expr(EXPR_ASSIGN, equals->line);
            a->text = expr->text;
            a->textLength = expr->textLength;
            a->left = value;
            return a;
        }
        lang_error(ERR_SYNTAX, equals->line, "Invalid assignment target.");
    }
    return expr;
}

static Expr* expression(Parser* p) { return assignment(p); }

Program* parse(const Token* tokens, int count) {
    Parser p = {tokens, count, 0};
    PtrVec stmts = {0};
    while (!is_at_end(&p)) ptrvec_push(&stmts, declaration(&p));
    Program* program = arena_alloc(sizeof(Program));
    program->stmts = (Stmt**)stmts.items;
    program->count = stmts.count;
    return program;
}
