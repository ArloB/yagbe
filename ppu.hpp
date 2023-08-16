#ifndef PPU_H
#define PPU_H

#include <SDL.h>
#include <array>
#include <memory>

class PPUObj {
public:
    PPUObj();
    ~PPUObj() = default;
    void step(int cycles);

private:
    SDL_Window* win;
    SDL_Renderer* renderer;
    SDL_Texture* renderTarget;

    std::array<uint8_t, 262144> background;
    std::array<uint8_t, 262144> window;
    std::array<uint8_t, 262144> sprites;
    std::array<uint8_t, 92160> framebuffer;

    uint16_t ppu_cycles;
    uint8_t last_mode;

    bool dFlag;
    bool lFlag;

    void calculateMaps(uint8_t row);
    void drawFrame();
};

inline std::unique_ptr<PPUObj> PPU;

#endif
