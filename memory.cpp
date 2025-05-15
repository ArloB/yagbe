#include "memory.hpp"
#include "iostream"
#include "gba.hpp"
#include <SDL.h>
#include <bitset>

/**
 * @brief Gets the current joypad input state based on the value written to the JOYP register.
 *
 * This function reads the keyboard state using SDL and maps the appropriate keys
 * to the Game Boy joypad buttons (A, B, Select, Start, Right, Left, Up, Down).
 * The specific buttons read depend on bits 4 and 5 of the input value `val`.
 *
 * @param val The value written to the JOYP register (0xFF00).
 * @return The updated value for the JOYP register, reflecting the current input state.
 */
uint8_t getInput(uint8_t val) {
    const uint8_t* keys = SDL_GetKeyboardState(NULL);
    uint8_t joypad = 0x0F; // Initialize with all buttons unpressed (1 = unpressed in GB hardware)
    
    // Action buttons (bit 5 low selects these buttons)
    if (!(val & 0x20)) {
        // Clear the bits that will be set based on button state
        joypad &= 0xF0;
        
        // Set bits based on button state (0 = pressed, 1 = unpressed)
        if (!keys[SDL_SCANCODE_A]) joypad |= 0x01; // A button
        if (!keys[SDL_SCANCODE_S]) joypad |= 0x02; // B button
        if (!keys[SDL_SCANCODE_X]) joypad |= 0x04; // Select button
        if (!keys[SDL_SCANCODE_Z]) joypad |= 0x08; // Start button
    }
    
    // Direction buttons (bit 4 low selects these buttons)
    if (!(val & 0x10)) {
        // Clear the bits that will be set based on button state
        joypad &= 0xF0;
        
        // Set bits based on button state (0 = pressed, 1 = unpressed)
        if (!keys[SDL_SCANCODE_RIGHT]) joypad |= 0x01; // Right
        if (!keys[SDL_SCANCODE_LEFT])  joypad |= 0x02; // Left
        if (!keys[SDL_SCANCODE_UP])    joypad |= 0x04; // Up
        if (!keys[SDL_SCANCODE_DOWN])  joypad |= 0x08; // Down
    }
    
    // Combine the input value (preserving bits 4-7) with the joypad state (bits 0-3)
    return (val & 0xF0) | (joypad & 0x0F);
}

/**
 * @brief Handles writes to I/O registers.
 *
 * This function is called when a value is written to an I/O register.
 * It updates the corresponding I/O register in the `io` vector and performs
 * specific actions based on the address being written to.
 *
 * @param addr The offset of the I/O register from 0xFF00.
 * @param val The value being written to the register.
 * @param m A pointer to the memory controller, used for certain I/O operations (e.g., serial transfer, DMA).
 * @param io A reference to the vector storing the state of I/O registers.
 */
void handleIO(uint8_t addr, uint8_t val, Mem* m, std::vector<uint8_t> &io) {
	uint8_t prev = io[addr];
	io[addr] = val;

	switch (addr) {
	case 0x00:
		io[addr] = getInput(val);
		break;
	case 0x02:
		if (val == 0x81) {
			std::cout << m->get(0xff01);
			io[addr] = 0;
		}
		break;
	case 0x04:
		io[addr] = 0;
		timer->resetdiv();
		break;
	case 0x07:
		io[addr] = (prev & ~7) | (val & 7);
		break;
	case 0x46:
		{
			uint16_t source_addr_base = static_cast<uint16_t>(val) << 8;
			for (int i = 0; i < 0xA0; i++) {
					m->set(0xFE00 + i, m->get(source_addr_base + i));
			}
		}
		break;
	case 0x50:
		m->disableBR();
		break;
	}
}

/**
 * @brief Loads ROM data from an input file stream into a vector.
 *
 * This function reads all bytes from the given input file stream `f`
 * and inserts them at the beginning of the `rom` vector.
 *
 * @param f An input file stream opened to the ROM file.
 * @param rom A reference to the vector where the ROM data will be stored.
 */
void loadR(std::ifstream& f, std::vector<uint8_t>& rom) {
	f.seekg(0, std::ios::beg);
	rom.assign(std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
}

/**
 * @brief Loads the boot ROM from a file.
 *
 * This function attempts to open and read the specified boot ROM file.
 * If successful, the contents are loaded into the `rom` vector.
 *
 * @param file A string containing the path to the boot ROM file.
 * @param rom A reference to the vector where the boot ROM data will be stored.
 * @return True if the boot ROM was loaded successfully, false otherwise.
 */
bool loadBR(std::string& file, std::vector<uint8_t>& rom) {
	auto f = std::ifstream(file, std::ios::binary);
	f.unsetf(std::ios::skipws);

	if (!f.is_open()) {
		std::cout << "failed to open boot rom: " << file << std::endl;
		return false;
	}

	rom.assign(std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());

	f.close();

	return true;
}
