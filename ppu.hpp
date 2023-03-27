#ifndef PPU_H
#define PPU_H

SDL_Window* win;
SDL_Renderer* renderer;
SDL_Texture* renderTarget;

std::array<uint8_t, 262144> background;
std::array<uint8_t, 262144> window;
std::array<uint8_t, 262144> sprites;

#endif
