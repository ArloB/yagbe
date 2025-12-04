#ifndef APU_HPP
#define APU_HPP

#include <cstdint>
#include <array>

class APU {
public:
    APU();
    ~APU() = default;

    void step(uint8_t cycles);
    uint8_t readRegister(uint16_t addr);
    void writeRegister(uint16_t addr, uint8_t value);

private:
    std::array<uint8_t, 0x30> registers;
};

#endif