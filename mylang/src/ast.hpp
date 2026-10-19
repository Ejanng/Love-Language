#pragma once
// The abstract syntax tree (AST): what the parser produces and the interpreter walks.
// Each node is one struct with a `kind` tag. It is compact and easy to read.
// A good later experiment is replacing this with std::variant or a visitor.
#include <memory>
#include <string>
#include <vector>

#include "token.hpp"

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct Expr {
    enum class Kind { Number, String, Bool, Nil, Variable, Assign, Unary, Binary, Logical, Call };

    Kind kind;
    int line = 0;
    double number = 0;          // Number
    bool boolean = false;       // Bool
    std::string text;           // String literal, or the name for Variable and Assign
    TokenType op = TokenType::EndOfFile;  // Unary, Binary, Logical
    ExprPtr left;               // Unary operand, Binary/Logical left, Assign value, Call callee
    ExprPtr right;              // Binary/Logical right
    std::vector<ExprPtr> args;  // Call arguments

    Expr(Kind k, int ln) : kind(k), line(ln) {}
};

struct Stmt {
    enum class Kind { Expression, Print, Let, Block, If, While, Function, Return };

    Kind kind;
    int line = 0;
    std::string name;                 // Let, Function
    ExprPtr expr;                     // Expression, Print, Let initializer, If/While condition, Return value (may be null)
    std::vector<StmtPtr> body;        // Block statements, Function body
    StmtPtr thenBranch;               // If "then" branch, While body
    StmtPtr elseBranch;               // If "else" branch (may be null)
    std::vector<std::string> params;  // Function parameters

    Stmt(Kind k, int ln) : kind(k), line(ln) {}
};
