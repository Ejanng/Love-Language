#include "chunk.h"

const char* op_name(Op op) {
    static const char* names[] = {
        "Constant", "Nil", "True", "False", "Pop", "GetLocal", "SetLocal", "GetGlobal", "DefineGlobal",
        "SetGlobal", "GetUpvalue", "SetUpvalue", "Equal", "NotEqual", "Greater", "GreaterEqual", "Less",
        "LessEqual", "Add", "Subtract", "Multiply", "Divide", "Modulo", "Not", "Negate", "Print", "Jump",
        "JumpIfFalse", "JumpIfTrue", "Call", "Closure", "CloseUpvalue", "Return",
    };
    return names[op];
}

void disassemble(const Proto* p, FILE* out) {
    fprintf(out, "== %s (arity %d, upvalues %d) ==\n", p->name, p->arity, p->upvalueCount);
    for (int i = 0; i < p->codeCount; i++) {
        const Instr* in = &p->code[i];
        fprintf(out, "%04d  line %3d  ", i, in->line);
        switch (in->op) {
            case OP_CONSTANT:
            case OP_GET_GLOBAL:
            case OP_DEFINE_GLOBAL:
            case OP_SET_GLOBAL: {
                const Value c = p->constants[in->operand];
                fprintf(out, "%-13s%d  ; ", op_name(in->op), in->operand);
                if (c.type == VAL_NUMBER) {
                    char buf[64];
                    format_number(buf, sizeof buf, c.as.number);
                    fputs(buf, out);
                } else if (is_obj_type(c, OBJ_STRING)) {
                    fprintf(out, "\"%s\"", AS_STRING(c)->chars);
                }
                break;
            }
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_GET_UPVALUE:
            case OP_SET_UPVALUE:
            case OP_CALL: fprintf(out, "%-13s%d", op_name(in->op), in->operand); break;
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE: fprintf(out, "%-13s-> %04d", op_name(in->op), in->operand); break;
            case OP_CLOSURE: {
                const Proto* fn = p->functions[in->operand];
                fprintf(out, "%-13s%d  ; <fn %s>", op_name(in->op), in->operand, fn->name);
                for (int u = 0; u < fn->upvalueCount; u++) {
                    fprintf(out, "%s%d", fn->upvalues[u].isLocal ? "  local " : "  upvalue ", fn->upvalues[u].index);
                }
                break;
            }
            default: fputs(op_name(in->op), out); break;
        }
        fputc('\n', out);
    }
    for (int i = 0; i < p->functionCount; i++) {
        fputc('\n', out);
        disassemble(p->functions[i], out);
    }
}
