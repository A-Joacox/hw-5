#pragma once
#include <cstdint>
#include <string>

#include "memory.h"

struct LoadInfo {
    uint32_t entry;      // PC inicial
    std::string format;  // "binario crudo" o "ELF32"
    uint32_t bytes;      // bytes copiados a memoria
};

// Carga `path` en memoria. Si empieza con la firma ELF se cargan sus segmentos
// PT_LOAD; si no, se trata como binario crudo y se copia a partir de 0x00000000.
// Lanza SimError si el archivo no se puede leer o es un ELF no soportado.
LoadInfo load_program(const std::string& path, Memory& mem);
