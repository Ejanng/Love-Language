#pragma once
// Bytecode backend, part 1: the instruction set and the compiled form of a function.
//
// Each instruction is an (opcode, operand) pair plus the source line it came from.
// Operands are plain ints: a constant index, a local slot, an upvalue index, a jump
// target (an absolute instruction index), or an argument count, depending on the opcode.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace vm {

struct Closure;

// A runtime value: nil, number, boolean, string, or closure.
// Always build string values with std::string (a bare "text" would convert to bool).
using Value = std::variant<std::monostate, double, bool, std::string, std::shared_ptr<Closure>>;

enum class Op {
    Constant,      // push constants[operand]
    Nil, True, False,
    Pop,           // discard the top of the stack
    GetLocal,      // push the local in slot `operand` of the current frame
    SetLocal,      // store the top of the stack into local `operand` (value stays on the stack)
    GetGlobal,     // push the global named by constants[operand]
    DefineGlobal,  // pop a value and bind it to the global named by constants[operand]
    SetGlobal,     // store the top of the stack into an existing global (value stays on the stack)
    GetUpvalue,    // push the captured variable `operand` of the current closure
    SetUpvalue,    // store the top of the stack into captured variable `operand`
    Equal, NotEqual, Greater, GreaterEqual, Less, LessEqual,
    Add, Subtract, Multiply, Divide, Modulo,
    Not, Negate,
    Print,         // pop and print
    Jump,          // ip = operand
    JumpIfFalse,   // if the top of the stack is falsy, ip = operand (does not pop)
    JumpIfTrue,    // if the top of the stack is truthy, ip = operand (does not pop)
    Call,          // call the value below `operand` arguments
    Closure,       // wrap functions[operand] in a closure, capturing its upvalues, and push it
    CloseUpvalue,  // move the local on top of the stack into its upvalue, then pop it
    Return,        // pop the result, drop the frame, push the result for the caller
};

inline const char* opName(Op op) {
    static const char* names[] = {
        "Constant", "Nil", "True", "False", "Pop", "GetLocal", "SetLocal", "GetGlobal", "DefineGlobal",
        "SetGlobal", "GetUpvalue", "SetUpvalue", "Equal", "NotEqual", "Greater", "GreaterEqual", "Less",
        "LessEqual", "Add", "Subtract", "Multiply", "Divide", "Modulo", "Not", "Negate", "Print", "Jump",
        "JumpIfFalse", "JumpIfTrue", "Call", "Closure", "CloseUpvalue", "Return",
    };
    return names[static_cast<int>(op)];
}

struct Instr {
    Op op;
    int operand = 0;
    int line = 0;
};

// Where a closure finds each variable it captures when it is created:
// a local slot of the enclosing function, or one of the enclosing closure's own upvalues.
struct UpvalueRef {
    bool isLocal;
    int index;
};

// A compiled function ("prototype"). Closures created at run time share one Proto.
struct Proto {
    std::string name;
    int arity = 0;
    std::vector<Instr> code;
    std::vector<Value> constants;                  // numbers, strings, and global names
    std::vector<std::shared_ptr<Proto>> functions;  // functions declared directly inside this one
    std::vector<UpvalueRef> upvalues;
};

inline std::string formatNumber(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
    std::ostringstream out;
    out << std::setprecision(15) << d;
    return out.str();
}

// Prints a readable listing of a function and every function nested inside it.
inline void disassemble(const Proto& p, std::ostream& out) {
    out << "== " << p.name << " (arity " << p.arity << ", upvalues " << p.upvalues.size() << ") ==\n";
    for (size_t i = 0; i < p.code.size(); i++) {
        const Instr& in = p.code[i];
        out << std::setw(4) << std::setfill('0') << i << std::setfill(' ') << "  line " << std::setw(3) << in.line
            << "  ";
        std::ostringstream operand;
        switch (in.op) {
            case Op::Constant:
            case Op::GetGlobal:
            case Op::DefineGlobal:
            case Op::SetGlobal: {
                const Value& c = p.constants[in.operand];
                operand << in.operand << "  ; ";
                if (const double* n = std::get_if<double>(&c)) operand << formatNumber(*n);
                if (const std::string* s = std::get_if<std::string>(&c)) operand << '"' << *s << '"';
                break;
            }
            case Op::GetLocal:
            case Op::SetLocal:
            case Op::GetUpvalue:
            case Op::SetUpvalue:
            case Op::Call: operand << in.operand; break;
            case Op::Jump:
            case Op::JumpIfFalse:
            case Op::JumpIfTrue: operand << "-> " << std::setw(4) << std::setfill('0') << in.operand << std::setfill(' '); break;
            case Op::Closure: {
                const Proto& fn = *p.functions[in.operand];
                operand << in.operand << "  ; <fn " << fn.name << ">";
                for (const UpvalueRef& u : fn.upvalues) operand << (u.isLocal ? "  local " : "  upvalue ") << u.index;
                break;
            }
            default: break;
        }
        if (operand.str().empty()) {
            out << opName(in.op);
        } else {
            out << std::left << std::setw(13) << opName(in.op) << std::right << operand.str();
        }
        out << "\n";
    }
    for (const auto& fn : p.functions) {
        out << "\n";
        disassemble(*fn, out);
    }
}

}  // namespace vm
