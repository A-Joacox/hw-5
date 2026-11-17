#pragma once
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "memory.h"

// Error de ejecución del simulador (instrucción inválida, PC desalineado, etc.).
struct SimError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Estado arquitectural de una CPU RV32I: PC, 32 registros y memoria.
class CPU {
public:
    uint32_t pc = 0;
    uint32_t x[32] = {};
    Memory mem;

    bool halted = false;  // true tras ecall exit o ebreak
    int exit_code = 0;

    // Entrada/salida usadas por ecall (reemplazables en tests).
    std::istream* in = &std::cin;
    std::ostream* out = &std::cout;

    // Pone registros en cero y el PC en `entry`. No toca la memoria.
    void reset(uint32_t entry = 0);

    // Fetch + decode + execute de una instrucción. Lanza SimError si falla.
    void step();

private:
    void write_reg(uint32_t r, uint32_t value) {
        if (r != 0) x[r] = value;
    }
    void ecall();
};

std::string hex32(uint32_t value);
