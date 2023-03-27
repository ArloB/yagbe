#include <iostream>
#include <fstream>
#include <format>
#include <SDL.h>
#include <ranges>
#include <bitset>

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

    auto f = std::ifstream("E:/code/gba/roms/instr_timing.gb", std::ios::binary);

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
    case 3:
        memory = std::unique_ptr<Mem>(new MBC1(nRAM, std::pow(2, nROM + 1)));
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

    if (SDL_CreateWindowAndRenderer(160, 144, 0, &win, &renderer)) {
        std::cout << "Window could not be created\n";
    }

    SDL_RenderSetLogicalSize(renderer, 160, 144);
    SDL_SetWindowResizable(win, SDL_TRUE);

    renderTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    
    (background = std::array<uint8_t, 262144>()).fill({});
    (window = std::array<uint8_t, 262144>()).fill({});
    (sprites = std::array<uint8_t, 262144>()).fill({});

    memory->set(0xff42, 0);
    memory->set(0xff43, 0);

    uint8_t cycles = 0;

    while (1) {

        if (!halted) {
             cycles = executeOp(memory->get($PC));

             $PC++;
        }
        else {
            cycles = 1;
        }

        checkInterrupts();

        timer->tick(cycles);
    }

    return 0;
}

