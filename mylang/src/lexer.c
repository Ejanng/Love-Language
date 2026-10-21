#include "lexer.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "errors.h"

typedef struct {
    const char* src;
    size_t len;
    size_t start;
    size_t cur;
    int line;
    Token* tokens;
    int count;
    int cap;
} Lexer;

static bool at_end(const Lexer* lx) { return lx->cur >= lx->len; }
static char peek(const Lexer* lx) { return at_end(lx) ? '\0' : lx->src[lx->cur]; }
static char peek_next(const Lexer* lx) { return lx->cur + 1 >= lx->len ? '\0' : lx->src[lx->cur + 1]; }
static char advance(Lexer* lx) { return lx->src[lx->cur++]; }

static bool match(Lexer* lx, char expected) {
    if (at_end(lx) || lx->src[lx->cur] != expected) return false;
    lx->cur++;
    return true;
}

static bool is_digit(char c) { return isdigit((unsigned char)c) != 0; }
static bool is_alpha(char c) { return isalpha((unsigned char)c) != 0 || c == '_'; }

static void push_token(Lexer* lx, Token t) {
    lx->tokens = arena_array_reserve(lx->tokens, lx->count, &lx->cap, sizeof(Token));
    lx->tokens[lx->count++] = t;
}

static void add(Lexer* lx, TokenType type) {
    size_t n = lx->cur - lx->start;
    push_token(lx, (Token){type, arena_strndup(lx->src + lx->start, n), n, 0, lx->line});
}

static void string(Lexer* lx) {
    int startLine = lx->line;
    size_t cap = 16, n = 0;
    char* value = malloc(cap);
    while (!at_end(lx) && peek(lx) != '"') {
        char c = advance(lx);
        if (c == '\n') lx->line++;
        if (c == '\\' && !at_end(lx)) {  // escape sequences: \n \t \" \\ .
            char e = advance(lx);
            switch (e) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                default: c = e;  // \" and \\ map to themselves, like any other escaped character
            }
        }
        if (n + 1 >= cap) value = realloc(value, cap *= 2);
        value[n++] = c;
    }
    if (at_end(lx)) {
        free(value);
        lang_error(ERR_SYNTAX, startLine, "Unterminated string.");
    }
    advance(lx);  // closing quote
    push_token(lx, (Token){TOK_STRING, arena_strndup(value, n), n, 0, startLine});
    free(value);
}

static void number(Lexer* lx) {
    while (is_digit(peek(lx))) advance(lx);
    if (peek(lx) == '.' && is_digit(peek_next(lx))) {
        advance(lx);
        while (is_digit(peek(lx))) advance(lx);
    }
    size_t n = lx->cur - lx->start;
    const char* text = arena_strndup(lx->src + lx->start, n);
    push_token(lx, (Token){TOK_NUMBER, text, n, strtod(text, NULL), lx->line});
}

static void identifier(Lexer* lx) {
    static const struct {
        const char* word;
        TokenType type;
    } keywords[] = {
        {"and", TOK_AND},       {"else", TOK_ELSE},     {"false", TOK_FALSE}, {"fn", TOK_FN},
        {"if", TOK_IF},         {"let", TOK_LET},       {"nil", TOK_NIL},     {"or", TOK_OR},
        {"print", TOK_PRINT},   {"return", TOK_RETURN}, {"true", TOK_TRUE},   {"while", TOK_WHILE},
    };
    while (is_alpha(peek(lx)) || is_digit(peek(lx))) advance(lx);
    size_t n = lx->cur - lx->start;
    TokenType type = TOK_IDENTIFIER;
    for (size_t i = 0; i < sizeof keywords / sizeof keywords[0]; i++) {
        if (strlen(keywords[i].word) == n && memcmp(keywords[i].word, lx->src + lx->start, n) == 0) {
            type = keywords[i].type;
            break;
        }
    }
    add(lx, type);
}

static void scan_token(Lexer* lx) {
    char c = advance(lx);
    switch (c) {
        case '(': add(lx, TOK_LEFT_PAREN); break;
        case ')': add(lx, TOK_RIGHT_PAREN); break;
        case '{': add(lx, TOK_LEFT_BRACE); break;
        case '}': add(lx, TOK_RIGHT_BRACE); break;
        case ',': add(lx, TOK_COMMA); break;
        case ';': add(lx, TOK_SEMICOLON); break;
        case '+': add(lx, TOK_PLUS); break;
        case '-': add(lx, TOK_MINUS); break;
        case '*': add(lx, TOK_STAR); break;
        case '%': add(lx, TOK_PERCENT); break;
        case '!': add(lx, match(lx, '=') ? TOK_BANG_EQUAL : TOK_BANG); break;
        case '=': add(lx, match(lx, '=') ? TOK_EQUAL_EQUAL : TOK_EQUAL); break;
        case '<': add(lx, match(lx, '=') ? TOK_LESS_EQUAL : TOK_LESS); break;
        case '>': add(lx, match(lx, '=') ? TOK_GREATER_EQUAL : TOK_GREATER); break;
        case '/':
            if (match(lx, '/')) {
                while (peek(lx) != '\n' && !at_end(lx)) advance(lx);  // comment runs to end of line
            } else {
                add(lx, TOK_SLASH);
            }
            break;
        case ' ':
        case '\r':
        case '\t': break;
        case '\n': lx->line++; break;
        case '"': string(lx); break;
        default:
            if (is_digit(c)) {
                number(lx);
            } else if (is_alpha(c)) {
                identifier(lx);
            } else {
                lang_error(ERR_SYNTAX, lx->line, "Unexpected character '%c'.", c);
            }
    }
}

Token* lex(const char* source, size_t length, int* count) {
    Lexer lx = {.src = source, .len = length, .line = 1};
    while (!at_end(&lx)) {
        lx.start = lx.cur;
        scan_token(&lx);
    }
    push_token(&lx, (Token){TOK_EOF, "", 0, 0, lx.line});
    *count = lx.count;
    return lx.tokens;
}
