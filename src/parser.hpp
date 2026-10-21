#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "errors.hpp"
#include "token.hpp"

namespace rene {

// Recursive-descent parser. Grammar (lowest to highest precedence):
//
//   program    := declaration* EOF
//   declaration:= "let" IDENT ("=" expr)? ";" | "fn" IDENT function | statement
//   statement  := print | if | while | for | return | break | continue | block | expr ";"
//   expr       := assignment
//   assignment := (IDENT | call "[" expr "]") "=" assignment | or
//   or         := and ("or" and)*
//   and        := equality ("and" equality)*
//   equality   := comparison (("==" | "!=") comparison)*
//   comparison := term (("<" | "<=" | ">" | ">=") term)*
//   term       := factor (("+" | "-") factor)*
//   factor     := unary (("*" | "/" | "%") unary)*
//   unary      := ("-" | "not") unary | call
//   call       := primary ("(" args? ")" | "[" expr "]")*
//   primary    := NUMBER | STRING | "true" | "false" | "nil" | IDENT
//               | "(" expr ")" | "[" args? "]" | "fn" function
class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::vector<StmtPtr> parse() {
        std::vector<StmtPtr> program;
        while (!check(TokenType::EndOfFile)) program.push_back(declaration());
        return program;
    }

private:
    std::vector<Token> tokens_;
    size_t current_ = 0;
    int loopDepth_ = 0;
    int functionDepth_ = 0;

    // ------------------------------------------------------------ helpers

    const Token& peek() const { return tokens_[current_]; }
    const Token& previous() const { return tokens_[current_ - 1]; }
    bool check(TokenType type) const { return peek().type == type; }

    bool checkNext(TokenType type) const {
        return current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == type;
    }

    const Token& advance() {
        if (!check(TokenType::EndOfFile)) ++current_;
        return previous();
    }

    bool match(TokenType type) {
        if (!check(type)) return false;
        advance();
        return true;
    }

    const Token& consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        fail(peek(), message);
    }

    [[noreturn]] void fail(const Token& token, const std::string& message) const {
        std::string where = token.type == TokenType::EndOfFile ? "end of file" : "'" + token.lexeme + "'";
        throw ReneError("SyntaxError", token.line, message + " (at " + where + ")");
    }

    // -------------------------------------------------------- statements

    StmtPtr declaration() {
        if (match(TokenType::Let)) return letDeclaration();
        if (check(TokenType::Fn) && checkNext(TokenType::Identifier)) {
            advance();
            std::string name = consume(TokenType::Identifier, "expected function name").lexeme;
            return function(name);
        }
        return statement();
    }

    StmtPtr letDeclaration() {
        int line = previous().line;
        std::string name = consume(TokenType::Identifier, "expected variable name after 'let'").lexeme;
        ExprPtr initializer;
        if (match(TokenType::Equal)) initializer = expression();
        consume(TokenType::Semicolon, "expected ';' after variable declaration");
        return std::make_shared<LetStmt>(name, initializer, line);
    }

    // Parses "(params) { body }" after the `fn` keyword (and optional name).
    std::shared_ptr<FunctionStmt> function(const std::string& name) {
        int line = previous().line;
        consume(TokenType::LeftParen, "expected '(' after function name");
        std::vector<std::string> params;
        if (!check(TokenType::RightParen)) {
            do {
                params.push_back(consume(TokenType::Identifier, "expected parameter name").lexeme);
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightParen, "expected ')' after parameters");
        consume(TokenType::LeftBrace, "expected '{' before function body");

        // A function body is its own world: break/continue can't escape it.
        int savedLoopDepth = loopDepth_;
        loopDepth_ = 0;
        ++functionDepth_;
        std::vector<StmtPtr> body = blockContents();
        --functionDepth_;
        loopDepth_ = savedLoopDepth;

        return std::make_shared<FunctionStmt>(name, params, body, line);
    }

    StmtPtr statement() {
        if (match(TokenType::Print)) return printStatement();
        if (match(TokenType::If)) return ifStatement();
        if (match(TokenType::While)) return whileStatement();
        if (match(TokenType::For)) return forStatement();
        if (match(TokenType::Return)) return returnStatement();
        if (match(TokenType::Break)) return loopControl(true);
        if (match(TokenType::Continue)) return loopControl(false);
        if (match(TokenType::LeftBrace)) {
            int line = previous().line;
            return std::make_shared<BlockStmt>(blockContents(), line);
        }
        int line = peek().line;
        ExprPtr expr = expression();
        consume(TokenType::Semicolon, "expected ';' after expression");
        return std::make_shared<ExpressionStmt>(expr, line);
    }

    // Assumes '{' was already consumed; consumes the closing '}'.
    std::vector<StmtPtr> blockContents() {
        std::vector<StmtPtr> statements;
        while (!check(TokenType::RightBrace) && !check(TokenType::EndOfFile)) {
            statements.push_back(declaration());
        }
        consume(TokenType::RightBrace, "expected '}' after block");
        return statements;
    }

    StmtPtr block(const std::string& context) {
        int line = consume(TokenType::LeftBrace, "expected '{' " + context).line;
        return std::make_shared<BlockStmt>(blockContents(), line);
    }

    StmtPtr printStatement() {
        int line = previous().line;
        std::vector<ExprPtr> values;
        if (!check(TokenType::Semicolon)) {
            do {
                values.push_back(expression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::Semicolon, "expected ';' after print");
        return std::make_shared<PrintStmt>(values, line);
    }

    StmtPtr ifStatement() {
        int line = previous().line;
        ExprPtr condition = expression();
        StmtPtr thenBranch = block("after if condition");
        StmtPtr elseBranch;
        if (match(TokenType::Else)) {
            elseBranch = match(TokenType::If) ? ifStatement() : block("after 'else'");
        }
        return std::make_shared<IfStmt>(condition, thenBranch, elseBranch, line);
    }

    StmtPtr whileStatement() {
        int line = previous().line;
        ExprPtr condition = expression();
        ++loopDepth_;
        StmtPtr body = block("after while condition");
        --loopDepth_;
        return std::make_shared<WhileStmt>(condition, body, line);
    }

    StmtPtr forStatement() {
        int line = previous().line;
        std::string variable = consume(TokenType::Identifier, "expected loop variable after 'for'").lexeme;
        consume(TokenType::In, "expected 'in' after loop variable");
        ExprPtr iterable = expression();
        ++loopDepth_;
        StmtPtr body = block("after for-in expression");
        --loopDepth_;
        return std::make_shared<ForInStmt>(variable, iterable, body, line);
    }

    StmtPtr returnStatement() {
        const Token& keyword = previous();
        if (functionDepth_ == 0) fail(keyword, "'return' outside of a function");
        ExprPtr value;
        if (!check(TokenType::Semicolon)) value = expression();
        consume(TokenType::Semicolon, "expected ';' after return value");
        return std::make_shared<ReturnStmt>(value, keyword.line);
    }

    StmtPtr loopControl(bool isBreak) {
        const Token& keyword = previous();
        if (loopDepth_ == 0) fail(keyword, "'" + keyword.lexeme + "' outside of a loop");
        consume(TokenType::Semicolon, "expected ';' after '" + keyword.lexeme + "'");
        if (isBreak) return std::make_shared<BreakStmt>(keyword.line);
        return std::make_shared<ContinueStmt>(keyword.line);
    }

    // ------------------------------------------------------- expressions

    ExprPtr expression() { return assignment(); }

    ExprPtr assignment() {
        ExprPtr target = logicOr();
        if (!match(TokenType::Equal)) return target;

        const Token& equals = previous();
        ExprPtr value = assignment();
        if (auto var = dynamic_cast<VariableExpr*>(target.get())) {
            return std::make_shared<AssignExpr>(var->name, value, equals.line);
        }
        if (auto index = dynamic_cast<IndexExpr*>(target.get())) {
            return std::make_shared<IndexAssignExpr>(index->object, index->index, value, equals.line);
        }
        fail(equals, "invalid assignment target");
    }

    ExprPtr logicOr() {
        ExprPtr expr = logicAnd();
        while (match(TokenType::Or)) {
            int line = previous().line;
            expr = std::make_shared<LogicalExpr>(expr, TokenType::Or, logicAnd(), line);
        }
        return expr;
    }

    ExprPtr logicAnd() {
        ExprPtr expr = equality();
        while (match(TokenType::And)) {
            int line = previous().line;
            expr = std::make_shared<LogicalExpr>(expr, TokenType::And, equality(), line);
        }
        return expr;
    }

    ExprPtr equality() {
        ExprPtr expr = comparison();
        while (match(TokenType::EqualEqual) || match(TokenType::BangEqual)) {
            const Token& op = previous();
            expr = std::make_shared<BinaryExpr>(expr, op.type, comparison(), op.line);
        }
        return expr;
    }

    ExprPtr comparison() {
        ExprPtr expr = term();
        while (match(TokenType::Less) || match(TokenType::LessEqual) ||
               match(TokenType::Greater) || match(TokenType::GreaterEqual)) {
            const Token& op = previous();
            expr = std::make_shared<BinaryExpr>(expr, op.type, term(), op.line);
        }
        return expr;
    }

    ExprPtr term() {
        ExprPtr expr = factor();
        while (match(TokenType::Plus) || match(TokenType::Minus)) {
            const Token& op = previous();
            expr = std::make_shared<BinaryExpr>(expr, op.type, factor(), op.line);
        }
        return expr;
    }

    ExprPtr factor() {
        ExprPtr expr = unary();
        while (match(TokenType::Star) || match(TokenType::Slash) || match(TokenType::Percent)) {
            const Token& op = previous();
            expr = std::make_shared<BinaryExpr>(expr, op.type, unary(), op.line);
        }
        return expr;
    }

    ExprPtr unary() {
        if (match(TokenType::Minus) || match(TokenType::Not)) {
            const Token& op = previous();
            return std::make_shared<UnaryExpr>(op.type, unary(), op.line);
        }
        return call();
    }

    ExprPtr call() {
        ExprPtr expr = primary();
        while (true) {
            if (match(TokenType::LeftParen)) {
                int line = previous().line;
                std::vector<ExprPtr> args = arguments(TokenType::RightParen, "expected ')' after arguments");
                expr = std::make_shared<CallExpr>(expr, args, line);
            } else if (match(TokenType::LeftBracket)) {
                int line = previous().line;
                ExprPtr index = expression();
                consume(TokenType::RightBracket, "expected ']' after index");
                expr = std::make_shared<IndexExpr>(expr, index, line);
            } else {
                return expr;
            }
        }
    }

    // Comma-separated expressions up to (and including) `closer`.
    std::vector<ExprPtr> arguments(TokenType closer, const std::string& message) {
        std::vector<ExprPtr> args;
        if (!check(closer)) {
            do {
                args.push_back(expression());
            } while (match(TokenType::Comma));
        }
        consume(closer, message);
        return args;
    }

    ExprPtr primary() {
        const Token& token = advance();
        switch (token.type) {
            case TokenType::Number: return std::make_shared<LiteralExpr>(Value{token.number}, token.line);
            case TokenType::String: return std::make_shared<LiteralExpr>(Value{token.lexeme}, token.line);
            case TokenType::True: return std::make_shared<LiteralExpr>(Value{true}, token.line);
            case TokenType::False: return std::make_shared<LiteralExpr>(Value{false}, token.line);
            case TokenType::Nil: return std::make_shared<LiteralExpr>(Value{}, token.line);
            case TokenType::Identifier: return std::make_shared<VariableExpr>(token.lexeme, token.line);
            case TokenType::LeftParen: {
                ExprPtr expr = expression();
                consume(TokenType::RightParen, "expected ')' after expression");
                return expr;
            }
            case TokenType::LeftBracket: {
                std::vector<ExprPtr> elements = arguments(TokenType::RightBracket, "expected ']' after list");
                return std::make_shared<ListExpr>(elements, token.line);
            }
            case TokenType::Fn: return std::make_shared<LambdaExpr>(function("<anonymous>"), token.line);
            default: fail(token, "expected an expression");
        }
    }
};

}  // namespace rene
