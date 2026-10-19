#pragma once
// Stage 3: a tree-walking interpreter. It evaluates the AST directly.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "errors.hpp"
#include "lexer.hpp"
#include "parser.hpp"

struct Function;

// A runtime value: nil, number, boolean, string, or function.
// Always build string values with std::string (a bare "text" would convert to bool).
using Value = std::variant<std::monostate, double, bool, std::string, std::shared_ptr<Function>>;

// A scope: a table of variables plus a link to the enclosing scope.
class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr) : parent_(std::move(parent)) {}

    void define(const std::string& name, Value value) { vars_[name] = std::move(value); }

    bool assign(const std::string& name, const Value& value) {
        auto it = vars_.find(name);
        if (it != vars_.end()) {
            it->second = value;
            return true;
        }
        return parent_ && parent_->assign(name, value);
    }

    const Value* lookup(const std::string& name) const {
        auto it = vars_.find(name);
        if (it != vars_.end()) return &it->second;
        return parent_ ? parent_->lookup(name) : nullptr;
    }

private:
    std::unordered_map<std::string, Value> vars_;
    std::shared_ptr<Environment> parent_;
};

// A user-defined function. `closure` is the scope it was defined in, which is what makes closures work.
// Note: a function stored in its own closure scope forms a reference cycle and is never freed.
// Fixing that properly is what a garbage collector is for.
struct Function {
    const Stmt* decl;
    std::shared_ptr<Environment> closure;
};

class Interpreter {
public:
    Interpreter() : globals_(std::make_shared<Environment>()), env_(globals_) {}

    // Runs a piece of source. Returns 0 on success, 65 for a syntax error, 70 for a runtime error.
    // Global variables and functions persist between calls, which is what the REPL relies on.
    int run(const std::string& source) {
        try {
            Lexer lexer(source);
            Parser parser(lexer.scanTokens());
            // Keep every program's AST alive: functions point into it.
            programs_.push_back(parser.parse());
        } catch (const SyntaxError& e) {
            report("Syntax error", e);
            return 65;
        }
        try {
            for (const StmtPtr& s : programs_.back()) execute(*s);
        } catch (const RuntimeError& e) {
            report("Runtime error", e);
            env_ = globals_;
            callDepth_ = 0;
            return 70;
        }
        return 0;
    }

private:
    static constexpr int kMaxCallDepth = 1000;

    struct ReturnSignal {
        Value value;
    };

    // Swaps the current scope for the duration of a block, restoring it even if an error is thrown.
    struct ScopeGuard {
        Interpreter& in;
        std::shared_ptr<Environment> saved;
        ScopeGuard(Interpreter& i, std::shared_ptr<Environment> next) : in(i), saved(i.env_) {
            in.env_ = std::move(next);
        }
        ~ScopeGuard() { in.env_ = saved; }
    };

    struct DepthGuard {
        int& depth;
        explicit DepthGuard(int& d) : depth(d) { ++depth; }
        ~DepthGuard() { --depth; }
    };

    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> env_;
    std::vector<std::vector<StmtPtr>> programs_;
    int callDepth_ = 0;

    static void report(const char* kind, const LangError& e) {
        std::cout.flush();  // keep program output ahead of the error message when both go to one stream
        std::cerr <<"[line " << e.line << "] " << kind << ": " << e.what() << "\n";
    }

    // ---- statements ----

    void execute(const Stmt& s) {
        switch (s.kind) {
            case Stmt::Kind::Expression:
                evaluate(*s.expr);
                break;
            case Stmt::Kind::Print:
                std::cout << stringify(evaluate(*s.expr)) << "\n";
                break;
            case Stmt::Kind::Let:
                env_->define(s.name, s.expr ? evaluate(*s.expr) : Value{});
                break;
            case Stmt::Kind::Block:
                executeBlock(s.body, std::make_shared<Environment>(env_));
                break;
            case Stmt::Kind::If:
                if (truthy(evaluate(*s.expr))) {
                    execute(*s.thenBranch);
                } else if (s.elseBranch) {
                    execute(*s.elseBranch);
                }
                break;
            case Stmt::Kind::While:
                while (truthy(evaluate(*s.expr))) execute(*s.thenBranch);
                break;
            case Stmt::Kind::Function:
                env_->define(s.name, std::make_shared<Function>(Function{&s, env_}));
                break;
            case Stmt::Kind::Return: {
                if (callDepth_ == 0) throw RuntimeError(s.line, "Can't return from top-level code.");
                throw ReturnSignal{s.expr ? evaluate(*s.expr) : Value{}};
            }
        }
    }

    void executeBlock(const std::vector<StmtPtr>& stmts, std::shared_ptr<Environment> scope) {
        ScopeGuard guard(*this, std::move(scope));
        for (const StmtPtr& s : stmts) execute(*s);
    }

    // ---- expressions ----

    Value evaluate(const Expr& e) {
        switch (e.kind) {
            case Expr::Kind::Number: return e.number;
            case Expr::Kind::String: return e.text;
            case Expr::Kind::Bool: return e.boolean;
            case Expr::Kind::Nil: return Value{};
            case Expr::Kind::Variable: {
                const Value* v = env_->lookup(e.text);
                if (!v) throw RuntimeError(e.line, "Undefined variable '" + e.text + "'.");
                return *v;
            }
            case Expr::Kind::Assign: {
                Value v = evaluate(*e.left);
                if (!env_->assign(e.text, v)) throw RuntimeError(e.line, "Undefined variable '" + e.text + "'.");
                return v;
            }
            case Expr::Kind::Unary: return evalUnary(e);
            case Expr::Kind::Binary: return evalBinary(e);
            case Expr::Kind::Logical: {
                Value left = evaluate(*e.left);
                if (e.op == TokenType::Or) {
                    if (truthy(left)) return left;
                } else {
                    if (!truthy(left)) return left;
                }
                return evaluate(*e.right);
            }
            case Expr::Kind::Call: return evalCall(e);
        }
        return Value{};
    }

    Value evalUnary(const Expr& e) {
        Value operand = evaluate(*e.left);
        if (e.op == TokenType::Bang) return !truthy(operand);
        const double* n = std::get_if<double>(&operand);
        if (!n) throw RuntimeError(e.line, "Operand of '-' must be a number.");
        return -*n;
    }

    Value evalBinary(const Expr& e) {
        Value l = evaluate(*e.left);
        Value r = evaluate(*e.right);

        if (e.op == TokenType::EqualEqual) return isEqual(l, r);
        if (e.op == TokenType::BangEqual) return !isEqual(l, r);

        if (e.op == TokenType::Plus) {
            const double* a = std::get_if<double>(&l);
            const double* b = std::get_if<double>(&r);
            if (a && b) return *a + *b;
            if (std::holds_alternative<std::string>(l) || std::holds_alternative<std::string>(r)) {
                return stringify(l) + stringify(r);  // string + anything joins them as text
            }
            throw RuntimeError(e.line, "Operands of '+' must be two numbers or include a string.");
        }

        const double* a = std::get_if<double>(&l);
        const double* b = std::get_if<double>(&r);
        if (!a || !b) throw RuntimeError(e.line, "Operands must be numbers.");

        switch (e.op) {
            case TokenType::Minus: return *a - *b;
            case TokenType::Star: return *a * *b;
            case TokenType::Slash:
                if (*b == 0) throw RuntimeError(e.line, "Division by zero.");
                return *a / *b;
            case TokenType::Percent:
                if (*b == 0) throw RuntimeError(e.line, "Division by zero.");
                return std::fmod(*a, *b);
            case TokenType::Greater: return *a > *b;
            case TokenType::GreaterEqual: return *a >= *b;
            case TokenType::Less: return *a < *b;
            case TokenType::LessEqual: return *a <= *b;
            default: break;
        }
        throw RuntimeError(e.line, "Unknown operator.");
    }

    Value evalCall(const Expr& e) {
        Value callee = evaluate(*e.left);
        std::vector<Value> args;
        args.reserve(e.args.size());
        for (const ExprPtr& a : e.args) args.push_back(evaluate(*a));

        auto* fnPtr = std::get_if<std::shared_ptr<Function>>(&callee);
        if (!fnPtr) throw RuntimeError(e.line, "Can only call functions.");
        const Function& fn = **fnPtr;

        if (args.size() != fn.decl->params.size()) {
            throw RuntimeError(e.line, "Expected " + std::to_string(fn.decl->params.size()) + " arguments but got " +
                                           std::to_string(args.size()) + ".");
        }
        if (callDepth_ >= kMaxCallDepth) throw RuntimeError(e.line, "Stack overflow: too many nested calls.");

        auto scope = std::make_shared<Environment>(fn.closure);
        for (size_t i = 0; i < args.size(); i++) scope->define(fn.decl->params[i], args[i]);

        DepthGuard depth(callDepth_);
        try {
            executeBlock(fn.decl->body, scope);
        } catch (ReturnSignal& ret) {
            return std::move(ret.value);
        }
        return Value{};  // functions without a return give nil
    }

    // ---- value helpers ----

    static bool truthy(const Value& v) {
        if (std::holds_alternative<std::monostate>(v)) return false;
        if (const bool* b = std::get_if<bool>(&v)) return *b;
        return true;  // 0 and "" are truthy
    }

    static bool isEqual(const Value& a, const Value& b) {
        if (a.index() != b.index()) return false;
        if (const double* x = std::get_if<double>(&a)) return *x == std::get<double>(b);
        if (const bool* x = std::get_if<bool>(&a)) return *x == std::get<bool>(b);
        if (const std::string* x = std::get_if<std::string>(&a)) return *x == std::get<std::string>(b);
        if (auto* x = std::get_if<std::shared_ptr<Function>>(&a)) return *x == std::get<std::shared_ptr<Function>>(b);
        return true;  // nil == nil
    }

    static std::string stringify(const Value& v) {
        if (std::holds_alternative<std::monostate>(v)) return "nil";
        if (const bool* b = std::get_if<bool>(&v)) return *b ? "true" : "false";
        if (const std::string* s = std::get_if<std::string>(&v)) return *s;
        if (auto* f = std::get_if<std::shared_ptr<Function>>(&v)) return "<fn " + (*f)->decl->name + ">";
        double d = std::get<double>(v);
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
        std::ostringstream out;
        out << std::setprecision(15) << d;
        return out.str();
    }
};
