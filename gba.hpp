#ifndef GBA_H
#define GBA_H

#include <array>
#include <memory>
#include <string>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_messagebox.h>

#include "timer.hpp"
#include "apu.hpp"

#if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define GBA_LITTLE_ENDIAN 1
#else
    #define GBA_LITTLE_ENDIAN 0
#endif

#if defined(_MSC_VER)
    #define GBA_FORCE_INLINE __forceinline
    #define GBA_INLINE_ATTR
#elif defined(__GNUC__) || defined(__clang__)
    #define GBA_FORCE_INLINE inline __attribute__((always_inline))
    #define GBA_INLINE_ATTR __attribute__((always_inline))
#else
    #define GBA_FORCE_INLINE inline
    #define GBA_INLINE_ATTR
#endif

/**
 * @brief 16-bit CPU register with byte access.
 */
class Register {
public:
    uint16_t word;
    
    Register() : word(0) {}
    Register(const Register& other) : word(other.word) {}
    Register& operator=(const Register& other) {
        word = other.word;
        return *this;
    }
    

    uint8_t getLo() const {
        return static_cast<uint8_t>(word & 0xFF);
    }
    
    void setLo(uint8_t byte) {
        word = (word & 0xFF00) | static_cast<uint16_t>(byte);
    }
    
    uint8_t getHi() const {
        return static_cast<uint8_t>((word >> 8) & 0xFF);
    }
    
    void setHi(uint8_t byte) {
        word = (word & 0x00FF) | (static_cast<uint16_t>(byte) << 8);
    }
};

class ByteProxy {
    Register* reg_ptr;
    bool is_high;
    
public:
    GBA_INLINE_ATTR
    ByteProxy(Register* ptr, bool high) : reg_ptr(ptr), is_high(high) {}
    
    GBA_FORCE_INLINE operator uint8_t() const {
        return is_high ? reg_ptr->getHi() : reg_ptr->getLo();
    }
    
    GBA_FORCE_INLINE ByteProxy& operator=(uint8_t val) {
        if (is_high) {
            reg_ptr->setHi(val);
        } else {
            reg_ptr->setLo(val);
        }
        return *this;
    }
    
    GBA_FORCE_INLINE ByteProxy& operator=(const ByteProxy& other) {
        *this = static_cast<uint8_t>(other);
        return *this;
    }
    
    GBA_FORCE_INLINE ByteProxy& operator+=(uint8_t val) { 
        *this = static_cast<uint8_t>(*this) + val; 
        return *this; 
    }

    GBA_FORCE_INLINE ByteProxy& operator-=(uint8_t val) { 
        *this = static_cast<uint8_t>(*this) - val; 
        return *this; 
    }

    GBA_FORCE_INLINE ByteProxy& operator++() { 
        *this = static_cast<uint8_t>(*this) + 1; 
        return *this; 
    }

    GBA_FORCE_INLINE uint8_t operator++(int) { 
        uint8_t old = static_cast<uint8_t>(*this); 
        ++(*this); 
        return old; 
    }

    GBA_FORCE_INLINE ByteProxy& operator--() { 
        *this = static_cast<uint8_t>(*this) - 1; 
        return *this; 
    }

    GBA_FORCE_INLINE uint8_t operator--(int) { 
        uint8_t old = static_cast<uint8_t>(*this); 
        --(*this); 
        return old; 
    }
};

namespace RegisterAccess {    
    GBA_FORCE_INLINE ByteProxy getLo(Register& reg) {
        return ByteProxy(&reg, false);
    }
    
    GBA_FORCE_INLINE ByteProxy getHi(Register& reg) {
        return ByteProxy(&reg, true);
    }
}

class FlagProxy {
    Register* reg_ptr;
    uint8_t bit_pos;
public:
    GBA_INLINE_ATTR
    FlagProxy(Register* ptr, uint8_t bit) : reg_ptr(ptr), bit_pos(bit) {}
    
    GBA_FORCE_INLINE operator bool() const { 
        uint8_t f_byte = reg_ptr->getLo();
        return (f_byte & (1U << bit_pos)) != 0; 
    }
    
    GBA_FORCE_INLINE FlagProxy& operator=(bool v) {
        uint8_t f_byte = reg_ptr->getLo();
        if (v) {
            f_byte |= (1U << bit_pos);
        } else {
            f_byte &= ~(1U << bit_pos);
        }
        f_byte &= 0xF0;
        reg_ptr->setLo(f_byte);
        return *this;
    }
    
    GBA_FORCE_INLINE FlagProxy& operator=(const FlagProxy& other) {
        *this = static_cast<bool>(other);
        return *this;
    }
};

/**
 * @brief Array of CPU registers (AF, BC, DE, HL, PC, SP).
 * AF is registers[0], BC is registers[1], etc.
 */
inline std::array<Register, 6> registers;
/**
 * @brief Global unique pointer to the Timer object.
 */
inline std::unique_ptr<Timer> timer;
/**
 * @brief Global unique pointer to the APU object.
 */
inline std::unique_ptr<APU> apu;
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

inline bool halt_bug = false;

inline bool shouldExit = false;

inline std::string romPath;

// Registers
#define $A  RegisterAccess::getHi(registers[0])
#define $B  RegisterAccess::getHi(registers[1])
#define $C  RegisterAccess::getLo(registers[1])
#define $BC registers[1].word
#define $D  RegisterAccess::getHi(registers[2])
#define $E  RegisterAccess::getLo(registers[2])
#define $DE registers[2].word
#define $F  RegisterAccess::getLo(registers[0])
#define $Z  FlagProxy(&registers[0], 7)
#define $N  FlagProxy(&registers[0], 6)
#define $HF  FlagProxy(&registers[0], 5)
#define $CR  FlagProxy(&registers[0], 4)
#define $AF registers[0].word
#define $H  RegisterAccess::getHi(registers[3])
#define $L  RegisterAccess::getLo(registers[3])
#define $HL registers[3].word
#define $PC registers[4].word
#define $SP registers[5].word

#endif