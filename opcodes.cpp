#include <cstdint>
#include "gba.hpp"

// from https://github.com/retrio/gb-test-roms/tree/master/instr_timing
const uint8_t cycles[256] = {
    1,3,2,2,1,1,2,1,5,2,2,2,1,1,2,1,
    0,3,2,2,1,1,2,1,3,2,2,2,1,1,2,1,
    2,3,2,2,1,1,2,1,2,2,2,2,1,1,2,1,
    2,3,2,2,3,3,3,1,2,2,2,2,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    2,2,2,2,2,2,0,2,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    2,3,3,4,3,4,2,4,2,4,3,0,3,6,2,4,
    2,3,3,0,3,4,2,4,2,4,3,0,3,0,2,4,
    3,3,2,0,0,4,2,4,4,1,4,0,0,0,2,4,
    3,3,2,1,0,4,2,4,3,2,4,1,0,0,2,4
};

const uint8_t cb_cycles[256] = {
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2
};

uint8_t read(uint16_t addr) {
    return memory->get(addr);
}

uint16_t read16(uint16_t addr) {
    uint8_t a = memory->get(addr);
    uint8_t b = memory->get(addr + 1);
    return a | (b << 8);
}

void write(uint16_t addr, uint8_t val) {
    memory->set(addr, val);
}

void clearFlags() {
    $Z = 0; $N = 0; $HF = 0; $CR = 0;
}

uint8_t add(uint8_t x, uint8_t y, bool carry) {
    uint8_t c = (carry ? $CR : 0);
    uint8_t res = x + c + y;

    $Z = !res;
    $N = 0;
    $HF = (x & 0xF) + (y & 0xF) + c > 0xF;
    $CR = y > UINT8_MAX - (x + c);

    return res;
}

uint8_t add(uint8_t x, uint8_t y) {
    return add(x, y, false);
}

uint16_t add(uint16_t x, uint16_t y) {
    uint16_t res = x + y;

    $N = 0;
    $HF = (x & 0xFFF) + (y & 0xFFF) > 0xFFF;
    $CR = y > UINT16_MAX - x;

    return res;
}

uint16_t add(uint16_t x, uint8_t y) {
    int16_t res = x + static_cast<int8_t>(y);

    $Z = 0;
    $N = 0;
    $HF = (x & 0xF) + (y & 0xF) > 0xF;
    $CR = ((x & 0xff) + (y & 0xff)) > 0xFF;

    return static_cast<uint16_t>(res);
}

uint8_t sub(uint8_t x, uint8_t y, bool carry) {
    uint8_t c = (carry ? $CR : 0);
    uint8_t res = x - c - y;

    $Z = !res;
    $N = 1;
    $HF = (x & 0xF) - c < (y & 0xF);
    $CR = y > (x - c);

    return res;
}

uint8_t sub(uint8_t x, uint8_t y) {
    return sub(x, y, false);
}

uint8_t inc(uint8_t x) {
    uint8_t res = x + 1;

    $Z = !res; $N = 0; $HF = (x & 0xF) + 1 > 0xF;

    return res;
}

uint16_t inc(uint16_t x) {
    return x + 1;
}

uint8_t dec(uint8_t x) {
    uint8_t res = x - 1;

    $Z = !res; $N = 1; $HF = (x & 0xF) < 1;

    return res;
}

uint16_t dec(uint16_t x) {
    return x - 1;
}

uint8_t and8(uint8_t x, uint8_t y) {
    uint8_t res = x & y;

    $Z = !res; $N = 0; $HF = 1; $CR = 0;

    return res;
}

uint8_t or8(uint8_t x, uint8_t y) {
    uint8_t res = x | y;

    $Z = !res; $N = 0; $HF = 0; $CR = 0;

    return res;
}

uint8_t xor8(uint8_t x, uint8_t y) {
    uint8_t res = x ^ y;

    $Z = !res; $N = 0; $HF = 0; $CR = 0;

    return res;
}

uint8_t swap(uint8_t x) {
    uint8_t res = (x << 4) | (x >> 4);

    $Z = !res; $N = 0; $HF = 0; $CR = 0;

    return res;
}

uint8_t sl(uint8_t x) {
    uint8_t res = x << 1;

    $Z = !res; $N = 0; $HF = 0; $CR = (x & 128) != 0;

    return res;
}

uint8_t sr(uint8_t x, bool logical) {
    uint8_t res = logical ? x >> 1 : ((x >> 1) | (x & 0x80));

    $Z = !res; $N = 0; $HF = 0; $CR = (x & 1) != 0;

    return res;
}

uint8_t rl(uint8_t x, bool carry, bool isA) {
    uint8_t res = (x << 1) | (carry ? (x & 128) != 0 : $CR);

    $Z = isA ? 0 : !res;
    
    $N = 0; $HF = 0; $CR = (x & 128) != 0;

    return res;
}

uint8_t rl(uint8_t x, bool carry) {
    return rl(x, carry, false);
}

uint8_t rr(uint8_t x, bool carry, bool isA) {
    uint8_t res = (x >> 1) | ((carry ? x & 1 : $CR) << 7);

    $Z = isA ? 0 : !res;

    $N = 0; $HF = 0; $CR = x & 1;

    return res;
}

uint8_t rr(uint8_t x, bool carry) {
    return rr(x, carry, false);
}

void test(uint8_t x, int pos) {
    $Z = ((x >> pos) & 1) == 0; $N = 0; $HF = 1;
}

uint8_t set(uint8_t x, int pos) {
    return x | (1 << pos);
}

uint8_t res(uint8_t x, int pos) {
    return x & (~(1 << pos));
}

inline void executePrefixOp(uint8_t op) {
    switch (op) {
    case 0x0:
        $B = rl($B, true);
        break;
    case 0x1:
        $C = rl($C, true);
        break;
    case 0x2:
        $D = rl($D, true);
        break;
    case 0x3:
        $E = rl($E, true);
        break;
    case 0x4:
        $H = rl($H, true);
        break;
    case 0x5:
        $L = rl($L, true);
        break;
    case 0x6:
        write($HL, rl(read($HL), true));
        break;
    case 0x7:
        $A = rl($A, true);
        break;
    case 0x8:
        $B = rr($B, true);
        break;
    case 0x9:
        $C = rr($C, true);
        break;
    case 0xa:
        $D = rr($D, true);
        break;
    case 0xb:
        $E = rr($E, true);
        break;
    case 0xc:
        $H = rr($H, true);
        break;
    case 0xd:
        $L = rr($L, true);
        break;
    case 0xe:
        write($HL, rr(read($HL), true));
        break;
    case 0xf:
        $A = rr($A, true);
        break;
    case 0x10:
        $B = rl($B, false);
        break;
    case 0x11:
        $C = rl($C, false);
        break;
    case 0x12:
        $D = rl($D, false);
        break;
    case 0x13:
        $E = rl($E, false);
        break;
    case 0x14:
        $H = rl($H, false);
        break;
    case 0x15:
        $L = rl($L, false);
        break;
    case 0x16:
        write($HL, rl(read($HL), false));
        break;
    case 0x17:
        $A = rl($A, false);
        break;
    case 0x18:
        $B = rr($B, false);
        break;
    case 0x19:
        $C = rr($C, false);
        break;
    case 0x1a:
        $D = rr($D, false);
        break;
    case 0x1b:
        $E = rr($E, false);
        break;
    case 0x1c:
        $H = rr($H, false);
        break;
    case 0x1d:
        $L = rr($L, false);
        break;
    case 0x1e:
        write($HL, rr(read($HL), false));
        break;
    case 0x1f:
        $A = rr($A, false);
        break;
    case 0x20:
        $B = sl($B);
        break;
    case 0x21:
        $C = sl($C);
        break;
    case 0x22:
        $D = sl($D);
        break;
    case 0x23:
        $E = sl($E);
        break;
    case 0x24:
        $H = sl($H);
        break;
    case 0x25:
        $L = sl($L);
        break;
    case 0x26:
        write($HL, sl(read($HL)));
        break;
    case 0x27:
        $A = sl($A);
        break;
    case 0x28:
        $B = sr($B, false);
        break;
    case 0x29:
        $C = sr($C, false);
        break;
    case 0x2a:
        $D = sr($D, false);
        break;
    case 0x2b:
        $E = sr($E, false);
        break;
    case 0x2c:
        $H = sr($H, false);
        break;
    case 0x2d:
        $L = sr($L, false);
        break;
    case 0x2e:
        write($HL, sr(read($HL), false));
        break;
    case 0x2f:
        $A = sr($A, false);
        break;
    case 0x30:
        $B = swap($B);
        break;
    case 0x31:
        $C = swap($C);
        break;
    case 0x32:
        $D = swap($D);
        break;
    case 0x33:
        $E = swap($E);
        break;
    case 0x34:
        $H = swap($H);
        break;
    case 0x35:
        $L = swap($L);
        break;
    case 0x36:
        write($HL, swap(read($HL)));
        break;
    case 0x37:
        $A = swap($A);
        break;
    case 0x38:
        $B = sr($B, true);
        break;
    case 0x39:
        $C = sr($C, true);
        break;
    case 0x3a:
        $D = sr($D, true);
        break;
    case 0x3b:
        $E = sr($E, true);
        break;
    case 0x3c:
        $H = sr($H, true);
        break;
    case 0x3d:
        $L = sr($L, true);
        break;
    case 0x3e:
        write($HL, sr(read($HL), true));
        break;
    case 0x3f:
        $A = sr($A, true);
        break;
    case 0x40:
        test($B, 0);
        break;
    case 0x41:
        test($C, 0);
        break;
    case 0x42:
        test($D, 0);
        break;
    case 0x43:
        test($E, 0);
        break;
    case 0x44:
        test($H, 0);
        break;
    case 0x45:
        test($L, 0);
        break;
    case 0x46:
        test(read($HL), 0);
        break;
    case 0x47:
        test($A, 0);
        break;
    case 0x48:
        test($B, 1);
        break;
    case 0x49:
        test($C, 1);
        break;
    case 0x4a:
        test($D, 1);
        break;
    case 0x4b:
        test($E, 1);
        break;
    case 0x4c:
        test($H, 1);
        break;
    case 0x4d:
        test($L, 1);
        break;
    case 0x4e:
        test(read($HL), 1);
        break;
    case 0x4f:
        test($A, 1);
        break;
    case 0x50:
        test($B, 2);
        break;
    case 0x51:
        test($C, 2);
        break;
    case 0x52:
        test($D, 2);
        break;
    case 0x53:
        test($E, 2);
        break;
    case 0x54:
        test($H, 2);
        break;
    case 0x55:
        test($L, 2);
        break;
    case 0x56:
        test(read($HL), 2);
        break;
    case 0x57:
        test($A, 2);
        break;
    case 0x58:
        test($B, 3);
        break;
    case 0x59:
        test($C, 3);
        break;
    case 0x5a:
        test($D, 3);
        break;
    case 0x5b:
        test($E, 3);
        break;
    case 0x5c:
        test($H, 3);
        break;
    case 0x5d:
        test($L, 3);
        break;
    case 0x5e:
        test(read($HL), 3);
        break;
    case 0x5f:
        test($A, 3);
        break;
    case 0x60:
        test($B, 4);
        break;
    case 0x61:
        test($C, 4);
        break;
    case 0x62:
        test($D, 4);
        break;
    case 0x63:
        test($E, 4);
        break;
    case 0x64:
        test($H, 4);
        break;
    case 0x65:
        test($L, 4);
        break;
    case 0x66:
        test(read($HL), 4);
        break;
    case 0x67:
        test($A, 4);
        break;
    case 0x68:
        test($B, 5);
        break;
    case 0x69:
        test($C, 5);
        break;
    case 0x6a:
        test($D, 5);
        break;
    case 0x6b:
        test($E, 5);
        break;
    case 0x6c:
        test($H, 5);
        break;
    case 0x6d:
        test($L, 5);
        break;
    case 0x6e:
        test(read($HL), 5);
        break;
    case 0x6f:
        test($A, 5);
        break;
    case 0x70:
        test($B, 6);
        break;
    case 0x71:
        test($C, 6);
        break;
    case 0x72:
        test($D, 6);
        break;
    case 0x73:
        test($E, 6);
        break;
    case 0x74:
        test($H, 6);
        break;
    case 0x75:
        test($L, 6);
        break;
    case 0x76:
        test(read($HL), 6);
        break;
    case 0x77:
        test($A, 6);
        break;
    case 0x78:
        test($B, 7);
        break;
    case 0x79:
        test($C, 7);
        break;
    case 0x7a:
        test($D, 7);
        break;
    case 0x7b:
        test($E, 7);
        break;
    case 0x7c:
        test($H, 7);
        break;
    case 0x7d:
        test($L, 7);
        break;
    case 0x7e:
        test(read($HL), 7);
        break;
    case 0x7f:
        test($A, 7);
        break;
    case 0x80:
        $B = res($B, 0);
        break;
    case 0x81:
        $C = res($C, 0);
        break;
    case 0x82:
        $D = res($D, 0);
        break;
    case 0x83:
        $E = res($E, 0);
        break;
    case 0x84:
        $H = res($H, 0);
        break;
    case 0x85:
        $L = res($L, 0);
        break;
    case 0x86:
        write($HL, res(read($HL), 0));
        break;
    case 0x87:
        $A = res($A, 0);
        break;
    case 0x88:
        $B = res($B, 1);
        break;
    case 0x89:
        $C = res($C, 1);
        break;
    case 0x8a:
        $D = res($D, 1);
        break;
    case 0x8b:
        $E = res($E, 1);
        break;
    case 0x8c:
        $H = res($H, 1);
        break;
    case 0x8d:
        $L = res($L, 1);
        break;
    case 0x8e:
        write($HL, res(read($HL), 1));
        break;
    case 0x8f:
        $A = res($A, 1);
        break;
    case 0x90:
        $B = res($B, 2);
        break;
    case 0x91:
        $C = res($C, 2);
        break;
    case 0x92:
        $D = res($D, 2);
        break;
    case 0x93:
        $E = res($E, 2);
        break;
    case 0x94:
        $H = res($H, 2);
        break;
    case 0x95:
        $L = res($L, 2);
        break;
    case 0x96:
        write($HL, res(read($HL), 2));
        break;
    case 0x97:
        $A = res($A, 2);
        break;
    case 0x98:
        $B = res($B, 3);
        break;
    case 0x99:
        $C = res($C, 3);
        break;
    case 0x9a:
        $D = res($D, 3);
        break;
    case 0x9b:
        $E = res($E, 3);
        break;
    case 0x9c:
        $H = res($H, 3);
        break;
    case 0x9d:
        $L = res($L, 3);
        break;
    case 0x9e:
        write($HL, res(read($HL), 3));
        break;
    case 0x9f:
        $A = res($A, 3);
        break;
    case 0xa0:
        $B = res($B, 4);
        break;
    case 0xa1:
        $C = res($C, 4);
        break;
    case 0xa2:
        $D = res($D, 4);
        break;
    case 0xa3:
        $E = res($E, 4);
        break;
    case 0xa4:
        $H = res($H, 4);
        break;
    case 0xa5:
        $L = res($L, 4);
        break;
    case 0xa6:
        write($HL, res(read($HL), 4));
        break;
    case 0xa7:
        $A = res($A, 4);
        break;
    case 0xa8:
        $B = res($B, 5);
        break;
    case 0xa9:
        $C = res($C, 5);
        break;
    case 0xaa:
        $D = res($D, 5);
        break;
    case 0xab:
        $E = res($E, 5);
        break;
    case 0xac:
        $H = res($H, 5);
        break;
    case 0xad:
        $L = res($L, 5);
        break;
    case 0xae:
        write($HL, res(read($HL), 5));
        break;
    case 0xaf:
        $A = res($A, 5);
        break;
    case 0xb0:
        $B = res($B, 6);
        break;
    case 0xb1:
        $C = res($C, 6);
        break;
    case 0xb2:
        $D = res($D, 6);
        break;
    case 0xb3:
        $E = res($E, 6);
        break;
    case 0xb4:
        $H = res($H, 6);
        break;
    case 0xb5:
        $L = res($L, 6);
        break;
    case 0xb6:
        write($HL, res(read($HL), 6));
        break;
    case 0xb7:
        $A = res($A, 6);
        break;
    case 0xb8:
        $B = res($B, 7);
        break;
    case 0xb9:
        $C = res($C, 7);
        break;
    case 0xba:
        $D = res($D, 7);
        break;
    case 0xbb:
        $E = res($E, 7);
        break;
    case 0xbc:
        $H = res($H, 7);
        break;
    case 0xbd:
        $L = res($L, 7);
        break;
    case 0xbe:
        write($HL, res(read($HL), 7));
        break;
    case 0xbf:
        $A = res($A, 7);
        break;
    case 0xc0:
        $B = set($B, 0);
        break;
    case 0xc1:
        $C = set($C, 0);
        break;
    case 0xc2:
        $D = set($D, 0);
        break;
    case 0xc3:
        $E = set($E, 0);
        break;
    case 0xc4:
        $H = set($H, 0);
        break;
    case 0xc5:
        $L = set($L, 0);
        break;
    case 0xc6:
        write($HL, set(read($HL), 0));
        break;
    case 0xc7:
        $A = set($A, 0);
        break;
    case 0xc8:
        $B = set($B, 1);
        break;
    case 0xc9:
        $C = set($C, 1);
        break;
    case 0xca:
        $D = set($D, 1);
        break;
    case 0xcb:
        $E = set($E, 1);
        break;
    case 0xcc:
        $H = set($H, 1);
        break;
    case 0xcd:
        $L = set($L, 1);
        break;
    case 0xce:
        write($HL, set(read($HL), 1));
        break;
    case 0xcf:
        $A = set($A, 1);
        break;
    case 0xd0:
        $B = set($B, 2);
        break;
    case 0xd1:
        $C = set($C, 2);
        break;
    case 0xd2:
        $D = set($D, 2);
        break;
    case 0xd3:
        $E = set($E, 2);
        break;
    case 0xd4:
        $H = set($H, 2);
        break;
    case 0xd5:
        $L = set($L, 2);
        break;
    case 0xd6:
        write($HL, set(read($HL), 2));
        break;
    case 0xd7:
        $A = set($A, 2);
        break;
    case 0xd8:
        $B = set($B, 3);
        break;
    case 0xd9:
        $C = set($C, 3);
        break;
    case 0xda:
        $D = set($D, 3);
        break;
    case 0xdb:
        $E = set($E, 3);
        break;
    case 0xdc:
        $H = set($H, 3);
        break;
    case 0xdd:
        $L = set($L, 3);
        break;
    case 0xde:
        write($HL, set(read($HL), 3));
        break;
    case 0xdf:
        $A = set($A, 3);
        break;
    case 0xe0:
        $B = set($B, 4);
        break;
    case 0xe1:
        $C = set($C, 4);
        break;
    case 0xe2:
        $D = set($D, 4);
        break;
    case 0xe3:
        $E = set($E, 4);
        break;
    case 0xe4:
        $H = set($H, 4);
        break;
    case 0xe5:
        $L = set($L, 4);
        break;
    case 0xe6:
        write($HL, set(read($HL), 4));
        break;
    case 0xe7:
        $A = set($A, 4);
        break;
    case 0xe8:
        $B = set($B, 5);
        break;
    case 0xe9:
        $C = set($C, 5);
        break;
    case 0xea:
        $D = set($D, 5);
        break;
    case 0xeb:
        $E = set($E, 5);
        break;
    case 0xec:
        $H = set($H, 5);
        break;
    case 0xed:
        $L = set($L, 5);
        break;
    case 0xee:
        write($HL, set(read($HL), 5));
        break;
    case 0xef:
        $A = set($A, 5);
        break;
    case 0xf0:
        $B = set($B, 6);
        break;
    case 0xf1:
        $C = set($C, 6);
        break;
    case 0xf2:
        $D = set($D, 6);
        break;
    case 0xf3:
        $E = set($E, 6);
        break;
    case 0xf4:
        $H = set($H, 6);
        break;
    case 0xf5:
        $L = set($L, 6);
        break;
    case 0xf6:
        write($HL, set(read($HL), 6));
        break;
    case 0xf7:
        $A = set($A, 6);
        break;
    case 0xf8:
        $B = set($B, 7);
        break;
    case 0xf9:
        $C = set($C, 7);
        break;
    case 0xfa:
        $D = set($D, 7);
        break;
    case 0xfb:
        $E = set($E, 7);
        break;
    case 0xfc:
        $H = set($H, 7);
        break;
    case 0xfd:
        $L = set($L, 7);
        break;
    case 0xfe:
        write($HL, set(read($HL), 7));
        break;
    case 0xff:
        $A = set($A, 7);
        break;
    }
}

uint8_t executeOp(uint8_t op) {
    bool imm_ime = false;
    uint8_t c = cycles[op];
    

    switch (op) {
    case 0x0:
        break;
    case 0x01:
        $BC = read16(++$PC); $PC++;
        break;
    case 0x02:
        write($BC, $A);
        break;
    case 0x03:
        $BC = inc($BC);
        break;
    case 0x04:
        $B = inc($B);
        break;
    case 0x05:
        $B = dec($B);
        break;
    case 0x06:
        $B = read(++$PC);
        break;
    case 0x07:
        $A = rl($A, true, true);
        break;
    case 0x08:
    {
        uint16_t nn = read16(++$PC); $PC++;
        write(nn, (registers[5].bytes.lo));
        write(nn + 1, (registers[5].bytes.hi));
    }
    break;
    case 0x09:
        $HL = add($HL, $BC);
        break;
    case 0x0A:
        $A = read($BC);
        break;
    case 0x0B:
        $BC = dec($BC);
        break;
    case 0x0C:
        $C = inc($C);
        break;
    case 0x0D:
        $C = dec($C);
        break;
    case 0x0E:
        $C = read(++$PC);
        break;
    case 0x0F:
        $A = rr($A, true, true);
        break;
        // case 0x10:
            // TODO: STOP
    case 0x11:
        $DE = read16(++$PC); $PC++;
        break;
    case 0x12:
        write($DE, $A);
        break;
    case 0x13:
        $DE = inc($DE);
        break;
    case 0x14:
        $D = inc($D);
        break;
    case 0x15:
        $D = dec($D);
        break;
    case 0x16:
        $D = read(++$PC);
        break;
    case 0x17:
        $A = rl($A, false, true);
        break;
    case 0x18:
        $PC += (int8_t)read(++$PC);
        break;
    case 0x19:
        $HL = add($HL, $DE);
        break;
    case 0x1A:
        $A = read($DE);
        break;
    case 0x1B:
        $DE = dec($DE);
        break;
    case 0x1C:
        $E = inc($E);
        break;
    case 0x1D:
        $E = dec($E);
        break;
    case 0x1E:
        $E = read(++$PC);
        break;
    case 0x1F:
        $A = rr($A, false, true);
        break;
    case 0x20:
    {
        int8_t n = read(++$PC);

        if (!$Z) {
            $PC += n;
            c++;
        }
    }
    break;
    case 0x21:
        $HL = read16(++$PC); $PC++;
        break;
    case 0x22:
        write($HL, $A);
        ++$HL;
        break;
    case 0x23:
        $HL = inc($HL);
        break;
    case 0x24:
        $H = inc($H);
        break;
    case 0x25:
        $H = dec($H);
        break;
    case 0x26:
        $H = read(++$PC);
        break;
    case 0x27:
    {
        uint8_t correction = 0;

        uint8_t n = $N;

        if ($HF) {
            correction |= 0x06;
        }

        if ($CR) {
            correction |= 0x60;
        }

        if ($N) {
            $A = sub($A, correction);
        }
        else {
            if (($A & 0x0f) > 0x09) {
                correction |= 0x06;
            }

            if ($A > 0x99) {
                correction |= 0x60;
            }

            $A = add($A, correction);
        }

         $N = n; $CR = (correction & 0x60) != 0; $Z = !$A; $HF = 0;
    }
    break;
    case 0x28:
    {
        int8_t n = read(++$PC);

        if ($Z) {
            $PC += n;
            c++;
        }
    }
    break;
    case 0x29:
        $HL = add($HL, $HL);
        break;
    case 0x2A:
        $A = read($HL);
        ++$HL;
        break;
    case 0x2B:
        $HL = dec($HL);
        break;
    case 0x2C:
        $L = inc($L);
        break;
    case 0x2D:
        $L = dec($L);
        break;
    case 0x2E:
        $L = read(++$PC);
        break;
    case 0x2F:
        $A = ~$A; $N = 1; $HF = 1;
        break;
    case 0x30:
    {
        int8_t n = read(++$PC);

        if (!$CR) {
            $PC += n;
            c++;
        }
    }
    break;
    case 0x31:
        $SP = read16(++$PC); $PC++;
        break;
    case 0x32:
        write($HL, $A);
        --$HL;
        break;
    case 0x33:
        $SP = inc($SP);
        break;
    case 0x34:
        write($HL, inc(read($HL)));
        break;
    case 0x35:
        write($HL, dec(read($HL)));
        break;
    case 0x36:
        write($HL, read(++$PC));
        break;
    case 0x37:
        $N = 0; $HF = 0; $CR = 1;
        break;
    case 0x38:
    {
        int8_t n = read(++$PC);

        if ($CR) {
            $PC += n;
            c++;
        }
    }
    break;
    case 0x39:
        $HL = add($HL, $SP);
        break;
    case 0x3A:
        $A = read($HL);
        --$HL;
        break;
    case 0x3B:
        $SP = dec($SP);
        break;
    case 0x3C:
        $A = inc($A);
        break;
    case 0x3D:
        $A = dec($A);
        break;
    case 0x3E:
        $A = read(++$PC);
        break;
    case 0x3F:
        $N = 0; $HF = 0; $CR = ~$CR;
        break;
    case 0x40:
        $B = $B;
        break;
    case 0x41:
        $B = $C;
        break;
    case 0x42:
        $B = $D;
        break;
    case 0x43:
        $B = $E;
        break;
    case 0x44:
        $B = $H;
        break;
    case 0x45:
        $B = $L;
        break;
    case 0x46:
        $B = read($HL);
        break;
    case 0x47:
        $B = $A;
        break;
    case 0x48:
        $C = $B;
        break;
    case 0x49:
        $C = $C;
        break;
    case 0x4A:
        $C = $D;
        break;
    case 0x4B:
        $C = $E;
        break;
    case 0x4C:
        $C = $H;
        break;
    case 0x4D:
        $C = $L;
        break;
    case 0x4E:
        $C = read($HL);
        break;
    case 0x4F:
        $C = $A;
        break;
    case 0x50:
        $D = $B;
        break;
    case 0x51:
        $D = $C;
        break;
    case 0x52:
        $D = $D;
        break;
    case 0x53:
        $D = $E;
        break;
    case 0x54:
        $D = $H;
        break;
    case 0x55:
        $D = $L;
        break;
    case 0x56:
        $D = read($HL);
        break;
    case 0x57:
        $D = $A;
        break;
    case 0x58:
        $E = $B;
        break;
    case 0x59:
        $E = $C;
        break;
    case 0x5A:
        $E = $D;
        break;
    case 0x5B:
        $E = $E;
        break;
    case 0x5C:
        $E = $H;
        break;
    case 0x5D:
        $E = $L;
        break;
    case 0x5E:
        $E = read($HL);
        break;
    case 0x5F:
        $E = $A;
        break;
    case 0x60:
        $H = $B;
        break;
    case 0x61:
        $H = $C;
        break;
    case 0x62:
        $H = $D;
        break;
    case 0x63:
        $H = $E;
        break;
    case 0x64:
        $H = $H;
        break;
    case 0x65:
        $H = $L;
        break;
    case 0x66:
        $H = read($HL);
        break;
    case 0x67:
        $H = $A;
        break;
    case 0x68:
        $L = $B;
        break;
    case 0x69:
        $L = $C;
        break;
    case 0x6A:
        $L = $D;
        break;
    case 0x6B:
        $L = $E;
        break;
    case 0x6C:
        $L = $H;
        break;
    case 0x6D:
        $L = $L;
        break;
    case 0x6E:
        $L = read($HL);
        break;
    case 0x6F:
        $L = $A;
        break;
    case 0x70:
        write($HL, $B);
        break;
    case 0x71:
        write($HL, $C);
        break;
    case 0x72:
        write($HL, $D);
        break;
    case 0x73:
        write($HL, $E);
        break;
    case 0x74:
        write($HL, $H);
        break;
    case 0x75:
        write($HL, $L);
        break;
    case 0x76:
        halted = true;
        break;
    case 0x77:
        write($HL, $A);
        break;
    case 0x78:
        $A = $B;
        break;
    case 0x79:
        $A = $C;
        break;
    case 0x7A:
        $A = $D;
        break;
    case 0x7B:
        $A = $E;
        break;
    case 0x7C:
        $A = $H;
        break;
    case 0x7D:
        $A = $L;
        break;
    case 0x7E:
        $A = read($HL);
        break;
    case 0x7F:
        $A = $A;
        break;
    case 0x80:
        $A = add($A, $B);
        break;
    case 0x81:
        $A = add($A, $C);
        break;
    case 0x82:
        $A = add($A, $D);
        break;
    case 0x83:
        $A = add($A, $E);
        break;
    case 0x84:
        $A = add($A, $H);
        break;
    case 0x85:
        $A = add($A, $L);
        break;
    case 0x86:
        $A = add($A, read($HL));
        break;
    case 0x87:
        $A = add($A, $A);
        break;
    case 0x88:
        $A = add($A, $B, true);
        break;
    case 0x89:
        $A = add($A, $C, true);
        break;
    case 0x8A:
        $A = add($A, $D, true);
        break;
    case 0x8B:
        $A = add($A, $E, true);
        break;
    case 0x8C:
        $A = add($A, $H, true);
        break;
    case 0x8D:
        $A = add($A, $L, true);
        break;
    case 0x8E:
        $A = add($A, read($HL), true);
        break;
    case 0x8F:
        $A = add($A, $A, true);
        break;
    case 0x90:
        $A = sub($A, $B);
        break;
    case 0x91:
        $A = sub($A, $C);
        break;
    case 0x92:
        $A = sub($A, $D);
        break;
    case 0x93:
        $A = sub($A, $E);
        break;
    case 0x94:
        $A = sub($A, $H);
        break;
    case 0x95:
        $A = sub($A, $L);
        break;
    case 0x96:
        $A = sub($A, read($HL));
        break;
    case 0x97:
        $A = sub($A, $A);
        break;
    case 0x98:
        $A = sub($A, $B, true);
        break;
    case 0x99:
        $A = sub($A, $C, true);
        break;
    case 0x9A:
        $A = sub($A, $D, true);
        break;
    case 0x9B:
        $A = sub($A, $E, true);
        break;
    case 0x9C:
        $A = sub($A, $H, true);
        break;
    case 0x9D:
        $A = sub($A, $L, true);
        break;
    case 0x9E:
        $A = sub($A, read($HL), true);
        break;
    case 0x9F:
        $A = sub($A, $A, true);
        break;
    case 0xA0:
        $A = and8($A, $B);
        break;
    case 0xA1:
        $A = and8($A, $C);
        break;
    case 0xA2:
        $A = and8($A, $D);
        break;
    case 0xA3:
        $A = and8($A, $E);
        break;
    case 0xA4:
        $A = and8($A, $H);
        break;
    case 0xA5:
        $A = and8($A, $L);
        break;
    case 0xA6:
        $A = and8($A, read($HL));
        break;
    case 0xA7:
        $A = and8($A, $A);
        break;
    case 0xA8:
        $A = xor8($A, $B);
        break;
    case 0xA9:
        $A = xor8($A, $C);
        break;
    case 0xAA:
        $A = xor8($A, $D);
        break;
    case 0xAB:
        $A = xor8($A, $E);
        break;
    case 0xAC:
        $A = xor8($A, $H);
        break;
    case 0xAD:
        $A = xor8($A, $L);
        break;
    case 0xAE:
        $A = xor8($A, read($HL));
        break;
    case 0xAF:
        $A = xor8($A, $A);
        break;
    case 0xB0:
        $A = or8($A, $B);
        break;
    case 0xB1:
        $A = or8($A, $C);
        break;
    case 0xB2:
        $A = or8($A, $D);
        break;
    case 0xB3:
        $A = or8($A, $E);
        break;
    case 0xB4:
        $A = or8($A, $H);
        break;
    case 0xB5:
        $A = or8($A, $L);
        break;
    case 0xB6:
        $A = or8($A, read($HL));
        break;
    case 0xB7:
        $A = or8($A, $A);
        break;
    case 0xB8:
        sub($A, $B);
        break;
    case 0xB9:
        sub($A, $C);
        break;
    case 0xBA:
        sub($A, $D);
        break;
    case 0xBB:
        sub($A, $E);
        break;
    case 0xBC:
        sub($A, $H);
        break;
    case 0xBD:
        sub($A, $L);
        break;
    case 0xBE:
        sub($A, read($HL));
        break;
    case 0xBF:
        sub($A, $A);
        break;
    case 0xC0:
        if (!$Z) {
            $PC = read16($SP++); $SP++; $PC--;
            c += 3;
        }
        break;
    case 0xC1:
        $BC = read16($SP++); $SP++;
        break;
    case 0xC2:
    {
        uint16_t nn = read16(++$PC); $PC++;

        if (!$Z) {
            $PC = nn; $PC--;
            c++;
        }
    }
    break;
    case 0xC3:
        $PC = read16(++$PC); $PC--;
        break;
    case 0xC4:
    {
        uint16_t nn = read16(++$PC); $PC+=2;

        if (!$Z) {
            write(--$SP, registers[4].bytes.hi);
            write(--$SP, registers[4].bytes.lo);
            $PC = nn;
            c += 3;
        }

        $PC--;
    }
    break;
    case 0xC5:
        write(--$SP, $B);
        write(--$SP, $C);
        break;
    case 0xC6:
        $A = add($A, read(++$PC));
        break;
    case 0xC7:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x00;  $PC--;
        break;
    case 0xC8:
        if ($Z) {
            $PC = read16($SP++); $SP++; $PC--;
            c += 3;
        }
        break;
    case 0xC9:
        $PC = read16($SP++); $SP++; $PC--;
        break;
    case 0xCA:
    {
        uint16_t nn = read16(++$PC); $PC++;

        if ($Z) {
            $PC = nn; $PC--;
            c++;
        }
    }
    break;
    case 0xCB:
    {
        uint8_t code = read(++$PC);
        executePrefixOp(code);
        c = cb_cycles[code];
    }
        break;
    case 0xCC:
    {
        uint16_t nn = read16(++$PC); $PC+=2;

        if ($Z) {
            write(--$SP, registers[4].bytes.hi);
            write(--$SP, registers[4].bytes.lo);
            $PC = nn;
            c += 3;
        }

        $PC--;
    }
    break;
    case 0xCD:
    {
        uint16_t nn = read16(++$PC); $PC += 2;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = nn; $PC--;
    }
        break;
    case 0xCE:
        $A = add($A, read(++$PC), true);
        break;
    case 0xCF:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x07; //0x08
        break;
    case 0xD0:
        if (!$CR) {
            $PC = read16($SP++); $SP++; $PC--;
            c += 3;
        }
        break;
    case 0xD1:
        $DE = read16($SP++); $SP++;
        break;
    case 0xD2:
    {
        uint16_t nn = read16(++$PC); $PC++;

        if (!$CR) {
            $PC = nn; $PC--;
            c++;
        }
    }
    break;
    case 0xD4:
    {
        uint16_t nn = read16(++$PC); $PC+=2;

        if (!$CR) {
            write(--$SP, registers[4].bytes.hi);
            write(--$SP, registers[4].bytes.lo);
            $PC = nn;
            c += 3;
        }
        
        $PC--;
    }
    break;
    case 0xD5:
        write(--$SP, $D);
        write(--$SP, $E);
        break;
    case 0xD6:
        $A = sub($A, read(++$PC));
        break;
    case 0xD7:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x0F; //0x10
        break;
    case 0xD8:
        if ($CR) {
            $PC = read16($SP++); $SP++; $PC--;
            c += 3;
        }
        break;
    case 0xD9:
        $PC = read16($SP++); $SP++; $PC--;
        IME = true;
        break;
    case 0xDA:
    {
        uint16_t nn = read16(++$PC); $PC++;

        if ($CR) {
            $PC = nn; $PC--;
            c++;
        }
    }
    break;
    case 0xDC:
    {
        uint16_t nn = read16(++$PC); $PC+=2;

        if ($CR) {
            write(--$SP, registers[4].bytes.hi);
            write(--$SP, registers[4].bytes.lo);
            $PC = nn;
            c += 3;
        }

        $PC--;
    }
    break;
    case 0xDE:
        $A = sub($A, read(++$PC), true);
        break;
    case 0xDF:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x17; //0x18
        break;
    case 0xE0:
        write(0xFF00 + read(++$PC), $A);
        break;
    case 0xE1:
        $HL = read16($SP++); $SP++;
        break;
    case 0xE2:
        write(0xFF00 + $C, $A);
        break;
    case 0xE5:
        write(--$SP, $H);
        write(--$SP, $L);
        break;
    case 0xE6:
        $A = and8($A, read(++$PC));
        break;
    case 0xE7:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x1F; //0x20
        break;
    case 0xE8:
        $SP = add($SP, read(++$PC));
        break;
    case 0xE9:
        $PC = $HL; $PC--;
        break;
    case 0xEA:
        write(read16(++$PC), $A); $PC++;
        break;
    case 0xEE:
        $A = xor8($A, read(++$PC));
        break;
    case 0xEF:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x27; //0x28
        break;
    case 0xF0:
        $A = read(0xFF00 + read(++$PC));
        break;
    case 0xF1:
        $AF = read16($SP++) & 0xfff0; $SP++;
        break;
    case 0xF2:
        $A = read($C + 0xFF00);
        break;
    case 0xF3:
        ime_sched = IME = false;
        break;
    case 0xF5:
        write(--$SP, $A);
        write(--$SP, $F);
        break;
    case 0xF6:
        $A = or8($A, read(++$PC));
        break;
    case 0xF7:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x2F; //0x30
        break;
    case 0xF8:
        $HL = add($SP, read(++$PC));
        break;
    case 0xF9:
        $SP = $HL;
        break;
    case 0xFA:
        $A = read(read16(++$PC)); $PC++;
        break;
    case 0xFB:
        imm_ime = ime_sched = true;
        break;
    case 0xFE:
        sub($A, read(++$PC));
        break;
    case 0xFF:
        $PC++;
        write(--$SP, registers[4].bytes.hi);
        write(--$SP, registers[4].bytes.lo);
        $PC = 0x37; // 0x38
        break;
    }

    if (!imm_ime && ime_sched) {
        IME = true;
        ime_sched = false;
    }

    return c;
}