#pragma once

#include <string>

namespace rene {

enum class TokenType {
    // Single-character tokens
    LeftParen, RightParen, LeftBrace, RightBrace, LeftBracket, RightBracket,
    Comma, Semicolon, Plus, Minus, Star, Slash, Percent,

    // One- or two-character tokens
    Equal, EqualEqual, BangEqual,
    Less, LessEqual, Greater, GreaterEqual,

    // Literals
    Identifier, String, Number,

    // Keywords
    Let, Fn, Return, If, Else, While, For, In, Break, Continue,
    Print, True, False, Nil, And, Or, Not,

    EndOfFile
};

struct Token {
    TokenType type;
    std::string lexeme;  // raw text, or the decoded value for string literals
    double number = 0.0;
    int line = 0;
};

}  // namespace rene
