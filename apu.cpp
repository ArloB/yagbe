#include "apu.hpp"

APU::APU() {
    registers.fill(0);
}

void APU::step(uint8_t cycles) {
    
}

uint8_t APU::readRegister(uint16_t addr) {
    // Handle reads from APU registers (0xFF10 - 0xFF3F)
    // Example:
    // if (addr >= 0xFF10 && addr <= 0xFF3F) {
    //     return registers[addr - 0xFF10];
    // }
    return 0xFF; // Default for unhandled/unreadable registers
}

void APU::writeRegister(uint16_t addr, uint8_t value) {
    // Handle writes to APU registers
    // Example:
    // if (addr >= 0xFF10 && addr <= 0xFF3F) {
    //     registers[addr - 0xFF10] = value;
        // Potentially trigger sound events or update internal state based on write
    // }
    
}