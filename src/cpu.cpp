#include "cpu.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "decode.h"

std::string hex32(uint32_t value) {
    std::ostringstream o;
    o << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return o.str();
}

void CPU::reset(uint32_t entry) {
    std::fill(std::begin(x), std::end(x), 0);
    pc = entry;
    halted = false;
    exit_code = 0;
}

void CPU::step() {
    using namespace rv;

    if (halted) throw SimError("el programa ya terminó (usa 'reset' para reiniciar)");
    if (pc & 3) throw SimError("PC desalineado: " + hex32(pc));

    const uint32_t inst = mem.read32(pc);
    const uint32_t r_d = rd(inst), f3 = funct3(inst), f7 = funct7(inst);
    const uint32_t a = x[rs1(inst)], b = x[rs2(inst)];
    uint32_t next = pc + 4;

    auto invalid = [&]() {
        std::string msg = "instrucción inválida " + hex32(inst) + " en PC=" + hex32(pc);
        if (inst == 0) msg += " (¿se llegó al final del programa?)";
        throw SimError(msg);
    };

    switch (opcode(inst)) {
    case LUI:
        write_reg(r_d, imm_u(inst));
        break;
    case AUIPC:
        write_reg(r_d, pc + imm_u(inst));
        break;
    case JAL:
        write_reg(r_d, next);
        next = pc + imm_j(inst);
        break;
    case JALR: {
        if (f3 != 0) invalid();
        uint32_t target = (a + imm_i(inst)) & ~1u;  // se calcula antes por si rd == rs1
        write_reg(r_d, next);
        next = target;
        break;
    }
    case BRANCH: {
        bool taken;
        switch (f3) {
        case 0: taken = a == b; break;                    // beq
        case 1: taken = a != b; break;                    // bne
        case 4: taken = int32_t(a) < int32_t(b); break;   // blt
        case 5: taken = int32_t(a) >= int32_t(b); break;  // bge
        case 6: taken = a < b; break;                     // bltu
        case 7: taken = a >= b; break;                    // bgeu
        default: invalid(); return;
        }
        if (taken) next = pc + imm_b(inst);
        break;
    }
    case LOAD: {
        uint32_t addr = a + imm_i(inst);
        uint32_t v;
        switch (f3) {
        case 0: v = uint32_t(int32_t(int8_t(mem.read8(addr)))); break;    // lb
        case 1: v = uint32_t(int32_t(int16_t(mem.read16(addr)))); break;  // lh
        case 2: v = mem.read32(addr); break;                              // lw
        case 4: v = mem.read8(addr); break;                               // lbu
        case 5: v = mem.read16(addr); break;                              // lhu
        default: invalid(); return;
        }
        write_reg(r_d, v);
        break;
    }
    case STORE: {
        uint32_t addr = a + imm_s(inst);
        switch (f3) {
        case 0: mem.write8(addr, uint8_t(b)); break;    // sb
        case 1: mem.write16(addr, uint16_t(b)); break;  // sh
        case 2: mem.write32(addr, b); break;            // sw
        default: invalid(); return;
        }
        break;
    }
    case OP_IMM: {
        const int32_t imm = imm_i(inst);
        const uint32_t shamt = rs2(inst);
        uint32_t v;
        switch (f3) {
        case 0: v = a + imm; break;                                        // addi
        case 2: v = int32_t(a) < imm; break;                               // slti
        case 3: v = a < uint32_t(imm); break;                              // sltiu
        case 4: v = a ^ imm; break;                                        // xori
        case 6: v = a | imm; break;                                        // ori
        case 7: v = a & imm; break;                                        // andi
        case 1:                                                            // slli
            if (f7 != 0) invalid();
            v = a << shamt;
            break;
        case 5:
            if (f7 == 0x00) v = a >> shamt;                                // srli
            else if (f7 == 0x20) v = uint32_t(int32_t(a) >> shamt);        // srai
            else { invalid(); return; }
            break;
        default: invalid(); return;
        }
        write_reg(r_d, v);
        break;
    }
    case OP: {
        const uint32_t sh = b & 0x1f;
        uint32_t v;
        if (f7 == 0x00) {
            switch (f3) {
            case 0: v = a + b; break;                     // add
            case 1: v = a << sh; break;                   // sll
            case 2: v = int32_t(a) < int32_t(b); break;   // slt
            case 3: v = a < b; break;                     // sltu
            case 4: v = a ^ b; break;                     // xor
            case 5: v = a >> sh; break;                   // srl
            case 6: v = a | b; break;                     // or
            case 7: v = a & b; break;                     // and
            default: invalid(); return;
            }
        } else if (f7 == 0x20 && f3 == 0) {
            v = a - b;                                    // sub
        } else if (f7 == 0x20 && f3 == 5) {
            v = uint32_t(int32_t(a) >> sh);               // sra
        } else {
            invalid();
            return;
        }
        write_reg(r_d, v);
        break;
    }
    case SYSTEM:
        if (inst == 0x00000073) ecall();
        else if (inst == 0x00100073) halted = true;  // ebreak
        else invalid();
        break;
    default:
        invalid();
    }

    pc = next;
}

// Subconjunto de las environment calls de SPIM/CPUlator. Código en a7 (x17).
void CPU::ecall() {
    const uint32_t a0 = x[10], a1 = x[11];
    switch (x[17]) {
    case 1:  // print int
        *out << int32_t(a0) << std::flush;
        break;
    case 4:  // print string (terminado en '\0')
        for (uint32_t p = a0; uint8_t c = mem.read8(p); ++p) *out << char(c);
        *out << std::flush;
        break;
    case 5: {  // read int
        int32_t v = 0;
        *in >> v;
        in->ignore(1 << 20, '\n');
        write_reg(10, uint32_t(v));
        break;
    }
    case 8: {  // read string: buffer en a0, tamaño máximo en a1
        std::string line;
        std::getline(*in, line);
        if (a1 == 0) break;
        uint32_t n = std::min<uint32_t>(uint32_t(line.size()), a1 - 1);
        for (uint32_t i = 0; i < n; ++i) mem.write8(a0 + i, uint8_t(line[i]));
        mem.write8(a0 + n, 0);
        break;
    }
    case 10:  // exit
        halted = true;
        exit_code = 0;
        break;
    case 11:  // print char
        *out << char(a0) << std::flush;
        break;
    case 12: {  // read char
        char c = 0;
        in->get(c);
        write_reg(10, uint32_t(uint8_t(c)));
        break;
    }
    case 17:  // exit2 (código de salida en a0)
    case 93:  // exit de Linux/newlib
        halted = true;
        exit_code = int32_t(a0);
        break;
    default:
        throw SimError("ecall no soportada: a7=" + std::to_string(x[17]) + " en PC=" + hex32(pc));
    }
}
