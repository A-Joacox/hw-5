#include "disasm.h"

#include <sstream>

#include "cpu.h"
#include "decode.h"

std::string disassemble(uint32_t inst, uint32_t pc) {
    using namespace rv;
    static const char* const BRANCH_OPS[8] = {"beq", "bne", nullptr, nullptr, "blt", "bge", "bltu", "bgeu"};
    static const char* const LOAD_OPS[8] = {"lb", "lh", "lw", nullptr, "lbu", "lhu", nullptr, nullptr};
    static const char* const STORE_OPS[8] = {"sb", "sh", "sw", nullptr, nullptr, nullptr, nullptr, nullptr};
    static const char* const IMM_OPS[8] = {"addi", "slli", "slti", "sltiu", "xori", nullptr, "ori", "andi"};
    static const char* const REG_OPS[8] = {"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"};

    const char* RD = ABI_NAMES[rd(inst)];
    const char* RS1 = ABI_NAMES[rs1(inst)];
    const char* RS2 = ABI_NAMES[rs2(inst)];
    const uint32_t f3 = funct3(inst), f7 = funct7(inst);
    const std::string INVALID = "<inválida>";
    std::ostringstream o;

    switch (opcode(inst)) {
    case LUI:
    case AUIPC:
        o << (opcode(inst) == LUI ? "lui " : "auipc ") << RD << ", 0x" << std::hex << (imm_u(inst) >> 12);
        break;
    case JAL:
        o << "jal " << RD << ", " << hex32(pc + imm_j(inst));
        break;
    case JALR:
        if (f3 != 0) return INVALID;
        o << "jalr " << RD << ", " << imm_i(inst) << "(" << RS1 << ")";
        break;
    case BRANCH:
        if (!BRANCH_OPS[f3]) return INVALID;
        o << BRANCH_OPS[f3] << " " << RS1 << ", " << RS2 << ", " << hex32(pc + imm_b(inst));
        break;
    case LOAD:
        if (!LOAD_OPS[f3]) return INVALID;
        o << LOAD_OPS[f3] << " " << RD << ", " << imm_i(inst) << "(" << RS1 << ")";
        break;
    case STORE:
        if (!STORE_OPS[f3]) return INVALID;
        o << STORE_OPS[f3] << " " << RS2 << ", " << imm_s(inst) << "(" << RS1 << ")";
        break;
    case OP_IMM:
        if (f3 == 1 || f3 == 5) {
            const char* name = f3 == 1 ? (f7 == 0 ? "slli" : nullptr)
                             : f7 == 0 ? "srli" : f7 == 0x20 ? "srai" : nullptr;
            if (!name) return INVALID;
            o << name << " " << RD << ", " << RS1 << ", " << rs2(inst);
        } else {
            o << IMM_OPS[f3] << " " << RD << ", " << RS1 << ", " << imm_i(inst);
        }
        break;
    case OP: {
        const char* name = f7 == 0 ? REG_OPS[f3]
                         : f7 == 0x20 && f3 == 0 ? "sub"
                         : f7 == 0x20 && f3 == 5 ? "sra" : nullptr;
        if (!name) return INVALID;
        o << name << " " << RD << ", " << RS1 << ", " << RS2;
        break;
    }
    case SYSTEM:
        if (inst == 0x00000073) return "ecall";
        if (inst == 0x00100073) return "ebreak";
        return INVALID;
    default:
        return INVALID;
    }
    return o.str();
}
