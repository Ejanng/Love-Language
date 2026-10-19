#pragma once
#include <string>

// Every kind of token the lexer can produce.
enum class TokenType {
    // Single-character tokens
    LeftParen, RightParen, LeftBrace, RightBrace, Comma, Semicolon,
    Plus, Minus, Star, Slash, Percent,
    // One or two character tokens
    Bang, BangEqual, Equal, EqualEqual, Less, LessEqual, Greater, GreaterEqual,
    // Literals and names
    Identifier, String, Number,
    // Keywords
    And, Else, False, Fn, If, Let, Nil, Or, Print, Return, True, While,
    EndOfFile
};

struct Token {
    TokenType type;
    std::string lexeme;  // source text; for String tokens, the text without quotes
    double number = 0;   // value of a Number token
    int line = 1;
};
