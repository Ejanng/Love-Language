#pragma once
// Bytecode backend, part 3: the virtual machine that runs compiled bytecode.
//
// One value stack is shared by every call. A call frame records which closure is running,
// its instruction pointer, and `base`, the stack index of its slot 0 (the callee itself).
// Arguments are already on the stack when a call starts, so they become locals 1..arity for free.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "../errors.hpp"
#include "../lexer.hpp"
#include "../parser.hpp"
#include "chunk.hpp"
#include "compiler.hpp"

namespace vm {

// A variable captured by a closure. While the variable's scope is still running it is "open"
// and lives on the stack at `slot`; when the scope ends it is "closed": the value moves into `closed`.
// Using a stack index instead of a pointer keeps it valid when the stack vector grows.
struct Upvalue {
    size_t slot;
    bool open = true;
    Value closed;
};

// A function value: compiled code plus the variables it captured.
// Note: a closure that captures itself (a recursive local function) forms a reference cycle and is never freed.
// Fixing that properly is what a garbage collector is for.
struct Closure {
    std::shared_ptr<Proto> proto;
    std::vector<std::shared_ptr<Upvalue>> upvalues;
};

class VM {
public:
    explicit VM(bool dumpBytecode = false) : dumpBytecode_(dumpBytecode) {}

    // Runs a piece of source. Returns 0 on success, 65 for a syntax or compile error, 70 for a runtime error.
    // Globals persist between calls, which is what the REPL relies on.
    int run(const std::string& source) {
        std::shared_ptr<Proto> script;
        try {
            Lexer lexer(source);
            Parser parser(lexer.scanTokens());
            std::vector<StmtPtr> program = parser.parse();
            script = Compiler().compile(program);
        } catch (const SyntaxError& e) {
            report("Syntax error", e);
            return 65;
        } catch (const CompileError& e) {
            report("Compile error", e);
            return 65;
        }
        if (dumpBytecode_) {
            disassemble(*script, std::cout);
            std::cout << "\n";
        }
        try {
            execute(script);
        } catch (const RuntimeError& e) {
            report("Runtime error", e);
            stack_.clear();
            frames_.clear();
            openUpvalues_.clear();
            return 70;
        }
        return 0;
    }

private:
    static constexpr size_t kMaxCallDepth = 1000;

    struct Frame {
        std::shared_ptr<Closure> closure;
        size_t ip = 0;
        size_t base = 0;
    };

    bool dumpBytecode_;
    std::vector<Value> stack_;
    std::vector<Frame> frames_;
    std::vector<std::shared_ptr<Upvalue>> openUpvalues_;
    std::unordered_map<std::string, Value> globals_;

    static void report(const char* kind, const LangError& e) {
        std::cout.flush();  // keep program output ahead of the error message when both go to one stream
        std::cerr << "[line " << e.line << "] " << kind << ": " << e.what() << "\n";
    }

    // ---- stack helpers ----

    void push(Value v) { stack_.push_back(std::move(v)); }

    Value pop() {
        Value v = std::move(stack_.back());
        stack_.pop_back();
        return v;
    }

    Value& peek(size_t distance = 0) { return stack_[stack_.size() - 1 - distance]; }

    // ---- the dispatch loop ----

    void execute(std::shared_ptr<Proto> script) {
        auto closure = std::make_shared<Closure>(Closure{std::move(script), {}});
        push(closure);
        frames_.push_back(Frame{closure, 0, 0});

        for (;;) {
            Frame& frame = frames_.back();  // re-read every step: calls and returns change frames_
            const Proto& proto = *frame.closure->proto;
            const Instr& in = proto.code[frame.ip++];
            switch (in.op) {
                case Op::Constant: push(proto.constants[in.operand]); break;
                case Op::Nil: push(Value{}); break;
                case Op::True: push(true); break;
                case Op::False: push(false); break;
                case Op::Pop: stack_.pop_back(); break;

                case Op::GetLocal: {
                    Value v = stack_[frame.base + in.operand];  // copy first: push may reallocate the stack
                    push(std::move(v));
                    break;
                }
                case Op::SetLocal: stack_[frame.base + in.operand] = peek(); break;

                case Op::GetGlobal: {
                    const std::string& name = std::get<std::string>(proto.constants[in.operand]);
                    auto it = globals_.find(name);
                    if (it == globals_.end()) throw RuntimeError(in.line, "Undefined variable '" + name + "'.");
                    push(it->second);
                    break;
                }
                case Op::DefineGlobal:
                    globals_[std::get<std::string>(proto.constants[in.operand])] = pop();
                    break;
                case Op::SetGlobal: {
                    const std::string& name = std::get<std::string>(proto.constants[in.operand]);
                    auto it = globals_.find(name);
                    if (it == globals_.end()) throw RuntimeError(in.line, "Undefined variable '" + name + "'.");
                    it->second = peek();
                    break;
                }

                case Op::GetUpvalue: {
                    Upvalue& up = *frame.closure->upvalues[in.operand];
                    Value v = up.open ? stack_[up.slot] : up.closed;
                    push(std::move(v));
                    break;
                }
                case Op::SetUpvalue: {
                    Upvalue& up = *frame.closure->upvalues[in.operand];
                    (up.open ? stack_[up.slot] : up.closed) = peek();
                    break;
                }

                case Op::Equal: {
                    Value b = pop();
                    Value a = pop();
                    push(isEqual(a, b));
                    break;
                }
                case Op::NotEqual: {
                    Value b = pop();
                    Value a = pop();
                    push(!isEqual(a, b));
                    break;
                }
                case Op::Add: {
                    Value b = pop();
                    Value a = pop();
                    const double* x = std::get_if<double>(&a);
                    const double* y = std::get_if<double>(&b);
                    if (x && y) {
                        push(*x + *y);
                    } else if (std::holds_alternative<std::string>(a) || std::holds_alternative<std::string>(b)) {
                        push(stringify(a) + stringify(b));  // string + anything joins them as text
                    } else {
                        throw RuntimeError(in.line, "Operands of '+' must be two numbers or include a string.");
                    }
                    break;
                }
                case Op::Greater:
                case Op::GreaterEqual:
                case Op::Less:
                case Op::LessEqual:
                case Op::Subtract:
                case Op::Multiply:
                case Op::Divide:
                case Op::Modulo: arithmetic(in); break;

                case Op::Not: push(!truthy(pop())); break;
                case Op::Negate: {
                    const double* n = std::get_if<double>(&peek());
                    if (!n) throw RuntimeError(in.line, "Operand of '-' must be a number.");
                    peek() = -*n;
                    break;
                }

                case Op::Print: std::cout << stringify(pop()) << "\n"; break;

                case Op::Jump: frame.ip = in.operand; break;
                case Op::JumpIfFalse:
                    if (!truthy(peek())) frame.ip = in.operand;
                    break;
                case Op::JumpIfTrue:
                    if (truthy(peek())) frame.ip = in.operand;
                    break;

                case Op::Call: callValue(in.operand, in.line); break;

                case Op::Closure: {
                    auto fn = std::make_shared<Closure>();
                    fn->proto = proto.functions[in.operand];
                    for (const UpvalueRef& ref : fn->proto->upvalues) {
                        fn->upvalues.push_back(ref.isLocal ? captureUpvalue(frame.base + ref.index)
                                                           : frame.closure->upvalues[ref.index]);
                    }
                    push(std::move(fn));
                    break;
                }
                case Op::CloseUpvalue:
                    closeUpvalues(stack_.size() - 1);
                    stack_.pop_back();
                    break;

                case Op::Return: {
                    Value result = pop();
                    size_t base = frame.base;
                    frames_.pop_back();
                    closeUpvalues(base);
                    stack_.resize(base);  // drop the callee, its arguments and its locals
                    if (frames_.empty()) return;  // the top-level script finished
                    push(std::move(result));
                    break;
                }
            }
        }
    }

    void arithmetic(const Instr& in) {
        Value b = pop();
        Value a = pop();
        const double* x = std::get_if<double>(&a);
        const double* y = std::get_if<double>(&b);
        if (!x || !y) throw RuntimeError(in.line, "Operands must be numbers.");
        switch (in.op) {
            case Op::Greater: push(*x > *y); break;
            case Op::GreaterEqual: push(*x >= *y); break;
            case Op::Less: push(*x < *y); break;
            case Op::LessEqual: push(*x <= *y); break;
            case Op::Subtract: push(*x - *y); break;
            case Op::Multiply: push(*x * *y); break;
            case Op::Divide:
                if (*y == 0) throw RuntimeError(in.line, "Division by zero.");
                push(*x / *y);
                break;
            case Op::Modulo:
                if (*y == 0) throw RuntimeError(in.line, "Division by zero.");
                push(std::fmod(*x, *y));
                break;
            default: break;
        }
    }

    // The callee sits below its `argc` arguments. It becomes slot 0 of the new frame.
    void callValue(int argc, int line) {
        size_t calleeSlot = stack_.size() - argc - 1;
        auto* fn = std::get_if<std::shared_ptr<Closure>>(&stack_[calleeSlot]);
        if (!fn) throw RuntimeError(line, "Can only call functions.");
        int arity = (*fn)->proto->arity;
        if (argc != arity) {
            throw RuntimeError(line, "Expected " + std::to_string(arity) + " arguments but got " +
                                         std::to_string(argc) + ".");
        }
        if (frames_.size() > kMaxCallDepth) throw RuntimeError(line, "Stack overflow: too many nested calls.");
        frames_.push_back(Frame{*fn, 0, calleeSlot});
    }

    // Returns the open upvalue for a stack slot, creating it if needed,
    // so two closures that capture the same variable share it.
    std::shared_ptr<Upvalue> captureUpvalue(size_t slot) {
        for (const auto& up : openUpvalues_) {
            if (up->slot == slot) return up;
        }
        auto up = std::make_shared<Upvalue>(Upvalue{slot, true, Value{}});
        openUpvalues_.push_back(up);
        return up;
    }

    // Closes every open upvalue at or above `fromSlot`: those stack slots are about to disappear.
    void closeUpvalues(size_t fromSlot) {
        auto keep = std::remove_if(openUpvalues_.begin(), openUpvalues_.end(), [&](const std::shared_ptr<Upvalue>& up) {
            if (up->slot < fromSlot) return false;
            up->closed = stack_[up->slot];
            up->open = false;
            return true;
        });
        openUpvalues_.erase(keep, openUpvalues_.end());
    }

    // ---- value helpers (same rules as the tree-walking interpreter) ----

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
        if (auto* x = std::get_if<std::shared_ptr<Closure>>(&a)) return *x == std::get<std::shared_ptr<Closure>>(b);
        return true;  // nil == nil
    }

    static std::string stringify(const Value& v) {
        if (std::holds_alternative<std::monostate>(v)) return "nil";
        if (const bool* b = std::get_if<bool>(&v)) return *b ? "true" : "false";
        if (const std::string* s = std::get_if<std::string>(&v)) return *s;
        if (auto* f = std::get_if<std::shared_ptr<Closure>>(&v)) return "<fn " + (*f)->proto->name + ">";
        return formatNumber(std::get<double>(v));
    }
};

}  // namespace vm
