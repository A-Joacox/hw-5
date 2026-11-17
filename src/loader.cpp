#include "loader.h"

#include <fstream>
#include <iterator>
#include <vector>

#include "cpu.h"

namespace {

using Bytes = std::vector<uint8_t>;

uint16_t le16(const Bytes& d, size_t off) {
    if (off + 2 > d.size()) throw SimError("ELF truncado");
    return uint16_t(d[off] | d[off + 1] << 8);
}
uint32_t le32(const Bytes& d, size_t off) {
    return uint32_t(le16(d, off)) | uint32_t(le16(d, off + 2)) << 16;
}

LoadInfo load_elf(const Bytes& d, Memory& mem) {
    // e_ident: EI_CLASS (1 = 32 bits), EI_DATA (1 = little-endian)
    if (d.size() < 52 || d[4] != 1 || d[5] != 1)
        throw SimError("solo se soportan ELF de 32 bits little-endian");
    if (le16(d, 18) != 243) throw SimError("el ELF no es para RISC-V (e_machine != 243)");

    const uint32_t entry = le32(d, 24);
    const uint32_t phoff = le32(d, 28);
    const uint16_t phentsize = le16(d, 42), phnum = le16(d, 44);

    uint32_t total = 0;
    for (uint32_t i = 0; i < phnum; ++i) {
        const size_t ph = size_t(phoff) + size_t(i) * phentsize;
        if (le32(d, ph) != 1) continue;  // solo PT_LOAD
        const uint32_t offset = le32(d, ph + 4), vaddr = le32(d, ph + 8);
        const uint32_t filesz = le32(d, ph + 16), memsz = le32(d, ph + 20);
        if (size_t(offset) + filesz > d.size()) throw SimError("segmento ELF fuera del archivo");
        for (uint32_t j = 0; j < memsz; ++j)  // lo que excede filesz (.bss) queda en 0
            mem.write8(vaddr + j, j < filesz ? d[offset + j] : 0);
        total += memsz;
    }
    return {entry, "ELF32", total};
}

}  // namespace

LoadInfo load_program(const std::string& path, Memory& mem) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw SimError("no se pudo abrir '" + path + "'");
    const Bytes data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (data.size() >= 4 && data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')
        return load_elf(data, mem);

    for (uint32_t i = 0; i < data.size(); ++i) mem.write8(i, data[i]);
    return {0, "binario crudo", uint32_t(data.size())};
}
