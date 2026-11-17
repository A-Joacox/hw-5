#pragma once
#include <cstdint>

// Campos e inmediatos de los formatos R/I/S/B/U/J de RV32I.
namespace rv {

enum Opcode : uint32_t {
    LOAD = 0x03,
    OP_IMM = 0x13,
    AUIPC = 0x17,
    STORE = 0x23,
    OP = 0x33,
    LUI = 0x37,
    BRANCH = 0x63,
    JALR = 0x67,
    JAL = 0x6f,
    SYSTEM = 0x73,
};

inline uint32_t opcode(uint32_t i) { return i & 0x7f; }
inline uint32_t rd(uint32_t i) { return (i >> 7) & 0x1f; }
inline uint32_t funct3(uint32_t i) { return (i >> 12) & 0x7; }
inline uint32_t rs1(uint32_t i) { return (i >> 15) & 0x1f; }
inline uint32_t rs2(uint32_t i) { return (i >> 20) & 0x1f; }
inline uint32_t funct7(uint32_t i) { return i >> 25; }

// Los inmediatos se extienden en signo usando shift aritmético sobre int32_t.
inline int32_t imm_i(uint32_t i) { return int32_t(i) >> 20; }
inline int32_t imm_s(uint32_t i) {
    return (int32_t(i) >> 25 << 5) | int32_t((i >> 7) & 0x1f);
}
inline int32_t imm_b(uint32_t i) {
    return (int32_t(i & 0x80000000) >> 19)  // imm[12]
         | int32_t((i & 0x80) << 4)         // imm[11]
         | int32_t((i >> 20) & 0x7e0)       // imm[10:5]
         | int32_t((i >> 7) & 0x1e);        // imm[4:1]
}
inline uint32_t imm_u(uint32_t i) { return i & 0xfffff000; }
inline int32_t imm_j(uint32_t i) {
    return (int32_t(i & 0x80000000) >> 11)  // imm[20]
         | int32_t(i & 0xff000)             // imm[19:12]
         | int32_t((i >> 9) & 0x800)        // imm[11]
         | int32_t((i >> 20) & 0x7fe);      // imm[10:1]
}

inline constexpr const char* ABI_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

}  // namespace rv
