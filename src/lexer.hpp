#pragma once

#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "errors.hpp"
#include "token.hpp"

namespace rene {

// Turns Rene source text into a flat list of tokens.
class Lexer {
public:
    explicit Lexer(std::string source) : source_(std::move(source)) {}

    std::vector<Token> tokenize() {
        while (!atEnd()) {
            start_ = current_;
            scanToken();
        }
        tokens_.push_back({TokenType::EndOfFile, "", 0.0, line_});
        return tokens_;
    }

private:
    std::string source_;
    std::vector<Token> tokens_;
    size_t start_ = 0;
    size_t current_ = 0;
    int line_ = 1;

    bool atEnd() const { return current_ >= source_.size(); }
    char advance() { return source_[current_++]; }
    char peek() const { return atEnd() ? '\0' : source_[current_]; }
    char peekNext() const { return current_ + 1 >= source_.size() ? '\0' : source_[current_ + 1]; }

    bool match(char expected) {
        if (atEnd() || source_[current_] != expected) return false;
        ++current_;
        return true;
    }

    void add(TokenType type, double number = 0.0) {
        tokens_.push_back({type, source_.substr(start_, current_ - start_), number, line_});
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw ReneError("SyntaxError", line_, message);
    }

    void scanToken() {
        char c = advance();
        switch (c) {
            case '(': add(TokenType::LeftParen); break;
            case ')': add(TokenType::RightParen); break;
            case '{': add(TokenType::LeftBrace); break;
            case '}': add(TokenType::RightBrace); break;
            case '[': add(TokenType::LeftBracket); break;
            case ']': add(TokenType::RightBracket); break;
            case ',': add(TokenType::Comma); break;
            case ';': add(TokenType::Semicolon); break;
            case '+': add(TokenType::Plus); break;
            case '-': add(TokenType::Minus); break;
            case '*': add(TokenType::Star); break;
            case '%': add(TokenType::Percent); break;
            case '=': add(match('=') ? TokenType::EqualEqual : TokenType::Equal); break;
            case '<': add(match('=') ? TokenType::LessEqual : TokenType::Less); break;
            case '>': add(match('=') ? TokenType::GreaterEqual : TokenType::Greater); break;
            case '!':
                if (!match('=')) fail("unexpected '!' (use 'not' for negation)");
                add(TokenType::BangEqual);
                break;
            case '/':
                if (match('/')) {
                    while (peek() != '\n' && !atEnd()) advance();  // line comment
                } else {
                    add(TokenType::Slash);
                }
                break;
            case ' ': case '\r': case '\t': break;
            case '\n': ++line_; break;
            case '"': string(); break;
            default:
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    number();
                } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                    identifier();
                } else {
                    fail(std::string("unexpected character '") + c + "'");
                }
        }
    }

    void string() {
        int startLine = line_;
        std::string value;
        while (peek() != '"' && !atEnd()) {
            char c = advance();
            if (c == '\n') ++line_;
            if (c != '\\') {
                value += c;
                continue;
            }
            if (atEnd()) break;
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case '"': value += '"'; break;
                case '\\': value += '\\'; break;
                default: fail(std::string("unknown escape sequence '\\") + escaped + "'");
            }
        }
        if (atEnd()) throw ReneError("SyntaxError", startLine, "unterminated string");
        advance();  // closing quote
        tokens_.push_back({TokenType::String, value, 0.0, startLine});
    }

    void number() {
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        add(TokenType::Number, std::stod(source_.substr(start_, current_ - start_)));
    }

    void identifier() {
        static const std::unordered_map<std::string, TokenType> keywords = {
            {"let", TokenType::Let},       {"fn", TokenType::Fn},
            {"return", TokenType::Return}, {"if", TokenType::If},
            {"else", TokenType::Else},     {"while", TokenType::While},
            {"for", TokenType::For},       {"in", TokenType::In},
            {"break", TokenType::Break},   {"continue", TokenType::Continue},
            {"print", TokenType::Print},   {"true", TokenType::True},
            {"false", TokenType::False},   {"nil", TokenType::Nil},
            {"and", TokenType::And},       {"or", TokenType::Or},
            {"not", TokenType::Not},
        };
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
        auto it = keywords.find(source_.substr(start_, current_ - start_));
        add(it == keywords.end() ? TokenType::Identifier : it->second);
    }
};

}  // namespace rene
