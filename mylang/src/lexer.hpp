#pragma once
// Stage 1: the lexer turns source text into a flat list of tokens.
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "errors.hpp"
#include "token.hpp"

class Lexer {
public:
    explicit Lexer(std::string source) : src_(std::move(source)) {}

    std::vector<Token> scanTokens() {
        while (!atEnd()) {
            start_ = cur_;
            scanToken();
        }
        tokens_.push_back(Token{TokenType::EndOfFile, "", 0, line_});
        return tokens_;
    }

private:
    std::string src_;
    std::vector<Token> tokens_;
    size_t start_ = 0;
    size_t cur_ = 0;
    int line_ = 1;

    bool atEnd() const { return cur_ >= src_.size(); }
    char peek() const { return atEnd() ? '\0' : src_[cur_]; }
    char peekNext() const { return cur_ + 1 >= src_.size() ? '\0' : src_[cur_ + 1]; }
    char advance() { return src_[cur_++]; }

    bool match(char expected) {
        if (atEnd() || src_[cur_] != expected) return false;
        cur_++;
        return true;
    }

    static bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
    static bool isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }

    void add(TokenType type) {
        tokens_.push_back(Token{type, src_.substr(start_, cur_ - start_), 0, line_});
    }

    void scanToken() {
        char c = advance();
        switch (c) {
            case '(': add(TokenType::LeftParen); break;
            case ')': add(TokenType::RightParen); break;
            case '{': add(TokenType::LeftBrace); break;
            case '}': add(TokenType::RightBrace); break;
            case ',': add(TokenType::Comma); break;
            case ';': add(TokenType::Semicolon); break;
            case '+': add(TokenType::Plus); break;
            case '-': add(TokenType::Minus); break;
            case '*': add(TokenType::Star); break;
            case '%': add(TokenType::Percent); break;
            case '!': add(match('=') ? TokenType::BangEqual : TokenType::Bang); break;
            case '=': add(match('=') ? TokenType::EqualEqual : TokenType::Equal); break;
            case '<': add(match('=') ? TokenType::LessEqual : TokenType::Less); break;
            case '>': add(match('=') ? TokenType::GreaterEqual : TokenType::Greater); break;
            case '/':
                if (match('/')) {
                    while (peek() != '\n' && !atEnd()) advance();  // comment runs to end of line
                } else {
                    add(TokenType::Slash);
                }
                break;
            case ' ':
            case '\r':
            case '\t': break;
            case '\n': line_++; break;
            case '"': string(); break;
            default:
                if (isDigit(c)) {
                    number();
                } else if (isAlpha(c)) {
                    identifier();
                } else {
                    throw SyntaxError(line_, std::string("Unexpected character '") + c + "'.");
                }
        }
    }

    void string() {
        int startLine = line_;
        std::string value;
        while (!atEnd() && peek() != '"') {
            char c = advance();
            if (c == '\n') line_++;
            if (c == '\\' && !atEnd()) {  // escape sequences: \n \t \" \\ .
                char e = advance();
                switch (e) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case '"': value += '"'; break;
                    case '\\': value += '\\'; break;
                    default: value += e;
                }
            } else {
                value += c;
            }
        }
        if (atEnd()) throw SyntaxError(startLine, "Unterminated string.");
        advance();  // closing quote
        tokens_.push_back(Token{TokenType::String, value, 0, startLine});
    }

    void number() {
        while (isDigit(peek())) advance();
        if (peek() == '.' && isDigit(peekNext())) {
            advance();
            while (isDigit(peek())) advance();
        }
        std::string text = src_.substr(start_, cur_ - start_);
        tokens_.push_back(Token{TokenType::Number, text, std::stod(text), line_});
    }

    void identifier() {
        while (isAlpha(peek()) || isDigit(peek())) advance();
        static const std::unordered_map<std::string, TokenType> keywords = {
            {"and", TokenType::And},       {"else", TokenType::Else},     {"false", TokenType::False},
            {"fn", TokenType::Fn},         {"if", TokenType::If},         {"let", TokenType::Let},
            {"nil", TokenType::Nil},       {"or", TokenType::Or},         {"print", TokenType::Print},
            {"return", TokenType::Return}, {"true", TokenType::True},     {"while", TokenType::While},
        };
        std::string text = src_.substr(start_, cur_ - start_);
        auto it = keywords.find(text);
        add(it == keywords.end() ? TokenType::Identifier : it->second);
    }
};
