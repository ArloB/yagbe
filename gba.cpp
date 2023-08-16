#include <iostream>
#include <format>
#include <SDL.h>
#include <bitset>
#include <memory>

#include "gba.hpp"
#include "opcodes.h"
#include "ppu.hpp"

inline void checkInterrupts() {
    uint8_t flags = memory->get(0xff0f);
    uint8_t int_enabled = memory->get(0xffff) & flags;

    if (int_enabled) {
        if (halted) {
            halted = false;
        }

        if (IME) {
            memory->set(--$SP, registers[4].bytes.hi);
            memory->set(--$SP, registers[4].bytes.lo);

            if (int_enabled & 1) {
                $PC = 0x40;
                memory->set(0xff0f, flags & (~1));
            }
            else if (int_enabled & 2) {
                $PC = 0x48;
                memory->set(0xff0f, flags & (~2));
            }
            else if (int_enabled & 4) {
                $PC = 0x50;
                memory->set(0xff0f, flags & (~4));
            }
            else if (int_enabled & 8) {
                $PC = 0x58;
                memory->set(0xff0f, flags & (~8));
            }
            else if (int_enabled & 8) {
                $PC = 0x60;
                memory->set(0xff0f, flags & (~16));
            }
            else {
                std::cout << "Unknown interrupt flag set";
                $PC = 0;
            }

            IME = false;
            ime_sched = false;
        }
    }
}

int main(int argc, char* argv[])
{   
    registers = std::array< Register, 6 >();
    timer = std::make_unique<Timer>();

    auto f = std::ifstream("E:/code/gba/roms/Tetris.gb", std::ios::binary);

    if (!f.is_open()) {
        std::cout << "Could not open rom\n";
        return -1;
    }

    f.unsetf(std::ios::skipws);

    f.seekg(0x147, std::ios::beg);

    uint8_t chip = f.get();
    uint8_t nROM = f.get();
    uint8_t nRAM = f.get();

    f.seekg(0);

    switch (chip) {
    case 0:
        memory = std::unique_ptr<Mem>(new NoMBC);
        break;
    case 8:
    case 9:
        memory = std::unique_ptr<Mem>(new NoMBC(true));
        break;
    case 1:
    case 2:
        memory = std::unique_ptr<Mem>(new MBC1(nRAM, std::pow(2, nROM + 1), false));
        break;
    case 3:
        memory = std::unique_ptr<Mem>(new MBC1(nRAM, std::pow(2, nROM + 1), true));
        break;
    case 0x0F:
        memory = std::unique_ptr<Mem>(new MBC3(nRAM, std::pow(2, nROM + 1), true, false));
        break;
    case 0x10:
		memory = std::unique_ptr<Mem>(new MBC3(nRAM, std::pow(2, nROM + 1), true, true));
        break;
    case 0x11:
    case 0x12:
        memory = std::unique_ptr<Mem>(new MBC3(nRAM, std::pow(2, nROM + 1), false, false));
        break;
    case 0x13:
        memory = std::unique_ptr<Mem>(new MBC3(nRAM, std::pow(2, nROM + 1), false, true));
		break;
    case 0x19:
    case 0x1A:
    case 0x1C:
    case 0x1D:
        memory = std::unique_ptr<Mem>(new MBC5(nRAM, std::pow(2, nROM + 1), false));
        break;
    case 0x1B:
    case 0x1E:
        memory = std::unique_ptr<Mem>(new MBC5(nRAM, std::pow(2, nROM + 1), true));
		break;
    default:
        std::cout << "Unsupported memory chip: 0x" << std::hex << unsigned(chip) << "\n";
        return -1;
    }

    memory->loadROM(f);

    f.close();

    memory->loadBootROM("E:/code/gba/ROM");

    if (!memory->isBRActive()) {
        $PC = 0x100; $SP = 0xFFFE;
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cout << "Could not INIT SDL\n";
        return 1;
    }

    PPU = std::make_unique<PPUObj>();

    uint8_t cycles = 0;

    while (1) {
        if (!stopped) {
            if (!halted) {
                uint8_t op = memory->get($PC);
                cycles = executeOp(op);

                $PC++;
            }
            else {
                cycles = 1;
            }

            PPU->step(cycles);

            timer->tick(cycles);

            checkInterrupts();

            if (memory->get(0xff44) == 144) {
				
			}
        }
        else {

        }
    }

    return 0;
}

