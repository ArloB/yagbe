#ifndef GBA_H
#define GBA_H

#include <array>
#include <vector>
#include <memory>

#include "memory.hpp"
#include "timer.hpp"

/**
 * @brief Union representing a 16-bit CPU register.
 * Allows access to the register as a whole 16-bit word,
 * or as two separate 8-bit bytes (high and low).
 * Also provides a bitfield structure for the F (Flags) register.
 */
union Register {
    uint16_t word;
#pragma pack(2) // Ensure byte packing for hi/lo members
    struct {
        uint8_t lo; // Low byte of the register (e.g., C, E, L, F)
        uint8_t hi; // High byte of the register (e.g., B, D, H, A)
    } bytes;
    struct {
        unsigned int : 4;        // Offset bits
        unsigned int carry : 1;   // Carry flag (C)
        unsigned int half : 1;    // Half-carry flag (H)
        unsigned int subtract : 1;// Subtract flag (N)
        unsigned int zero : 1;    // Zero flag (Z)
        unsigned int : 8;        // Offset bits
    } flags;
#pragma pack()
};

/**
 * @brief Array of CPU registers (AF, BC, DE, HL, PC, SP).
 * AF is registers[0], BC is registers[1], etc.
 */
inline std::array< Register, 6 > registers;
/**
 * @brief Global unique pointer to the Timer object.
 */
inline std::unique_ptr<Timer> timer;

/**
 * @brief Flag to schedule enabling of IME (Interrupt Master Enable) after the next instruction.
 */
inline bool ime_sched = false;
/**
 * @brief Interrupt Master Enable flag. If false, CPU will not jump to interrupt vectors.
 */
inline bool IME = true;
/**
 * @brief CPU Halted flag. Set when HALT instruction is executed.
 */
inline bool halted = false;
/**
 * @brief CPU Stopped flag. Set when STOP instruction is executed.
 */
inline bool stopped = false;

// Registers
#define $A  registers[0].bytes.hi
#define $B  registers[1].bytes.hi
#define $C  registers[1].bytes.lo
#define $BC registers[1].word
#define $D  registers[2].bytes.hi
#define $E  registers[2].bytes.lo
#define $DE registers[2].word
#define $F  registers[0].bytes.lo
#define $Z  registers[0].flags.zero
#define $N  registers[0].flags.subtract
#define $HF  registers[0].flags.half
#define $CR  registers[0].flags.carry
#define $AF registers[0].word
#define $H  registers[3].bytes.hi
#define $L  registers[3].bytes.lo
#define $HL registers[3].word
#define $PC registers[4].word
#define $SP registers[5].word

#endif