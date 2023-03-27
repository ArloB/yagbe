#ifndef GBA_H
#define GBA_H

#include <array>
#include <vector>
#include <memory>

#include "memory.hpp"
#include "timer.hpp"

union Register {
    uint16_t word;
#pragma pack(2)
    struct {
        uint8_t lo;
        uint8_t hi;
    } bytes;
    struct {
        unsigned int : 4;
        unsigned int carry : 1;
        unsigned int half : 1;
        unsigned int subtract : 1;
        unsigned int zero : 1;
        unsigned int : 8;
    } flags;
#pragma pack()
};

inline std::unique_ptr<Mem> memory;
inline std::array< Register, 6 > registers;
inline std::unique_ptr<Timer> timer;

inline bool ime_sched = false;
inline bool IME = true;
inline bool halted = false;

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