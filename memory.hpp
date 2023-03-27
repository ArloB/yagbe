#ifndef MEMORY_H
#define MEMORY_H

#include <vector>
#include <cmath>
#include <string>
#include <fstream>
#include <iostream>

class Mem {
public:
	virtual inline uint8_t get(uint16_t addr) = 0;
	virtual inline void set(uint16_t addr, uint8_t val) = 0;
	virtual void loadROM(std::ifstream &rom) = 0;
	virtual void loadBootROM(std::string file) = 0;
	virtual inline bool isBRActive() = 0;
	virtual void disableBR() = 0;
	virtual ~Mem() = default;
};

void handleIO(uint8_t addr, uint8_t val, Mem* m, std::vector<uint8_t> &io);
bool loadBR(std::string& file, std::vector<uint8_t>& rom);
void loadR(std::ifstream& f, std::vector<uint8_t>& rom);

class NoMBC : public Mem {
public:
	NoMBC(uint8_t ram) {
		bROM = std::vector<uint8_t>(0x100);
		rom = std::vector<uint8_t>(0x8000);
		vRAM = std::vector<uint8_t>(0x2000);
		cRAM = std::vector<uint8_t>(0x2000 * ram);
		wRAM = std::vector<uint8_t>(0x207F);
		oam = std::vector<uint8_t>(0xA0);
		io = std::vector<uint8_t>(0x80);
		ie = 0;
		cRAM_enabled = ram;
	};

	NoMBC() : NoMBC(0) {};

	~NoMBC() = default;

	inline uint8_t get(uint16_t addr) {
		if (boot_rom_active && addr < 0x100) {
			return bROM[addr];
		}
		else if (addr < 0x8000) {
			return rom[addr];
		}
		else if (addr < 0xA000) {
			return vRAM[addr - 0x8000];
		}
		else if (addr < 0xC000) {
			return cRAM_enabled ? cRAM[addr - 0xA000] : 0x0;
		}
		else if (addr < 0xE000) {
			return wRAM[addr - 0xC000];
		}
		else if (addr < 0xFE00) {
			return wRAM[addr - 0xE000];
		}
		else if (addr < 0xFEA0) {
			return oam[addr - 0xFE00];
		}
		else if (addr < 0xFF00) {
			return 0;
		}
		else if (addr < 0xFF80) {
			return io[addr - 0xFF00];
		}
		else {
			if (addr == 0xFFFF) {
				return ie;
			}
			else {
				return wRAM[0x2000 + (addr - 0xFF80)];
			}
		}
	}

	inline void set(uint16_t addr, uint8_t val) {
		if (addr >= 0x8000 && addr < 0xA000) {
			vRAM[addr - 0x8000] = val;
		}
		else if (addr < 0xC000) {
			if (cRAM_enabled) {
				cRAM[addr - 0xA000] = val;
			}
		}
		else if (addr < 0xE000) {
			wRAM[addr - 0xC000] = val;
		}
		else if (addr < 0xFE00) {
			wRAM[addr - 0xE000] = val;
		}
		else if (addr < 0xFEA0) {
			oam[addr - 0xFE00] = val;
		}
		else if (addr >= 0xFF00 && addr < 0xFF80) {	
			handleIO(addr - 0xFF00, val, this, this->io);
		}
		else {
			if (addr == 0xFFFF) {
				ie = val;
			}
			else {
				wRAM[0x2000 + (addr - 0xFF80)] = val;
			}
		}
	}

	void loadROM(std::ifstream& f) {
		loadR(f, rom);
	}

	void loadBootROM(std::string f) {
		boot_rom_active = loadBR(f, bROM);
	}

	inline bool isBRActive() {
		return boot_rom_active;
	}

	void disableBR() {
		boot_rom_active = false;
	}

private:
	bool cRAM_enabled;
	std::vector<uint8_t> bROM, rom, vRAM, cRAM, wRAM, oam, io;
	uint8_t ie;
	bool boot_rom_active = false;
};

class MBC1 : public Mem {
public:
	MBC1(uint8_t nRAM, uint16_t nROM) {
		bROM = std::vector<uint8_t>(0x100);
		rom = std::vector<uint8_t>(0x8000 + (0x4000 * (nROM - 2)));
		vRAM = std::vector<uint8_t>(0x2000);
		switch (nRAM) {
		case 1:
			cRAM = std::vector<uint8_t>(0x800); break;
		case 2:
			cRAM = std::vector<uint8_t>(0x2000); break;
		case 3:
			cRAM = std::vector<uint8_t>(0x8000); break;
		}
		wRAM = std::vector<uint8_t>(0x207F);
		oam = std::vector<uint8_t>(0xA0);
		io = std::vector<uint8_t>(0x80);
		ie = 0;
		rom_banks = nROM;
		cRAM_enabled = ram_banks = nRAM;
	}

	~MBC1() = default;

	inline uint8_t get(uint16_t addr) {
		if (boot_rom_active && addr < 0x100) {
			return bROM[addr];
		}
		else if (addr < 0x4000) {
			uint8_t zero_bank_number;

			if (rom_banks <= 32) {
				zero_bank_number = 0;
			}
			else if (rom_banks == 64) {
				zero_bank_number = (ram_bank_number & 1) << 5;
			}
			else {
				zero_bank_number = ((ram_bank_number & 1) << 5) | ((ram_bank_number & 2) << 5);
			}

			return mode ? rom[addr] : rom[0x4000 * zero_bank_number + addr];
		}
		else if (addr < 0x8000) {
			uint8_t high_bank_number = rom_bank_number;

			if (rom_banks == 64) {
				high_bank_number |= (ram_bank_number & 1) << 5;
			}
			else if (rom_banks == 128) {
				high_bank_number |= ((ram_bank_number & 1) << 5) | ((ram_bank_number & 2) << 5);
			}

			return rom[0x4000 * high_bank_number + (addr - 0x4000)];
		}
		else if (addr < 0xA000) {
			return vRAM[addr - 0x8000];
		}
		else if (addr < 0xC000) {
			if (cRAM_enabled) {
				if (ram_banks == 3) {
					return mode ? cRAM[addr - 0xA000] : cRAM[0x2000 * ram_bank_number + (addr - 0xA000)];
				}

				return cRAM[(addr - 0xA000) % cRAM.size()];
			}
			
			return 0xFF;
		}
		else if (addr < 0xE000) {
			return wRAM[addr - 0xC000];
		}
		else if (addr < 0xFE00) {
			return wRAM[addr - 0xE000];
		}
		else if (addr < 0xFEA0) {
			return oam[addr - 0xFE00];
		}
		else if (addr < 0xFF00) {
			return 0;
		}
		else if (addr < 0xFF80) {
			if (addr == 0xff44) {
				return 0x90;
			}

			return io[addr - 0xFF00];
		}
		else {
			if (addr == 0xFFFF) {
				return ie;
			}
			else {
				return wRAM[0x2000 + (addr - 0xFF80)];
			}
		}
	}

	inline void set(uint16_t addr, uint8_t val) {
		if (addr < 0x2000) {
				cRAM_enabled = ((val & 0xF) == 0xA) && ram_banks;
		}
		else if (addr < 0x4000) {
			uint16_t num = val & (std::min(rom_banks - 1, 31));

			rom_bank_number = val == 0 ? 1 : num;
		}
		else if (addr < 0x6000) {
			ram_bank_number = val & 3;
		}
		else if (addr < 0x8000) {
			mode = val & 1;
		}
		else if (addr < 0xA000) {
			vRAM[addr - 0x8000] = val;
		}
		else if (addr < 0xC000) {
			if (cRAM_enabled) {
				if (ram_banks == 3) {
					if (mode) {
						cRAM[addr - 0xA000] = val;
					}
					else {
						cRAM[0x2000 * ram_bank_number + (addr - 0xA000)] = val;
					}
				}
				else {
					cRAM[(addr - 0xA000) % cRAM.size()] = val;
				}
			}
		}
		else if (addr < 0xE000) {
			wRAM[addr - 0xC000] = val;
		}
		else if (addr < 0xFE00) {
			wRAM[addr - 0xE000] = val;
		}
		else if (addr < 0xFEA0) {
			oam[addr - 0xFE00] = val;
		}
		else if (addr >= 0xFF00 && addr < 0xFF80) {
			handleIO(addr - 0xFF00, val, this, this->io);
		}
		else {
			if (addr == 0xFFFF) {
				ie = val;
			}
			else {
				wRAM[0x2000 + (addr - 0xFF80)] = val;
			}
		}
	}

	void loadROM(std::ifstream& f) {
		loadR(f, rom);
	}

	void loadBootROM(std::string f) {
		boot_rom_active = loadBR(f, bROM);
	}

	inline bool isBRActive() {
		return boot_rom_active;
	}

	void disableBR() {
		boot_rom_active = false;
	}

private:
	bool cRAM_enabled = false;
	std::vector<uint8_t> bROM, rom, vRAM, cRAM, wRAM, oam, io;
	uint8_t ie;
	uint16_t rom_banks;
	uint8_t ram_banks;
	uint16_t rom_bank_number = 1;
	uint16_t ram_bank_number = 1;
	bool mode = false;
	bool boot_rom_active = false;
};

#endif // MEMORY_H