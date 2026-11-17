#pragma once
#include <cstdint>
#include <string>

// Devuelve el texto ensamblador de `inst` (los saltos se muestran con dirección absoluta).
std::string disassemble(uint32_t inst, uint32_t pc);
