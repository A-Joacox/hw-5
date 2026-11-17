// Tests unitarios del núcleo: cada instrucción RV32I, little-endian, ecall y desensamblador.
// Las instrucciones se codifican a mano con los helpers R/I/S/B/U/J.
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>

#include "cpu.h"
#include "disasm.h"

static int failures = 0, checks = 0;
#define CHECK_EQ(a, b)                                                                   \
    do {                                                                                 \
        ++checks;                                                                        \
        auto va_ = (a);                                                                  \
        auto vb_ = (b);                                                                  \
        if (!(va_ == vb_)) {                                                             \
            ++failures;                                                                  \
            std::printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #a " == " #b);          \
        }                                                                                \
    } while (0)

// --- Codificadores de formatos ---
uint32_t R(uint32_t f7, uint32_t rs2, uint32_t rs1, uint32_t f3, uint32_t rd) {
    return f7 << 25 | rs2 << 20 | rs1 << 15 | f3 << 12 | rd << 7 | 0x33;
}
uint32_t I(int32_t imm, uint32_t rs1, uint32_t f3, uint32_t rd, uint32_t op = 0x13) {
    return uint32_t(imm & 0xfff) << 20 | rs1 << 15 | f3 << 12 | rd << 7 | op;
}
uint32_t S(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t f3) {
    uint32_t u = uint32_t(imm);
    return (u >> 5 & 0x7f) << 25 | rs2 << 20 | rs1 << 15 | f3 << 12 | (u & 0x1f) << 7 | 0x23;
}
uint32_t B(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t f3) {
    uint32_t u = uint32_t(imm);
    return (u >> 12 & 1) << 31 | (u >> 5 & 0x3f) << 25 | rs2 << 20 | rs1 << 15 | f3 << 12 |
           (u >> 1 & 0xf) << 8 | (u >> 11 & 1) << 7 | 0x63;
}
uint32_t U(uint32_t imm20, uint32_t rd, uint32_t op) { return imm20 << 12 | rd << 7 | op; }
uint32_t J(int32_t imm, uint32_t rd) {
    uint32_t u = uint32_t(imm);
    return (u >> 20 & 1) << 31 | (u >> 1 & 0x3ff) << 21 | (u >> 11 & 1) << 20 | (u >> 12 & 0xff) << 12 |
           rd << 7 | 0x6f;
}
uint32_t ADDI(uint32_t rd, uint32_t rs1, int32_t imm) { return I(imm, rs1, 0, rd); }
uint32_t LOAD(uint32_t f3, uint32_t rd, uint32_t rs1, int32_t imm) { return I(imm, rs1, f3, rd, 0x03); }

// Carga el programa en 0 y ejecuta todas sus instrucciones (o `steps` si se indica).
void run(CPU& cpu, std::initializer_list<uint32_t> prog, int steps = -1) {
    cpu.mem.clear();
    cpu.reset();
    uint32_t a = 0;
    for (uint32_t w : prog) cpu.mem.write32(a, w), a += 4;
    for (int i = 0; i < (steps < 0 ? int(prog.size()) : steps); ++i) cpu.step();
}

int main() {
    CPU c;

    // ALU con inmediatos
    run(c, {ADDI(1, 0, -5), ADDI(2, 1, 7), I(-1, 1, 2, 3), I(-1, 1, 3, 4), I(0x0f0, 2, 4, 5),
            I(0x0f0, 2, 6, 6), I(0x003, 1, 7, 7), ADDI(0, 0, 5)});
    CHECK_EQ(c.x[1], uint32_t(-5));
    CHECK_EQ(c.x[2], 2u);
    CHECK_EQ(c.x[3], 1u);        // slti  -5 < -1
    CHECK_EQ(c.x[4], 1u);        // sltiu 0xFFFFFFFB < 0xFFFFFFFF
    CHECK_EQ(c.x[5], 0xf2u);     // xori
    CHECK_EQ(c.x[6], 0xf2u);     // ori
    CHECK_EQ(c.x[7], 3u);        // andi -5 & 3
    CHECK_EQ(c.x[0], 0u);        // x0 no se modifica

    // Shifts inmediatos
    run(c, {ADDI(1, 0, -16), I(4, 1, 1, 2), I(4, 1, 5, 3), I(0x400 | 4, 1, 5, 4)});
    CHECK_EQ(c.x[2], uint32_t(-256));  // slli
    CHECK_EQ(c.x[3], 0x0FFFFFFFu);     // srli
    CHECK_EQ(c.x[4], uint32_t(-1));    // srai

    // ALU registro-registro
    run(c, {ADDI(1, 0, -3), ADDI(2, 0, 36), R(0, 2, 1, 0, 3), R(0x20, 2, 1, 0, 4), R(0, 2, 1, 2, 5),
            R(0, 2, 1, 3, 6), R(0, 2, 1, 4, 7), R(0, 2, 1, 6, 8), R(0, 2, 1, 7, 9),
            R(0, 2, 1, 1, 10), R(0, 2, 1, 5, 11), R(0x20, 2, 1, 5, 12)});
    CHECK_EQ(c.x[3], 33u);                         // add
    CHECK_EQ(c.x[4], uint32_t(-39));               // sub
    CHECK_EQ(c.x[5], 1u);                          // slt  -3 < 36
    CHECK_EQ(c.x[6], 0u);                          // sltu 0xFFFFFFFD < 36 es falso
    CHECK_EQ(c.x[7], uint32_t(-3) ^ 36u);          // xor
    CHECK_EQ(c.x[8], uint32_t(-3) | 36u);          // or
    CHECK_EQ(c.x[9], uint32_t(-3) & 36u);          // and
    CHECK_EQ(c.x[10], uint32_t(-3) << 4);          // sll usa solo 5 bits (36 & 31 = 4)
    CHECK_EQ(c.x[11], uint32_t(-3) >> 4);          // srl
    CHECK_EQ(c.x[12], uint32_t(-1));               // sra

    // lui / auipc
    run(c, {U(0x12345, 1, 0x37), U(0x00001, 2, 0x17)});
    CHECK_EQ(c.x[1], 0x12345000u);
    CHECK_EQ(c.x[2], 0x1004u);  // pc (4) + 0x1000

    // Loads/stores y little-endian
    run(c, {U(0x87654, 1, 0x37), ADDI(1, 1, 0x321), ADDI(2, 0, 0x100), S(0, 1, 2, 2),
            LOAD(0, 3, 2, 0), LOAD(4, 4, 2, 0), LOAD(0, 5, 2, 1), LOAD(1, 6, 2, 2), LOAD(5, 7, 2, 2),
            LOAD(2, 8, 2, 0), S(8, 1, 2, 0), S(12, 1, 2, 1)});
    CHECK_EQ(c.x[1], 0x87654321u);
    CHECK_EQ(c.mem.read8(0x100), 0x21);   // byte menos significativo primero
    CHECK_EQ(c.mem.read8(0x103), 0x87);
    CHECK_EQ(c.x[3], 0x21u);              // lb positivo
    CHECK_EQ(c.x[4], 0x21u);              // lbu
    CHECK_EQ(c.x[5], 0x43u);              // lb en 0x101
    CHECK_EQ(c.x[6], 0xFFFF8765u);        // lh con signo
    CHECK_EQ(c.x[7], 0x8765u);            // lhu
    CHECK_EQ(c.x[8], 0x87654321u);        // lw
    CHECK_EQ(c.mem.read32(0x108), 0x21u); // sb
    CHECK_EQ(c.mem.read32(0x10c), 0x4321u); // sh
    run(c, {ADDI(1, 0, 0x80), ADDI(2, 0, 0x100), S(-4, 1, 2, 0), LOAD(0, 3, 2, -4)});
    CHECK_EQ(c.x[3], 0xFFFFFF80u);        // lb negativo con offset negativo

    // Branches: cada uno salta sobre un "addi x10, x10, 1" si se toma.
    struct Case { uint32_t f3; int32_t a, b; bool taken; };
    for (Case k : {Case{0, 5, 5, true}, Case{0, 5, 6, false}, Case{1, 5, 6, true}, Case{1, 5, 5, false},
                   Case{4, -1, 0, true}, Case{4, 0, -1, false}, Case{5, 0, -1, true}, Case{5, -1, 0, false},
                   Case{6, 0, -1, true}, Case{6, -1, 0, false}, Case{7, -1, 0, true}, Case{7, 0, -1, false}}) {
        run(c, {ADDI(1, 0, k.a), ADDI(2, 0, k.b), B(8, 2, 1, k.f3), ADDI(10, 10, 1), ADDI(11, 0, 1)}, 4);
        CHECK_EQ(c.x[10], k.taken ? 0u : 1u);
    }
    // Branch hacia atrás
    run(c, {ADDI(1, 0, 3), ADDI(1, 1, -1), B(-4, 0, 1, 1)}, 1 + 2 * 3);
    CHECK_EQ(c.x[1], 0u);
    CHECK_EQ(c.pc, 12u);

    // jal / jalr
    run(c, {J(8, 1), ADDI(5, 0, 1), ADDI(6, 0, 1)}, 2);
    CHECK_EQ(c.x[1], 4u);
    CHECK_EQ(c.x[5], 0u);
    CHECK_EQ(c.x[6], 1u);
    run(c, {ADDI(1, 0, 13), I(0, 1, 0, 1, 0x67)}, 2);  // jalr ra, 0(ra): bit 0 se limpia, rd == rs1
    CHECK_EQ(c.pc, 12u);
    CHECK_EQ(c.x[1], 8u);

    // Instrucción inválida y PC desalineado lanzan SimError
    bool threw = false;
    try { run(c, {0xFFFFFFFF}); } catch (const SimError&) { threw = true; }
    CHECK_EQ(threw, true);
    threw = false;
    try { run(c, {ADDI(1, 0, 2), I(0, 1, 0, 0, 0x67), ADDI(0, 0, 0)}, 3); } catch (const SimError&) { threw = true; }
    CHECK_EQ(threw, true);

    // ecall: print int, print string, read int, exit2
    std::ostringstream out;
    std::istringstream in("42\n");
    c.out = &out;
    c.in = &in;
    run(c, {ADDI(10, 0, -7), ADDI(17, 0, 1), 0x73,
            ADDI(1, 0, 'o'), ADDI(2, 0, 0x200), S(0, 1, 2, 0), ADDI(1, 0, 'k'), S(1, 1, 2, 0),
            ADDI(10, 0, 0x200), ADDI(17, 0, 4), 0x73,
            ADDI(17, 0, 5), 0x73, ADDI(5, 10, 0),
            ADDI(10, 0, 3), ADDI(17, 0, 17), 0x73});
    CHECK_EQ(out.str(), std::string("-7ok"));
    CHECK_EQ(c.x[5], 42u);
    CHECK_EQ(c.halted, true);
    CHECK_EQ(c.exit_code, 3);
    c.out = &std::cout;
    c.in = &std::cin;

    // Desensamblador
    CHECK_EQ(disassemble(ADDI(2, 2, -16), 0), std::string("addi sp, sp, -16"));
    CHECK_EQ(disassemble(S(12, 1, 2, 2), 0), std::string("sw ra, 12(sp)"));
    CHECK_EQ(disassemble(B(-8, 6, 5, 0), 0x20), std::string("beq t0, t1, 0x00000018"));
    CHECK_EQ(disassemble(R(0x20, 2, 1, 5, 3), 0), std::string("sra gp, ra, sp"));
    CHECK_EQ(disassemble(0x00000073, 0), std::string("ecall"));
    CHECK_EQ(disassemble(0xFFFFFFFF, 0), std::string("<inválida>"));

    std::printf("%d/%d checks OK\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
