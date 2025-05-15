#include <iostream>
#include <format>
#include <SDL.h>
#include <bitset>
#include <memory>
#include "tinyfiledialogs.h"

#include "gba.hpp"
#include "opcodes.h"
#include "ppu.hpp"

/**
 * @brief Checks for and handles pending interrupts.
 *
 * This function reads the interrupt enable (IE) register (0xFFFF) and the
 * interrupt flag (IF) register (0xFF0F). If any enabled interrupts are pending
 * (i.e., the corresponding bits are set in both IE and IF), and the master
 * interrupt enable flag (IME) is set, the function will:
 * 1. Clear the `halted` flag if the CPU was halted.
 * 2. Push the current program counter (PC) onto the stack.
 * 3. Jump to the appropriate interrupt service routine (ISR) address.
 * 4. Clear the corresponding bit in the IF register.
 * 5. Clear the IME flag.
 * The `ime_sched` flag is also cleared.
 */
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
            else if (int_enabled & 16) {
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

/**
 * @brief Main entry point for the Game Boy emulator.
 *
 * Initializes registers, timer, memory (based on ROM header), PPU, and SDL.
 * Loads the boot ROM and the game ROM.
 * Enters the main emulation loop, which fetches and executes opcodes,
 * steps the PPU and timer, and checks for interrupts.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 on successful execution, -1 or 1 on error.
 */
int main(int argc, char* argv[])
{   
    registers = std::array< Register, 6 >();
    timer = std::make_unique<Timer>();

    // Use file dialog to select ROM
    const char* filters[] = { "*.gb", "*.gbc" };
    const char* romPath = tinyfd_openFileDialog(
        "Select Game Boy ROM",
        ".\\",
        2,
        filters,
        "Game Boy ROMs",
        0);
    
    if (!romPath) {
        tinyfd_messageBox(
            "Error",
            "No ROM selected",
            "ok",
            "error",
            1);
        return -1;
    }
    
    auto f = std::ifstream(romPath, std::ios::binary);

    if (!f.is_open()) {
        tinyfd_messageBox(
            "Error",
            "Could not open ROM",
            "ok",
            "error",
            1);
        return 1;
    }

    f.unsetf(std::ios::skipws);

    f.seekg(0x147, std::ios::beg);

    uint8_t chip = f.get();
    size_t rom_size_factor = 1 << (f.get() + 1);
    uint8_t nRAM = f.get();

    f.seekg(0);   

    switch (chip) {
    case 0:
        memory = std::make_unique<NoMBC>();
        break;
    case 8:
    case 9:
        memory = std::make_unique<NoMBC>(true);
        break;
    case 1:
    case 2:
        memory = std::make_unique<MBC1>(nRAM, rom_size_factor, false);
        break;
    case 3:
        memory = std::make_unique<MBC1>(nRAM, rom_size_factor, true);
        break;
    case 0x0F:
        memory = std::make_unique<MBC3>(nRAM, rom_size_factor, true, false);
        break;
    case 0x10:
		memory = std::make_unique<MBC3>(nRAM, rom_size_factor, true, true);
        break;
    case 0x11:
    case 0x12:
        memory = std::make_unique<MBC3>(nRAM, rom_size_factor, false, false);
        break;
    case 0x13:
        memory = std::make_unique<MBC3>(nRAM, rom_size_factor, false, true);
		break;
    case 0x19:
    case 0x1A:
    case 0x1C:
    case 0x1D:
        memory = std::make_unique<MBC5>(nRAM, rom_size_factor, false);
        break;
    case 0x1B:
    case 0x1E:
        memory = std::make_unique<MBC5>(nRAM, rom_size_factor, true);
		break;
    default:
        tinyfd_messageBox(
            "Error",
            std::format("Unsupported memory chip: 0x{:x}", unsigned(chip)).c_str(),
            "ok",
            "error",
            1);
        return -1;
    }

    memory->loadROM(f);

    f.close();

    memory->loadBootROM("E:/code/gba/ROM");

    if (!memory->isBRActive()) {
        $PC = 0x100; $SP = 0xFFFE;
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        tinyfd_messageBox(
            "Error",
            "Could not INIT SDL",
            "ok",
            "error",
            1);
        return 1;
    }

    PPU = std::make_unique<PPUObj>();

    uint8_t cycles = 0;
    SDL_Event event;

    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            }
        }

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
        }
        else {

        }
    }

    return 0;
}

