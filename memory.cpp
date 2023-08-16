#include "memory.hpp"
#include "iostream"
#include "gba.hpp"
#include <SDL.h>
#include <bitset>

uint8_t getInput(uint8_t val) {
	const uint8_t* keys = SDL_GetKeyboardState(NULL);
	uint8_t joypad = 0x00;

	if ((val & 0x30) == 0x10) {
		joypad |= !keys[SDL_SCANCODE_A];
		joypad |= !keys[SDL_SCANCODE_S] << 1;
		joypad |= !keys[SDL_SCANCODE_X] << 2;
		joypad |= !keys[SDL_SCANCODE_Z] << 3;
	}
	else if ((val & 0x30) == 0x20) {
		joypad |= !keys[SDL_SCANCODE_RIGHT];
		joypad |= !keys[SDL_SCANCODE_LEFT] << 1;
		joypad |= !keys[SDL_SCANCODE_UP] << 2;
		joypad |= !keys[SDL_SCANCODE_DOWN] << 3;
	}

	(val &= 0xf0) |= 0xc0;

	std::cout << std::bitset<8>(joypad) << std::endl;

	return (val | joypad);
}

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
		for (int i = 0; i < 40; i++) {
			memory->set(0xfe00 + (i * 4), memory->get((prev * 0x100) + (i * 4)));
			memory->set(0xfe00 + (i * 4) + 1, memory->get((prev * 0x100) + (i * 4) + 1));
			memory->set(0xfe00 + (i * 4) + 2, memory->get((prev * 0x100) + (i * 4) + 2));
			memory->set(0xfe00 + (i * 4) + 3, memory->get((prev * 0x100) + (i * 4) + 3));
		}
		break;
	case 0x50:
		m->disableBR();
		break;
	}
}

void loadR(std::ifstream& f, std::vector<uint8_t>& rom) {
	rom.insert(rom.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
}

bool loadBR(std::string& file, std::vector<uint8_t>& rom) {
	auto f = std::ifstream(file, std::ios::binary);
	f.unsetf(std::ios::skipws);

	if (!f.is_open()) {
		std::cout << "failed to open rom\n";
		return false;
	}

	rom.insert(rom.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());

	f.close();

	return true;
}
