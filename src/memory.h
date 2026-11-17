#pragma once
#include <array>
#include <cstdint>
#include <unordered_map>

// Memoria plana byte-addressable de 32 bits, little-endian.
// Se reserva por páginas de 4 KiB solo cuando se escriben; lo no escrito vale 0.
class Memory {
public:
    uint8_t read8(uint32_t addr) const {
        auto it = pages_.find(addr >> PAGE_BITS);
        return it == pages_.end() ? 0 : it->second[addr & PAGE_MASK];
    }
    void write8(uint32_t addr, uint8_t value) {
        pages_[addr >> PAGE_BITS][addr & PAGE_MASK] = value;
    }

    uint16_t read16(uint32_t addr) const {
        return uint16_t(read8(addr) | read8(addr + 1) << 8);
    }
    uint32_t read32(uint32_t addr) const {
        return uint32_t(read16(addr)) | uint32_t(read16(addr + 2)) << 16;
    }
    void write16(uint32_t addr, uint16_t value) {
        write8(addr, uint8_t(value));
        write8(addr + 1, uint8_t(value >> 8));
    }
    void write32(uint32_t addr, uint32_t value) {
        write16(addr, uint16_t(value));
        write16(addr + 2, uint16_t(value >> 16));
    }

    void clear() { pages_.clear(); }

private:
    static constexpr uint32_t PAGE_BITS = 12;
    static constexpr uint32_t PAGE_SIZE = 1u << PAGE_BITS;
    static constexpr uint32_t PAGE_MASK = PAGE_SIZE - 1;
    // std::array se value-inicializa (ceros) al crearse con operator[].
    std::unordered_map<uint32_t, std::array<uint8_t, PAGE_SIZE>> pages_;
};
