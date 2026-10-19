#pragma once
// Stage 2: a recursive descent parser turns tokens into an AST.
//
// Grammar (lowest precedence first):
//   program     -> declaration* EOF
//   declaration -> fnDecl | letDecl | statement
//   fnDecl      -> "fn" IDENT "(" params? ")" block
//   letDecl     -> "let" IDENT ( "=" expression )? ";"
//   statement   -> print | if | while | return | block | exprStmt
//   expression  -> assignment
//   assignment  -> IDENT "=" assignment | logicOr
//   logicOr     -> logicAnd ( "or" logicAnd )*
//   logicAnd    -> equality ( "and" equality )*
//   equality    -> comparison ( ( "!=" | "==" ) comparison )*
//   comparison  -> term ( ( ">" | ">=" | "<" | "<=" ) term )*
//   term        -> factor ( ( "-" | "+" ) factor )*
//   factor      -> unary ( ( "/" | "*" | "%" ) unary )*
//   unary       -> ( "!" | "-" ) unary | call
//   call        -> primary ( "(" arguments? ")" )*
//   primary     -> NUMBER | STRING | "true" | "false" | "nil" | IDENT | "(" expression ")"
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "errors.hpp"
#include "token.hpp"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::vector<StmtPtr> parse() {
        std::vector<StmtPtr> program;
        while (!isAtEnd()) program.push_back(declaration());
        return program;
    }

private:
    std::vector<Token> tokens_;
    size_t current_ = 0;

    // ---- statements ----

    StmtPtr declaration() {
        if (match(TokenType::Fn)) return fnDeclaration();
        if (match(TokenType::Let)) return letDeclaration();
        return statement();
    }

    StmtPtr fnDeclaration() {
        Token name = consume(TokenType::Identifier, "Expect function name after 'fn'.");
        auto s = std::make_unique<Stmt>(Stmt::Kind::Function, name.line);
        s->name = name.lexeme;
        consume(TokenType::LeftParen, "Expect '(' after function name.");
        if (!check(TokenType::RightParen)) {
            do {
                if (s->params.size() >= 255) throw error(peek(), "Can't have more than 255 parameters.");
                s->params.push_back(consume(TokenType::Identifier, "Expect parameter name.").lexeme);
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightParen, "Expect ')' after parameters.");
        consume(TokenType::LeftBrace, "Expect '{' before function body.");
        s->body = blockStatements();
        return s;
    }

    StmtPtr letDeclaration() {
        Token name = consume(TokenType::Identifier, "Expect variable name after 'let'.");
        auto s = std::make_unique<Stmt>(Stmt::Kind::Let, name.line);
        s->name = name.lexeme;
        if (match(TokenType::Equal)) s->expr = expression();
        consume(TokenType::Semicolon, "Expect ';' after variable declaration.");
        return s;
    }

    StmtPtr statement() {
        if (match(TokenType::Print)) return printStatement();
        if (match(TokenType::If)) return ifStatement();
        if (match(TokenType::While)) return whileStatement();
        if (match(TokenType::Return)) return returnStatement();
        if (match(TokenType::LeftBrace)) {
            auto s = std::make_unique<Stmt>(Stmt::Kind::Block, previous().line);
            s->body = blockStatements();
            return s;
        }
        return expressionStatement();
    }

    StmtPtr printStatement() {
        auto s = std::make_unique<Stmt>(Stmt::Kind::Print, previous().line);
        s->expr = expression();
        consume(TokenType::Semicolon, "Expect ';' after value.");
        return s;
    }

    StmtPtr ifStatement() {
        auto s = std::make_unique<Stmt>(Stmt::Kind::If, previous().line);
        consume(TokenType::LeftParen, "Expect '(' after 'if'.");
        s->expr = expression();
        consume(TokenType::RightParen, "Expect ')' after condition.");
        s->thenBranch = statement();
        if (match(TokenType::Else)) s->elseBranch = statement();
        return s;
    }

    StmtPtr whileStatement() {
        auto s = std::make_unique<Stmt>(Stmt::Kind::While, previous().line);
        consume(TokenType::LeftParen, "Expect '(' after 'while'.");
        s->expr = expression();
        consume(TokenType::RightParen, "Expect ')' after condition.");
        s->thenBranch = statement();
        return s;
    }

    StmtPtr returnStatement() {
        auto s = std::make_unique<Stmt>(Stmt::Kind::Return, previous().line);
        if (!check(TokenType::Semicolon)) s->expr = expression();
        consume(TokenType::Semicolon, "Expect ';' after return value.");
        return s;
    }

    StmtPtr expressionStatement() {
        auto s = std::make_unique<Stmt>(Stmt::Kind::Expression, peek().line);
        s->expr = expression();
        consume(TokenType::Semicolon, "Expect ';' after expression.");
        return s;
    }

    std::vector<StmtPtr> blockStatements() {
        std::vector<StmtPtr> stmts;
        while (!check(TokenType::RightBrace) && !isAtEnd()) stmts.push_back(declaration());
        consume(TokenType::RightBrace, "Expect '}' after block.");
        return stmts;
    }

    // ---- expressions ----

    ExprPtr expression() { return assignment(); }

    ExprPtr assignment() {
        ExprPtr expr = logicOr();
        if (match(TokenType::Equal)) {
            Token equals = previous();
            ExprPtr value = assignment();  // right-associative: a = b = 3
            if (expr->kind == Expr::Kind::Variable) {
                auto a = std::make_unique<Expr>(Expr::Kind::Assign, equals.line);
                a->text = expr->text;
                a->left = std::move(value);
                return a;
            }
            throw SyntaxError(equals.line, "Invalid assignment target.");
        }
        return expr;
    }

    // Parses a left-associative chain such as a + b - c using the next-higher precedence level.
    ExprPtr leftAssoc(ExprPtr (Parser::*next)(), std::initializer_list<TokenType> ops,
                      Expr::Kind kind = Expr::Kind::Binary) {
        ExprPtr expr = (this->*next)();
        while (matchAny(ops)) {
            Token op = previous();
            ExprPtr right = (this->*next)();
            auto e = std::make_unique<Expr>(kind, op.line);
            e->op = op.type;
            e->left = std::move(expr);
            e->right = std::move(right);
            expr = std::move(e);
        }
        return expr;
    }

    ExprPtr logicOr() { return leftAssoc(&Parser::logicAnd, {TokenType::Or}, Expr::Kind::Logical); }
    ExprPtr logicAnd() { return leftAssoc(&Parser::equality, {TokenType::And}, Expr::Kind::Logical); }
    ExprPtr equality() { return leftAssoc(&Parser::comparison, {TokenType::BangEqual, TokenType::EqualEqual}); }
    ExprPtr comparison() {
        return leftAssoc(&Parser::term, {TokenType::Greater, TokenType::GreaterEqual, TokenType::Less, TokenType::LessEqual});
    }
    ExprPtr term() { return leftAssoc(&Parser::factor, {TokenType::Minus, TokenType::Plus}); }
    ExprPtr factor() { return leftAssoc(&Parser::unary, {TokenType::Slash, TokenType::Star, TokenType::Percent}); }

    ExprPtr unary() {
        if (matchAny({TokenType::Bang, TokenType::Minus})) {
            Token op = previous();
            auto e = std::make_unique<Expr>(Expr::Kind::Unary, op.line);
            e->op = op.type;
            e->left = unary();
            return e;
        }
        return call();
    }

    ExprPtr call() {
        ExprPtr expr = primary();
        while (match(TokenType::LeftParen)) {
            auto c = std::make_unique<Expr>(Expr::Kind::Call, previous().line);
            c->left = std::move(expr);
            if (!check(TokenType::RightParen)) {
                do {
                    if (c->args.size() >= 255) throw error(peek(), "Can't have more than 255 arguments.");
                    c->args.push_back(expression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RightParen, "Expect ')' after arguments.");
            expr = std::move(c);
        }
        return expr;
    }

    ExprPtr primary() {
        if (match(TokenType::Number)) {
            auto e = std::make_unique<Expr>(Expr::Kind::Number, previous().line);
            e->number = previous().number;
            return e;
        }
        if (match(TokenType::String)) {
            auto e = std::make_unique<Expr>(Expr::Kind::String, previous().line);
            e->text = previous().lexeme;
            return e;
        }
        if (match(TokenType::True) || match(TokenType::False)) {
            auto e = std::make_unique<Expr>(Expr::Kind::Bool, previous().line);
            e->boolean = previous().type == TokenType::True;
            return e;
        }
        if (match(TokenType::Nil)) return std::make_unique<Expr>(Expr::Kind::Nil, previous().line);
        if (match(TokenType::Identifier)) {
            auto e = std::make_unique<Expr>(Expr::Kind::Variable, previous().line);
            e->text = previous().lexeme;
            return e;
        }
        if (match(TokenType::LeftParen)) {
            ExprPtr e = expression();
            consume(TokenType::RightParen, "Expect ')' after expression.");
            return e;
        }
        throw error(peek(), "Expect expression.");
    }

    // ---- token helpers ----

    const Token& peek() const { return tokens_[current_]; }
    const Token& previous() const { return tokens_[current_ - 1]; }
    bool isAtEnd() const { return peek().type == TokenType::EndOfFile; }
    bool check(TokenType t) const { return !isAtEnd() && peek().type == t; }

    const Token& advance() {
        if (!isAtEnd()) current_++;
        return previous();
    }

    bool match(TokenType t) {
        if (!check(t)) return false;
        advance();
        return true;
    }

    bool matchAny(std::initializer_list<TokenType> types) {
        for (TokenType t : types) {
            if (match(t)) return true;
        }
        return false;
    }

    const Token& consume(TokenType t, const std::string& message) {
        if (check(t)) return advance();
        throw error(peek(), message);
    }

    static SyntaxError error(const Token& token, const std::string& message) {
        std::string found = token.type == TokenType::EndOfFile ? "reached end of file" : "found '" + token.lexeme + "'";
        return SyntaxError(token.line, message + " (" + found + ")");
    }
};
