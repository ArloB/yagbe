#include <iostream>
#include <format>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <memory>

#include "gba.hpp"
#include "opcodes.h"
#include "ppu.hpp"
#include "memory.hpp"

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
 */
inline void handleInterrupts(uint8_t flags, uint8_t int_flags) {
    IME = false;
    
    memory->set(--$SP, RegisterAccess::getHi(registers[4]));
    memory->set(--$SP, RegisterAccess::getLo(registers[4]));

    if (int_flags & 1) {
        $PC = 0x40;
        memory->set(0xff0f, flags & 0xFE);
    } else if (int_flags & 2) {
        $PC = 0x48;
        memory->set(0xff0f, flags & 0xFD);
    } else if (int_flags & 4) {
        $PC = 0x50;
        memory->set(0xff0f, flags & 0xFB);
    } else if (int_flags & 8) {
        $PC = 0x58;
        memory->set(0xff0f, flags & 0xF7);
    } else if (int_flags & 16) {
        $PC = 0x60;
        memory->set(0xff0f, flags & 0xEF);
    }
}

static void onFileChosen(void *userdata, const char * const *filelist, int filter) {
    if (!filelist) {
        return;
    } else if (!*filelist) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "No ROM selected", NULL);
        SDL_Quit();
    }

    romPath = filelist[0];
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

    if (! SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not INIT SDL", NULL);
        SDL_Quit();
    }

    apu = std::make_unique<APU>();

    SDL_DialogFileFilter filters[] = { "Game Boy ROMs", "gb;gbc" };
    SDL_ShowOpenFileDialog(onFileChosen, NULL, NULL, filters, SDL_arraysize(filters), ".\\", false);   
 
    while (romPath.empty()) {
        SDL_PumpEvents();
        SDL_Delay(20);
    }
    
    auto f = std::ifstream(romPath, std::ios::binary);

    if (!f.is_open()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not open ROM", NULL);
        SDL_Quit();
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
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", std::format("Unsupported memory chip: 0x{:x}", unsigned(chip)).c_str(), NULL);
        SDL_Quit();
    }

    memory->loadROM(f);

    f.close();

    memory->loadBootROM("E:/code/gba/ROM");

    if (!memory->isBRActive()) {
        $PC = 0x100; $SP = 0xFFFE;
    }

    PPU = std::make_unique<PPUObj>();

    uint8_t cycles = 0;
    SDL_Event event;

    while (1) {
        if (shouldExit) {
            goto exit;
        }
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                goto exit;
            }
        }
        
        if (halted || stopped) {
            cycles = 1;
        } else {
            uint8_t op = memory->get($PC);
            cycles = executeOp(op);
            
            if (halt_bug) {
                halt_bug = false;
            } else {
                $PC++;
            }
        }
        
        PPU->step(cycles);
        timer->tick(cycles);
        apu->step(cycles);
        
        uint8_t flags = memory->get(0xff0f);
        uint8_t ie = memory->get(0xffff);
        uint8_t int_enabled = ie & flags;
        
        if (halted && (int_enabled & 0x1F)) {
            halted = false;
            if (!IME) {
                halt_bug = true;
            }
        } else if (stopped && (int_enabled & 0x18)) {
            stopped = false;
        }
        
        if (int_enabled && IME) {
            handleInterrupts(flags, int_enabled);
            PPU->step(5);
            timer->tick(5);
            apu->step(5);
        }
    }

    exit: SDL_Quit();

    return 0;
}

