#pragma once
#include <stddef.h>

// Every kind of token the lexer can produce.
typedef enum {
    // Single-character tokens
    TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_LEFT_BRACE, TOK_RIGHT_BRACE, TOK_COMMA, TOK_SEMICOLON,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    // One or two character tokens
    TOK_BANG, TOK_BANG_EQUAL, TOK_EQUAL, TOK_EQUAL_EQUAL, TOK_LESS, TOK_LESS_EQUAL, TOK_GREATER, TOK_GREATER_EQUAL,
    // Literals and names
    TOK_IDENTIFIER, TOK_STRING, TOK_NUMBER,
    // Keywords
    TOK_AND, TOK_ELSE, TOK_FALSE, TOK_FN, TOK_IF, TOK_LET, TOK_NIL, TOK_OR, TOK_PRINT, TOK_RETURN, TOK_TRUE, TOK_WHILE,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* lexeme;  // '\0'-terminated source text; for TOK_STRING, the text without quotes
    size_t length;       // length of lexeme (a string literal may contain '\0')
    double number;       // value of a TOK_NUMBER token
    int line;
} Token;
