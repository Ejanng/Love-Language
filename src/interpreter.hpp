#pragma once

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "value.hpp"

namespace rene {

// ---------------------------------------------------------------- Environment

// A scope: maps names to values and links to its enclosing scope.
class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr) : parent_(std::move(parent)) {}

    void define(const std::string& name, Value value) { values_[name] = std::move(value); }

    Value get(const std::string& name, int line) const {
        for (const Environment* env = this; env; env = env->parent_.get()) {
            auto it = env->values_.find(name);
            if (it != env->values_.end()) return it->second;
        }
        throw ReneError("RuntimeError", line, "undefined variable '" + name + "'");
    }

    void assign(const std::string& name, Value value, int line) {
        for (Environment* env = this; env; env = env->parent_.get()) {
            auto it = env->values_.find(name);
            if (it != env->values_.end()) {
                it->second = std::move(value);
                return;
            }
        }
        throw ReneError("RuntimeError", line, "cannot assign to undeclared variable '" + name +
                                                  "' (declare it with 'let' first)");
    }

private:
    std::unordered_map<std::string, Value> values_;
    std::shared_ptr<Environment> parent_;
};

// ------------------------------------------------------------ Control signals

struct ReturnSignal { Value value; };
struct BreakSignal {};
struct ContinueSignal {};

// ------------------------------------------------------------------ Functions

struct NativeFunction : Callable {
    using Fn = std::function<Value(std::vector<Value>&, int line)>;

    std::string name_;
    int min_;
    int max_;
    Fn fn_;

    NativeFunction(std::string name, int min, int max, Fn fn)
        : name_(std::move(name)), min_(min), max_(max), fn_(std::move(fn)) {}

    std::string name() const override { return name_; }
    int minArity() const override { return min_; }
    int maxArity() const override { return max_; }
    Value call(Interpreter&, std::vector<Value>& args, int line) override { return fn_(args, line); }
};

struct UserFunction : Callable {
    std::shared_ptr<FunctionStmt> declaration;
    std::shared_ptr<Environment> closure;

    UserFunction(std::shared_ptr<FunctionStmt> decl, std::shared_ptr<Environment> env)
        : declaration(std::move(decl)), closure(std::move(env)) {}

    std::string name() const override { return declaration->name; }
    int minArity() const override { return static_cast<int>(declaration->params.size()); }
    int maxArity() const override { return minArity(); }
    Value call(Interpreter& interpreter, std::vector<Value>& args, int line) override;
};

// ---------------------------------------------------------------- Interpreter

// Tree-walking interpreter: executes the AST directly (no compilation step).
class Interpreter {
public:
    Interpreter() : globals_(std::make_shared<Environment>()), env_(globals_) { defineBuiltins(); }

    void run(const std::vector<StmtPtr>& program) {
        for (const auto& stmt : program) execute(*stmt);
    }

    Value callUserFunction(const UserFunction& fn, std::vector<Value>& args, int line) {
        if (callDepth_ >= config::MAX_CALL_DEPTH) {
            throw ReneError("RuntimeError", line, "stack overflow (too many nested calls)");
        }
        auto scope = std::make_shared<Environment>(fn.closure);
        const auto& params = fn.declaration->params;
        for (size_t i = 0; i < params.size(); ++i) scope->define(params[i], args[i]);

        ++callDepth_;
        try {
            executeBlock(fn.declaration->body, scope);
        } catch (ReturnSignal& signal) {
            --callDepth_;
            return std::move(signal.value);
        } catch (...) {
            --callDepth_;
            throw;
        }
        --callDepth_;
        return Value{};
    }

private:
    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> env_;
    int callDepth_ = 0;

    // Restores the previous scope even when a signal or error unwinds the stack.
    struct ScopeGuard {
        Interpreter& interp;
        std::shared_ptr<Environment> previous;
        ~ScopeGuard() { interp.env_ = std::move(previous); }
    };

    void executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> scope) {
        ScopeGuard guard{*this, env_};
        env_ = std::move(scope);
        for (const auto& stmt : statements) execute(*stmt);
    }

    [[noreturn]] static void fail(int line, const std::string& message) {
        throw ReneError("RuntimeError", line, message);
    }

    static double expectNumber(const Value& v, int line, const std::string& context) {
        if (!isNumber(v)) fail(line, context + " expects a number, got " + typeName(v));
        return std::get<double>(v);
    }

    // Validates a list/string index and returns it as a size_t.
    static size_t checkIndex(const Value& index, size_t size, int line) {
        double d = expectNumber(index, line, "indexing");
        if (std::floor(d) != d) fail(line, "index must be a whole number, got " + formatNumber(d));
        if (d < 0) d += static_cast<double>(size);  // negative indexes count from the end
        if (d < 0 || d >= static_cast<double>(size)) {
            fail(line, "index " + formatNumber(std::get<double>(index)) + " out of range (size " +
                           std::to_string(size) + ")");
        }
        return static_cast<size_t>(d);
    }

    // ---------------------------------------------------------- statements

    void execute(const Stmt& stmt) {
        if (auto s = dynamic_cast<const ExpressionStmt*>(&stmt)) {
            evaluate(*s->expression);
            return;
        }
        if (auto s = dynamic_cast<const PrintStmt*>(&stmt)) {
            std::string line;
            for (size_t i = 0; i < s->values.size(); ++i) {
                if (i > 0) line += ' ';
                line += stringify(evaluate(*s->values[i]));
            }
            std::cout << line << '\n';
            return;
        }
        if (auto s = dynamic_cast<const LetStmt*>(&stmt)) {
            env_->define(s->name, s->initializer ? evaluate(*s->initializer) : Value{});
            return;
        }
        if (auto s = dynamic_cast<const BlockStmt*>(&stmt)) {
            executeBlock(s->statements, std::make_shared<Environment>(env_));
            return;
        }
        if (auto s = dynamic_cast<const IfStmt*>(&stmt)) {
            if (isTruthy(evaluate(*s->condition))) {
                execute(*s->thenBranch);
            } else if (s->elseBranch) {
                execute(*s->elseBranch);
            }
            return;
        }
        if (auto s = dynamic_cast<const WhileStmt*>(&stmt)) {
            while (isTruthy(evaluate(*s->condition))) {
                try {
                    execute(*s->body);
                } catch (BreakSignal&) {
                    break;
                } catch (ContinueSignal&) {
                }
            }
            return;
        }
        if (auto s = dynamic_cast<const ForInStmt*>(&stmt)) {
            executeForIn(*s);
            return;
        }
        if (dynamic_cast<const BreakStmt*>(&stmt)) throw BreakSignal{};
        if (dynamic_cast<const ContinueStmt*>(&stmt)) throw ContinueSignal{};
        if (auto s = dynamic_cast<const FunctionStmt*>(&stmt)) {
            auto decl = std::make_shared<FunctionStmt>(*s);
            env_->define(s->name, std::shared_ptr<Callable>(std::make_shared<UserFunction>(decl, env_)));
            return;
        }
        if (auto s = dynamic_cast<const ReturnStmt*>(&stmt)) {
            throw ReturnSignal{s->value ? evaluate(*s->value) : Value{}};
        }
        fail(stmt.line, "unknown statement");
    }

    void executeForIn(const ForInStmt& s) {
        Value iterable = evaluate(*s.iterable);
        std::vector<Value> items;  // snapshot, so mutating the list inside the loop is safe
        if (isList(iterable)) {
            items = std::get<std::shared_ptr<ListObject>>(iterable)->items;
        } else if (isString(iterable)) {
            for (char c : std::get<std::string>(iterable)) items.emplace_back(std::string(1, c));
        } else {
            fail(s.line, "cannot iterate over " + typeName(iterable));
        }

        for (auto& item : items) {
            auto scope = std::make_shared<Environment>(env_);
            scope->define(s.variable, item);
            try {
                executeBlock({s.body}, scope);
            } catch (BreakSignal&) {
                break;
            } catch (ContinueSignal&) {
            }
        }
    }

    // --------------------------------------------------------- expressions

    Value evaluate(const Expr& expr) {
        if (auto e = dynamic_cast<const LiteralExpr*>(&expr)) return e->value;
        if (auto e = dynamic_cast<const VariableExpr*>(&expr)) return env_->get(e->name, e->line);
        if (auto e = dynamic_cast<const AssignExpr*>(&expr)) {
            Value value = evaluate(*e->value);
            env_->assign(e->name, value, e->line);
            return value;
        }
        if (auto e = dynamic_cast<const UnaryExpr*>(&expr)) {
            Value right = evaluate(*e->right);
            if (e->op == TokenType::Not) return !isTruthy(right);
            return -expectNumber(right, e->line, "unary '-'");
        }
        if (auto e = dynamic_cast<const LogicalExpr*>(&expr)) {
            Value left = evaluate(*e->left);
            if (e->op == TokenType::Or ? isTruthy(left) : !isTruthy(left)) return left;
            return evaluate(*e->right);
        }
        if (auto e = dynamic_cast<const BinaryExpr*>(&expr)) {
            return binary(evaluate(*e->left), e->op, evaluate(*e->right), e->line);
        }
        if (auto e = dynamic_cast<const CallExpr*>(&expr)) return callExpr(*e);
        if (auto e = dynamic_cast<const ListExpr*>(&expr)) {
            auto list = std::make_shared<ListObject>();
            for (const auto& element : e->elements) list->items.push_back(evaluate(*element));
            return list;
        }
        if (auto e = dynamic_cast<const IndexExpr*>(&expr)) {
            Value object = evaluate(*e->object);
            Value index = evaluate(*e->index);
            if (isList(object)) {
                auto& items = std::get<std::shared_ptr<ListObject>>(object)->items;
                return items[checkIndex(index, items.size(), e->line)];
            }
            if (isString(object)) {
                const auto& str = std::get<std::string>(object);
                return std::string(1, str[checkIndex(index, str.size(), e->line)]);
            }
            fail(e->line, "cannot index into " + typeName(object));
        }
        if (auto e = dynamic_cast<const IndexAssignExpr*>(&expr)) {
            Value object = evaluate(*e->object);
            if (!isList(object)) fail(e->line, "only lists support index assignment, got " + typeName(object));
            Value index = evaluate(*e->index);
            Value value = evaluate(*e->value);
            auto& items = std::get<std::shared_ptr<ListObject>>(object)->items;
            items[checkIndex(index, items.size(), e->line)] = value;
            return value;
        }
        if (auto e = dynamic_cast<const LambdaExpr*>(&expr)) {
            return std::shared_ptr<Callable>(std::make_shared<UserFunction>(e->function, env_));
        }
        fail(expr.line, "unknown expression");
    }

    Value callExpr(const CallExpr& e) {
        Value callee = evaluate(*e.callee);
        if (!isCallable(callee)) fail(e.line, "can only call functions, got " + typeName(callee));
        auto fn = std::get<std::shared_ptr<Callable>>(callee);

        std::vector<Value> args;
        for (const auto& arg : e.args) args.push_back(evaluate(*arg));

        int count = static_cast<int>(args.size());
        if (count < fn->minArity() || count > fn->maxArity()) {
            std::string expected = fn->minArity() == fn->maxArity()
                                       ? std::to_string(fn->minArity())
                                       : std::to_string(fn->minArity()) + "-" + std::to_string(fn->maxArity());
            fail(e.line, fn->name() + "() expects " + expected + " argument(s), got " + std::to_string(count));
        }
        return fn->call(*this, args, e.line);
    }

    static Value binary(const Value& left, TokenType op, const Value& right, int line) {
        switch (op) {
            case TokenType::EqualEqual: return left == right;
            case TokenType::BangEqual: return left != right;
            case TokenType::Plus:
                if (isNumber(left) && isNumber(right)) return std::get<double>(left) + std::get<double>(right);
                if (isString(left) || isString(right)) return stringify(left) + stringify(right);
                if (isList(left) && isList(right)) {
                    auto joined = std::make_shared<ListObject>(*std::get<std::shared_ptr<ListObject>>(left));
                    const auto& tail = std::get<std::shared_ptr<ListObject>>(right)->items;
                    joined->items.insert(joined->items.end(), tail.begin(), tail.end());
                    return joined;
                }
                fail(line, "cannot add " + typeName(left) + " and " + typeName(right));
            case TokenType::Less:
            case TokenType::LessEqual:
            case TokenType::Greater:
            case TokenType::GreaterEqual:
                if (isString(left) && isString(right)) {
                    return compare(std::get<std::string>(left).compare(std::get<std::string>(right)), op);
                }
                if (isNumber(left) && isNumber(right)) {
                    double a = std::get<double>(left), b = std::get<double>(right);
                    return compare(a < b ? -1 : (a > b ? 1 : 0), op);
                }
                fail(line, "cannot compare " + typeName(left) + " and " + typeName(right));
            default: break;
        }

        double a = expectNumber(left, line, "arithmetic");
        double b = expectNumber(right, line, "arithmetic");
        switch (op) {
            case TokenType::Minus: return a - b;
            case TokenType::Star: return a * b;
            case TokenType::Slash:
                if (b == 0) fail(line, "division by zero");
                return a / b;
            case TokenType::Percent:
                if (b == 0) fail(line, "modulo by zero");
                return std::fmod(a, b);
            default: fail(line, "unknown operator");
        }
    }

    static Value compare(int order, TokenType op) {
        switch (op) {
            case TokenType::Less: return order < 0;
            case TokenType::LessEqual: return order <= 0;
            case TokenType::Greater: return order > 0;
            default: return order >= 0;
        }
    }

    // ------------------------------------------------------------ builtins

    void native(const std::string& name, int min, int max, NativeFunction::Fn fn) {
        globals_->define(name, std::shared_ptr<Callable>(std::make_shared<NativeFunction>(name, min, max, std::move(fn))));
    }

    static std::shared_ptr<ListObject> expectList(const Value& v, int line, const std::string& fn) {
        if (!isList(v)) fail(line, fn + "() expects a list, got " + typeName(v));
        return std::get<std::shared_ptr<ListObject>>(v);
    }

    void defineBuiltins() {
        native("len", 1, 1, [](std::vector<Value>& args, int line) -> Value {
            if (isString(args[0])) return static_cast<double>(std::get<std::string>(args[0]).size());
            return static_cast<double>(expectList(args[0], line, "len")->items.size());
        });
        native("push", 2, 2, [](std::vector<Value>& args, int line) -> Value {
            expectList(args[0], line, "push")->items.push_back(args[1]);
            return args[0];
        });
        native("pop", 1, 1, [](std::vector<Value>& args, int line) -> Value {
            auto list = expectList(args[0], line, "pop");
            if (list->items.empty()) fail(line, "pop() from an empty list");
            Value last = list->items.back();
            list->items.pop_back();
            return last;
        });
        native("str", 1, 1, [](std::vector<Value>& args, int) -> Value { return stringify(args[0]); });
        native("type", 1, 1, [](std::vector<Value>& args, int) -> Value { return typeName(args[0]); });
        native("num", 1, 1, [](std::vector<Value>& args, int line) -> Value {
            if (isNumber(args[0])) return args[0];
            if (isString(args[0])) {
                const auto& text = std::get<std::string>(args[0]);
                try {
                    size_t used = 0;
                    double d = std::stod(text, &used);
                    if (used == text.size()) return d;
                } catch (...) {
                }
                fail(line, "num() cannot convert \"" + text + "\" to a number");
            }
            fail(line, "num() cannot convert " + typeName(args[0]) + " to a number");
        });
        native("range", 1, 2, [](std::vector<Value>& args, int line) -> Value {
            double start = 0, end;
            if (args.size() == 1) {
                end = expectNumber(args[0], line, "range()");
            } else {
                start = expectNumber(args[0], line, "range()");
                end = expectNumber(args[1], line, "range()");
            }
            auto list = std::make_shared<ListObject>();
            for (double i = start; i < end; ++i) list->items.emplace_back(i);
            return list;
        });
        native("input", 0, 1, [](std::vector<Value>& args, int) -> Value {
            if (!args.empty()) std::cout << stringify(args[0]) << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) return Value{};
            return line;
        });
        native("clock", 0, 0, [](std::vector<Value>&, int) -> Value {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        });
    }
};

inline Value UserFunction::call(Interpreter& interpreter, std::vector<Value>& args, int line) {
    return interpreter.callUserFunction(*this, args, line);
}

}  // namespace rene
