#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "token.hpp"
#include "value.hpp"

namespace rene {

// ---------------------------------------------------------------- Expressions

struct Expr {
    int line = 0;
    explicit Expr(int l) : line(l) {}
    virtual ~Expr() = default;
};
using ExprPtr = std::shared_ptr<Expr>;

struct Stmt {
    int line = 0;
    explicit Stmt(int l) : line(l) {}
    virtual ~Stmt() = default;
};
using StmtPtr = std::shared_ptr<Stmt>;

struct LiteralExpr : Expr {
    Value value;
    LiteralExpr(Value v, int l) : Expr(l), value(std::move(v)) {}
};

struct VariableExpr : Expr {
    std::string name;
    VariableExpr(std::string n, int l) : Expr(l), name(std::move(n)) {}
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr value;
    AssignExpr(std::string n, ExprPtr v, int l) : Expr(l), name(std::move(n)), value(std::move(v)) {}
};

struct UnaryExpr : Expr {
    TokenType op;
    ExprPtr right;
    UnaryExpr(TokenType o, ExprPtr r, int l) : Expr(l), op(o), right(std::move(r)) {}
};

struct BinaryExpr : Expr {
    ExprPtr left;
    TokenType op;
    ExprPtr right;
    BinaryExpr(ExprPtr a, TokenType o, ExprPtr b, int l)
        : Expr(l), left(std::move(a)), op(o), right(std::move(b)) {}
};

// `and` / `or` short-circuit, so they get their own node.
struct LogicalExpr : Expr {
    ExprPtr left;
    TokenType op;
    ExprPtr right;
    LogicalExpr(ExprPtr a, TokenType o, ExprPtr b, int l)
        : Expr(l), left(std::move(a)), op(o), right(std::move(b)) {}
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    CallExpr(ExprPtr c, std::vector<ExprPtr> a, int l) : Expr(l), callee(std::move(c)), args(std::move(a)) {}
};

struct ListExpr : Expr {
    std::vector<ExprPtr> elements;
    ListExpr(std::vector<ExprPtr> e, int l) : Expr(l), elements(std::move(e)) {}
};

struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    IndexExpr(ExprPtr o, ExprPtr i, int l) : Expr(l), object(std::move(o)), index(std::move(i)) {}
};

struct IndexAssignExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    ExprPtr value;
    IndexAssignExpr(ExprPtr o, ExprPtr i, ExprPtr v, int l)
        : Expr(l), object(std::move(o)), index(std::move(i)), value(std::move(v)) {}
};

struct FunctionStmt;

// Anonymous function: fn (a, b) { ... }
struct LambdaExpr : Expr {
    std::shared_ptr<FunctionStmt> function;
    LambdaExpr(std::shared_ptr<FunctionStmt> f, int l) : Expr(l), function(std::move(f)) {}
};

// ----------------------------------------------------------------- Statements

struct ExpressionStmt : Stmt {
    ExprPtr expression;
    ExpressionStmt(ExprPtr e, int l) : Stmt(l), expression(std::move(e)) {}
};

struct PrintStmt : Stmt {
    std::vector<ExprPtr> values;
    PrintStmt(std::vector<ExprPtr> v, int l) : Stmt(l), values(std::move(v)) {}
};

struct LetStmt : Stmt {
    std::string name;
    ExprPtr initializer;  // may be null -> nil
    LetStmt(std::string n, ExprPtr i, int l) : Stmt(l), name(std::move(n)), initializer(std::move(i)) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    BlockStmt(std::vector<StmtPtr> s, int l) : Stmt(l), statements(std::move(s)) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;  // may be null
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e, int l)
        : Stmt(l), condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b, int l) : Stmt(l), condition(std::move(c)), body(std::move(b)) {}
};

// for item in iterable { ... }
struct ForInStmt : Stmt {
    std::string variable;
    ExprPtr iterable;
    StmtPtr body;
    ForInStmt(std::string v, ExprPtr i, StmtPtr b, int l)
        : Stmt(l), variable(std::move(v)), iterable(std::move(i)), body(std::move(b)) {}
};

struct BreakStmt : Stmt {
    explicit BreakStmt(int l) : Stmt(l) {}
};

struct ContinueStmt : Stmt {
    explicit ContinueStmt(int l) : Stmt(l) {}
};

struct FunctionStmt : Stmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    FunctionStmt(std::string n, std::vector<std::string> p, std::vector<StmtPtr> b, int l)
        : Stmt(l), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value;  // may be null -> nil
    ReturnStmt(ExprPtr v, int l) : Stmt(l), value(std::move(v)) {}
};

}  // namespace rene
