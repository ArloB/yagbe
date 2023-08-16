#include "ppu.hpp"

#include <iostream>
#include "memory.hpp"

PPUObj::PPUObj() {
    if (SDL_CreateWindowAndRenderer(160, 144, 0, &win, &renderer)) {
        std::cout << "Window could not be created\n";
    }

    SDL_SetWindowSize(win, 480, 432);
    SDL_RenderSetLogicalSize(renderer, 160, 144);

    SDL_SetWindowResizable(win, SDL_TRUE);

    

    renderTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    (background = std::array<uint8_t, 262144>()).fill({});
    (window = std::array<uint8_t, 262144>()).fill({});
    (sprites = std::array<uint8_t, 262144>()).fill({});
    (framebuffer = std::array<uint8_t, 92160>()).fill({});

    memory->set(0xFF42, 0);
    memory->set(0xFF43, 0);

    ppu_cycles = 0;
    last_mode = 0;

    lFlag = false;
    dFlag = false;
};

int COLORS[] = {
        0xff,0xff,0xff,
        0xaa,0xaa,0xaa,
        0x55,0x55,0x55,
        0x00,0x00,0x00
};

void PPUObj::calculateMaps(uint8_t row) {
    uint8_t LCDC = memory->get(0xff40);
    
    uint8_t SCX = memory->get(0xff43);
    uint8_t SCY = memory->get(0xff42);
    uint8_t WY = memory->get(0xff4a);
    uint8_t WX = memory->get(0xff4b) - 7;

    uint16_t tilemap = LCDC >> 3 & 1 ? 0x9c00 : 0x9800;
    uint16_t tiledata = LCDC >> 4 & 1 ? 0x8000 : 0x8800;
    
    uint8_t palette = memory->get(0xff47);

    for (int j = 0; j < 256; j++) {
        uint8_t offY = row + SCY;
        uint8_t offX = j + SCX;
        int colour = 0;

        int t = memory->get(tilemap + ((offY / 8 * 32) + (offX / 8)));

        if (tiledata == 0x8800) {
            colour = (memory->get(tiledata + 0x800 + ((int8_t)t * 0x10) + (offY % 8 * 2)) >> (7 - (offX % 8)) & 0x1) + ((memory->get(tiledata + 0x800 + ((int8_t)t * 0x10) + (offY % 8 * 2) + 1) >> (7 - (offX % 8)) & 0x1) * 2);
        }
        else {
            colour = (memory->get(tiledata + (t * 0x10) + (offY % 8 * 2)) >> (7 - (offX % 8)) & 0x1) + (memory->get(tiledata + (t * 0x10) + (offY % 8 * 2) + 1) >> (7 - (offX % 8)) & 0x1) * 2;
        }

        uint8_t colorfrompal = (palette >> (2 * colour)) & 3;
        background[(row * 256 * 4) + (j * 4)] = COLORS[colorfrompal * 3];
        background[(row * 256 * 4) + (j * 4) + 1] = COLORS[colorfrompal * 3 + 1];
        background[(row * 256 * 4) + (j * 4) + 2] = COLORS[colorfrompal * 3 + 2];
        background[(row * 256 * 4) + (j * 4) + 3] = 0xff;

        if (LCDC >> 5 & 1 && WX <= j && j <= WX + 160 && WY <= row && row <= WY + 144) {
            t = memory->get(tilemap + (((row - WY) / 8 * 32) + ((j - WX) / 8)));

            if (tiledata == 0x8800) {
                colour = (memory->get(tiledata + 0x800 + ((int8_t)t * 0x10) + ((row - WY) % 8 * 2)) >> (7 - ((j - WX) % 8)) & 0x1) + ((memory->get(tiledata + 0x800 + ((int8_t)t * 0x10) + ((row - WY) % 8 * 2) + 1) >> (7 - ((j - WX) % 8)) & 0x1) * 2);
            }
            else {
                colour = (memory->get(tiledata + (t * 0x10) + ((row - WY) % 8 * 2)) >> (7 - ((j - WX) % 8)) & 0x1) + (memory->get(tiledata + (t * 0x10) + ((row - WY) % 8 * 2) + 1) >> (7 - ((j - WX) % 8)) & 0x1) * 2;
            }

            colorfrompal = (palette >> (2 * colour)) & 3;

            window[(row * 256 * 4) + (j * 4)] = COLORS[colorfrompal * 3];
            window[(row * 256 * 4) + (j * 4) + 1] = COLORS[colorfrompal * 3 + 1];
            window[(row * 256 * 4) + (j * 4) + 2] = COLORS[colorfrompal * 3 + 2];
            window[(row * 256 * 4) + (j * 4) + 3] = 0xff;
        }
    }

    if (memory->get(0xff40) >> 1 & 1) {
        for (uint16_t i = 0xfe00; i < 0xfe9f; i += 4) {
            uint8_t y = memory->get(i);
            uint8_t x = memory->get(i + 1);
            uint8_t height = (memory->get(0xff40) >> 2 & 0x01) ? 16 : 8;

            if (row >= (y - 16) && row <= ((y - 16) + height)) {
                uint8_t t = memory->get(i + 2);
                uint8_t f = memory->get(i + 3);
                uint8_t colour = 0;

                for (int u = 0; u < height; u++) {
                    for (int v = 0; v < 8; v++) {
                        switch (f & 0x60) {
                        case 0x00:
                            colour = (memory->get(0x8000 + (t * 0x10) + (u * 2)) >> (7 - v) & 0x1) + (memory->get(0x8000 + (t * 0x10) + (u * 2) + 1) >> (7 - v) & 0x1) * 2;
                            break;
                        case 0x20:
                            colour = (memory->get(0x8000 + (t * 0x10) + (u * 2)) >> v & 0x1) + (memory->get(0x8000 + (t * 0x10) + (u * 2) + 1) >> v & 0x1) * 2;
                            break;
                        case 0x40:
                            colour = (memory->get(0x8000 + (t * 0x10) + ((height - u - 1) * 2)) >> (7 - v) & 0x1) + (memory->get(0x8000 + (t * 0x10) + ((height - u - 1) * 2) + 1) >> (7 - v) & 0x1) * 2;
                            break;
                        case 0x60:
                            colour = (memory->get(0x8000 + (t * 0x10) + ((height - u - 1) * 2)) >> v & 0x1) + (memory->get(0x8000 + (t * 0x10) + ((height - u - 1) * 2) + 1) >> v & 0x1) * 2;
                            break;
                        default:
                            break;
                        }

                        uint8_t colorfrompal = (memory->get(f >> 4 & 1 ? 0xff49 : 0xff48) >> (2 * colour)) & 3;

                        if (colour && ((y + u) >= 16 && (y + u) <= 0xff) && ((x + v) >= 8 && (x + v) <= 0xff)) {
                            sprites[((y + u - 16) * 256 * 4) + ((x + v - 8) * 4)] = COLORS[colorfrompal * 3];
                            sprites[((y + u - 16) * 256 * 4) + ((x + v - 8) * 4) + 1] = COLORS[colorfrompal * 3 + 1];
                            sprites[((y + u - 16) * 256 * 4) + ((x + v - 8) * 4) + 2] = COLORS[colorfrompal * 3 + 2];
                            sprites[((y + u - 16) * 256 * 4) + ((x + v - 8) * 4) + 3] = 0xff;
                        }
                    }
                }
            }
        }
    }
}

void PPUObj::drawFrame() {
    for (int row = 0; row < 144; row++) {
        for (int col = 0; col < 160; col++) {
            int yoff = (row * 256 * 4);
            int xoff = (col * 4);

            framebuffer[(row * 160 * 4) + (col * 4)] = background[yoff + xoff];
            framebuffer[(row * 160 * 4) + (col * 4) + 1] = background[yoff + xoff + 1];
            framebuffer[(row * 160 * 4) + (col * 4) + 2] = background[yoff + xoff + 2];
            framebuffer[(row * 160 * 4) + (col * 4) + 3] = background[yoff + xoff + 3];

            if (window[yoff + xoff + 3]) {
                framebuffer[(row * 160 * 4) + (col * 4)] = window[yoff + xoff];
                framebuffer[(row * 160 * 4) + (col * 4) + 1] = window[yoff + xoff + 1];
                framebuffer[(row * 160 * 4) + (col * 4) + 2] = window[yoff + xoff + 2];
                framebuffer[(row * 160 * 4) + (col * 4) + 3] = window[yoff + xoff + 3];
            }

            if (sprites[yoff + xoff + 3]) {
                framebuffer[(row * 160 * 4) + (col * 4)] = sprites[yoff + xoff];
                framebuffer[(row * 160 * 4) + (col * 4) + 1] = sprites[yoff + xoff + 1];
                framebuffer[(row * 160 * 4) + (col * 4) + 2] = sprites[yoff + xoff + 2];
                framebuffer[(row * 160 * 4) + (col * 4) + 3] = sprites[yoff + xoff + 3];
            }
        }
    }

    SDL_UpdateTexture(renderTarget, NULL, framebuffer.data(), 160 * sizeof(unsigned char) * 4);
    SDL_RenderCopy(renderer, renderTarget, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void PPUObj::step(int cycles) {
    ppu_cycles += cycles * 4;

    uint8_t LY = memory->get(0xff44);

    if (ppu_cycles <= 80) {
        memory->set(0xff41, (memory->get(0xff41) & 0xfc) | 2);

        if (last_mode != 2 && (memory->get(0xff41) >> 5) & 1) {
            memory->set(0xff0f, memory->get(0xff0f) | 2);
        }

        last_mode = 2;
    } 
    else if (ppu_cycles <= 252) {
        memory->set(0xff41, (memory->get(0xff41) & 0xfc) | 3);
    } 
    else if (ppu_cycles <= 456) {
        memory->set(0xff41, (memory->get(0xff41) & 0xfc));

        if (last_mode != 0 && (memory->get(0xff41) >> 3) & 1) {
            memory->set(0xff0f, memory->get(0xff0f) | 2);
        }

        last_mode = 0;
    }

    if (LY >= 144) {
        memory->set(0xff41, (memory->get(0xff41) & 0xfc) | 1);

        if (last_mode != 1 && (memory->get(0xff41) >> 4) & 1) {
            memory->set(0xff0f, memory->get(0xff0f) | 2);
        }

        last_mode = 1;
    }

    if (!lFlag && ppu_cycles > 252 && LY < 145) {
        calculateMaps(LY);
        
        lFlag = true;
    }

    if (!dFlag && LY == 144 && memory->get(0xff40) >> 7) {
        memory->set(0xff0f, memory->get(0xff0f) | 1);

        dFlag = true;
        drawFrame();

        background.fill({});
        sprites.fill({});
        window.fill({});
    }

    if (ppu_cycles > 456) {
        ppu_cycles -= 456;
        memory->set(0xff44, LY + 1);
        lFlag = false;
    }

    if (LY == memory->get(0xff45) && (memory->get(0xff41) >> 6) & 1) {
		memory->set(0xff0f, memory->get(0xff0f) | 2);
        memory->set(0xff41, memory->get(0xff41) | 4);
    } else {
		memory->set(0xff41, memory->get(0xff41) & 0xfb);
	}

    if (LY > 154) {
        memory->set(0xff44, 0);
        dFlag = false;
    }
}