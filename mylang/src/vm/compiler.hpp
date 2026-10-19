#pragma once
// Bytecode backend, part 2: the compiler walks the AST once and emits stack bytecode.
//
// Name resolution happens here, at compile time:
//   - a name declared in an enclosing block of the current function is a local (a stack slot),
//   - a local of an enclosing function is an upvalue (captured by the closure),
//   - anything else is a global, looked up by name at run time.
// Top-level `let` and `fn` declarations are globals; declarations inside any block are locals.
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../ast.hpp"
#include "../errors.hpp"
#include "chunk.hpp"

namespace vm {

class Compiler {
public:
    // Compiles a whole program into the top-level "<script>" function.
    std::shared_ptr<Proto> compile(const std::vector<StmtPtr>& program) {
        FunctionState script(nullptr, "<script>");
        current_ = &script;
        for (const StmtPtr& s : program) statement(*s);
        int lastLine = program.empty() ? 1 : program.back()->line;
        emit(Op::Nil, 0, lastLine);
        emit(Op::Return, 0, lastLine);
        current_ = nullptr;
        return script.proto;
    }

private:
    struct Local {
        std::string name;
        int depth;
        bool captured = false;  // an inner closure refers to it, so it must be moved off the stack when it ends
    };

    // Compiler state for one function being compiled. They form a chain through `enclosing`.
    struct FunctionState {
        FunctionState* enclosing;
        std::shared_ptr<Proto> proto = std::make_shared<Proto>();
        std::vector<Local> locals;
        int scopeDepth = 0;

        FunctionState(FunctionState* parent, std::string name) : enclosing(parent) {
            proto->name = std::move(name);
            locals.push_back(Local{"", 0});  // slot 0 holds the function being called
        }
    };

    FunctionState* current_ = nullptr;

    // ---- emitting ----

    int emit(Op op, int operand, int line) {
        current_->proto->code.push_back(Instr{op, operand, line});
        return static_cast<int>(current_->proto->code.size()) - 1;
    }

    int here() const { return static_cast<int>(current_->proto->code.size()); }

    // Jumps are emitted before their target is known and patched once it is ("backpatching").
    void patchJump(int at) { current_->proto->code[at].operand = here(); }

    int addConstant(Value v) {
        current_->proto->constants.push_back(std::move(v));
        return static_cast<int>(current_->proto->constants.size()) - 1;
    }

    // ---- scopes and names ----

    void beginScope() { current_->scopeDepth++; }

    void endScope(int line) {
        FunctionState& fs = *current_;
        fs.scopeDepth--;
        while (fs.locals.size() > 1 && fs.locals.back().depth > fs.scopeDepth) {
            emit(fs.locals.back().captured ? Op::CloseUpvalue : Op::Pop, 0, line);
            fs.locals.pop_back();
        }
    }

    // The new local takes the next stack slot, which is where its value already sits.
    void addLocal(const std::string& name) { current_->locals.push_back(Local{name, current_->scopeDepth}); }

    static int resolveLocal(const FunctionState& fs, const std::string& name) {
        for (int i = static_cast<int>(fs.locals.size()) - 1; i > 0; i--) {
            if (fs.locals[i].name == name) return i;
        }
        return -1;
    }

    static int addUpvalue(FunctionState& fs, bool isLocal, int index) {
        std::vector<UpvalueRef>& ups = fs.proto->upvalues;
        for (size_t i = 0; i < ups.size(); i++) {
            if (ups[i].isLocal == isLocal && ups[i].index == index) return static_cast<int>(i);
        }
        ups.push_back(UpvalueRef{isLocal, index});
        return static_cast<int>(ups.size()) - 1;
    }

    // Looks for `name` in the enclosing functions, threading an upvalue through each level in between.
    static int resolveUpvalue(FunctionState& fs, const std::string& name) {
        if (!fs.enclosing) return -1;
        int local = resolveLocal(*fs.enclosing, name);
        if (local != -1) {
            fs.enclosing->locals[local].captured = true;
            return addUpvalue(fs, true, local);
        }
        int up = resolveUpvalue(*fs.enclosing, name);
        if (up != -1) return addUpvalue(fs, false, up);
        return -1;
    }

    void namedVariable(const std::string& name, bool assign, int line) {
        int slot = resolveLocal(*current_, name);
        if (slot != -1) {
            emit(assign ? Op::SetLocal : Op::GetLocal, slot, line);
            return;
        }
        int up = resolveUpvalue(*current_, name);
        if (up != -1) {
            emit(assign ? Op::SetUpvalue : Op::GetUpvalue, up, line);
            return;
        }
        emit(assign ? Op::SetGlobal : Op::GetGlobal, addConstant(name), line);
    }

    // ---- statements ----

    void statement(const Stmt& s) {
        switch (s.kind) {
            case Stmt::Kind::Expression:
                expression(*s.expr);
                emit(Op::Pop, 0, s.line);
                break;
            case Stmt::Kind::Print:
                expression(*s.expr);
                emit(Op::Print, 0, s.line);
                break;
            case Stmt::Kind::Let:
                // The initializer is compiled before the name exists, so `let x = x;` reads the outer x.
                if (s.expr) {
                    expression(*s.expr);
                } else {
                    emit(Op::Nil, 0, s.line);
                }
                defineVariable(s.name, s.line);
                break;
            case Stmt::Kind::Block:
                beginScope();
                for (const StmtPtr& inner : s.body) statement(*inner);
                endScope(s.line);
                break;
            case Stmt::Kind::If: {
                expression(*s.expr);
                int toElse = emit(Op::JumpIfFalse, 0, s.line);
                emit(Op::Pop, 0, s.line);  // drop the condition on the "then" path
                statement(*s.thenBranch);
                int toEnd = emit(Op::Jump, 0, s.line);
                patchJump(toElse);
                emit(Op::Pop, 0, s.line);  // drop the condition on the "else" path
                if (s.elseBranch) statement(*s.elseBranch);
                patchJump(toEnd);
                break;
            }
            case Stmt::Kind::While: {
                int loopStart = here();
                expression(*s.expr);
                int toExit = emit(Op::JumpIfFalse, 0, s.line);
                emit(Op::Pop, 0, s.line);
                statement(*s.thenBranch);
                emit(Op::Jump, loopStart, s.line);
                patchJump(toExit);
                emit(Op::Pop, 0, s.line);
                break;
            }
            case Stmt::Kind::Function:
                // A local function is declared before its body is compiled so it can call itself.
                if (current_->scopeDepth > 0) addLocal(s.name);
                function(s);
                if (current_->scopeDepth == 0) emit(Op::DefineGlobal, addConstant(s.name), s.line);
                break;
            case Stmt::Kind::Return:
                if (!current_->enclosing) throw CompileError(s.line, "Can't return from top-level code.");
                if (s.expr) {
                    expression(*s.expr);
                } else {
                    emit(Op::Nil, 0, s.line);
                }
                emit(Op::Return, 0, s.line);
                break;
        }
    }

    void defineVariable(const std::string& name, int line) {
        if (current_->scopeDepth > 0) {
            addLocal(name);
        } else {
            emit(Op::DefineGlobal, addConstant(name), line);
        }
    }

    // Compiles a function body into its own Proto and emits a Closure instruction that creates it.
    void function(const Stmt& s) {
        FunctionState fs(current_, s.name);
        fs.proto->arity = static_cast<int>(s.params.size());
        fs.scopeDepth = 1;  // parameters and the body's declarations are locals
        current_ = &fs;
        for (const std::string& p : s.params) addLocal(p);
        for (const StmtPtr& inner : s.body) statement(*inner);
        emit(Op::Nil, 0, s.line);  // falling off the end returns nil
        emit(Op::Return, 0, s.line);
        current_ = fs.enclosing;

        current_->proto->functions.push_back(fs.proto);
        emit(Op::Closure, static_cast<int>(current_->proto->functions.size()) - 1, s.line);
    }

    // ---- expressions ----
    // Every expression leaves exactly one value on the stack.

    void expression(const Expr& e) {
        switch (e.kind) {
            case Expr::Kind::Number: emit(Op::Constant, addConstant(e.number), e.line); break;
            case Expr::Kind::String: emit(Op::Constant, addConstant(e.text), e.line); break;
            case Expr::Kind::Bool: emit(e.boolean ? Op::True : Op::False, 0, e.line); break;
            case Expr::Kind::Nil: emit(Op::Nil, 0, e.line); break;
            case Expr::Kind::Variable: namedVariable(e.text, false, e.line); break;
            case Expr::Kind::Assign:
                expression(*e.left);
                namedVariable(e.text, true, e.line);
                break;
            case Expr::Kind::Unary:
                expression(*e.left);
                emit(e.op == TokenType::Bang ? Op::Not : Op::Negate, 0, e.line);
                break;
            case Expr::Kind::Binary:
                expression(*e.left);
                expression(*e.right);
                emit(binaryOp(e.op), 0, e.line);
                break;
            case Expr::Kind::Logical: {
                // Short-circuit: if the left side decides the answer, skip the right side and keep the left value.
                expression(*e.left);
                int skip = emit(e.op == TokenType::Or ? Op::JumpIfTrue : Op::JumpIfFalse, 0, e.line);
                emit(Op::Pop, 0, e.line);
                expression(*e.right);
                patchJump(skip);
                break;
            }
            case Expr::Kind::Call:
                expression(*e.left);
                for (const ExprPtr& a : e.args) expression(*a);
                emit(Op::Call, static_cast<int>(e.args.size()), e.line);
                break;
        }
    }

    static Op binaryOp(TokenType t) {
        switch (t) {
            case TokenType::EqualEqual: return Op::Equal;
            case TokenType::BangEqual: return Op::NotEqual;
            case TokenType::Greater: return Op::Greater;
            case TokenType::GreaterEqual: return Op::GreaterEqual;
            case TokenType::Less: return Op::Less;
            case TokenType::LessEqual: return Op::LessEqual;
            case TokenType::Plus: return Op::Add;
            case TokenType::Minus: return Op::Subtract;
            case TokenType::Star: return Op::Multiply;
            case TokenType::Slash: return Op::Divide;
            default: return Op::Modulo;
        }
    }
};

}  // namespace vm
