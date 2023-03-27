#include "memory.hpp"
#include "iostream"
#include "gba.hpp"

void handleIO(uint8_t addr, uint8_t val, Mem* m, std::vector<uint8_t> &io) {
	uint8_t prev = io[addr];
	io[addr] = val;

	switch (addr) {
	case 0x02:
		if (val == 0x81) {
			std::cout << m->get(0xff01);
			io[addr] = 0;
		}
		break;
	case 0x04:
		io[addr] = 0;
		timer->reset();
		break;
	case 0x07:
		io[addr] = (prev & ~7) | (val & 7);
	/*case 0x46:
		uint8_t val = 
		m->*/
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
