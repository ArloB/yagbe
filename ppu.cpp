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

/**
 * @brief Calculates the pixel data for the background and window layers for a given scanline.
 *
 * This function renders the background and window layers for the specified `row` (scanline).
 * It reads tile data and tile maps from VRAM based on LCDC register settings,
 * SCX/SCY scroll registers, and WX/WY window position registers.
 * Colors are determined using the BGP palette register (0xFF47).
 * The results are stored in the `background` and `window` pixel arrays.
 * It also renders sprites that are visible on this scanline.
 *
 * @param row The current scanline number (LY register value, 0-143 for visible lines).
 */
void PPUObj::calculateMaps(uint8_t row) {
    uint8_t LCDC = memory->get(0xff40);
    
    uint8_t SCX = memory->get(0xff43);
    uint8_t SCY = memory->get(0xff42);
    uint8_t WY = memory->get(0xff4a);
    uint8_t WX = memory->get(0xff4b) - 7;

    uint16_t bgTileMapArea = (LCDC & 0x08) ? 0x9C00 : 0x9800; // LCDC Bit 3 for BG
    uint16_t windowTileMapArea = (LCDC & 0x40) ? 0x9C00 : 0x9800; // LCDC Bit 6 for Window
    bool signedTileAddressing = !(LCDC & 0x10); // LCDC Bit 4: 0 = 0x8800 method, 1 = 0x8000 method
    uint16_t tileDataBaseAddress = (LCDC & 0x10) ? 0x8000 : 0x8800;
    
    uint8_t palette = memory->get(0xff47);

    for (int j = 0; j < 256; j++) { // Iterate across the 256-pixel wide virtual map
        // Background Pixel
        uint8_t offY_bg = row + SCY;
        uint8_t offX_bg = j + SCX;
        int colour_bg = 0;

        uint8_t tile_index_bg = memory->get(bgTileMapArea + ((offY_bg / 8 * 32) + (offX_bg / 8)));
        uint16_t tile_addr_bg;

        if (signedTileAddressing) { // 0x8800 method (signed index)
            tile_addr_bg = tileDataBaseAddress + 0x800 + (((int8_t)tile_index_bg) * 0x10);
        } else { // 0x8000 method (unsigned index)
            tile_addr_bg = tileDataBaseAddress + (tile_index_bg * 0x10);
        }
        colour_bg = (memory->get(tile_addr_bg + (offY_bg % 8 * 2)) >> (7 - (offX_bg % 8)) & 0x1) + 
                    ((memory->get(tile_addr_bg + (offY_bg % 8 * 2) + 1) >> (7 - (offX_bg % 8)) & 0x1) * 2);
        
        uint8_t colorfrompal_bg = (palette >> (2 * colour_bg)) & 3;
        background[(row * 256 * 4) + (j * 4)] = COLORS[colorfrompal_bg * 3];
        background[(row * 256 * 4) + (j * 4) + 1] = COLORS[colorfrompal_bg * 3 + 1];
        background[(row * 256 * 4) + (j * 4) + 2] = COLORS[colorfrompal_bg * 3 + 2];
        background[(row * 256 * 4) + (j * 4) + 3] = 0xff; // Mark as opaque for now

        // Window Pixel (if window enabled and active for this pixel on this row)
        bool windowEnabled = (LCDC & 0x20); // LCDC Bit 5
        if (windowEnabled && WX <= j && WY <= row) {
            // Check if current pixel (j) is within window's horizontal range on screen (0-159)
            // And current row is within window's vertical range on screen (0-143)
            // This check is implicitly handled by how drawFrame later crops from the 256x256 buffers.
            // The WX <= j and WY <= row determines if the window *starts* covering pixels.

            uint8_t offY_win = row - WY;
            uint8_t offX_win = j - WX;
            int colour_win = 0;

            uint8_t tile_index_win = memory->get(windowTileMapArea + ((offY_win / 8 * 32) + (offX_win / 8)));
            uint16_t tile_addr_win;

            if (signedTileAddressing) { // 0x8800 method
                tile_addr_win = tileDataBaseAddress + 0x800 + (((int8_t)tile_index_win) * 0x10);
            } else { // 0x8000 method
                tile_addr_win = tileDataBaseAddress + (tile_index_win * 0x10);
            }

            colour_win = (memory->get(tile_addr_win + (offY_win % 8 * 2)) >> (7 - (offX_win % 8)) & 0x1) +
                         ((memory->get(tile_addr_win + (offY_win % 8 * 2) + 1) >> (7 - (offX_win % 8)) & 0x1) * 2);
            
            uint8_t colorfrompal_win = (palette >> (2 * colour_win)) & 3;

            window[(row * 256 * 4) + (j * 4)] = COLORS[colorfrompal_win * 3];
            window[(row * 256 * 4) + (j * 4) + 1] = COLORS[colorfrompal_win * 3 + 1];
            window[(row * 256 * 4) + (j * 4) + 2] = COLORS[colorfrompal_win * 3 + 2];
            window[(row * 256 * 4) + (j * 4) + 3] = 0xff; // Mark as opaque
        } else {
            // If window not active here, ensure window buffer is transparent for this pixel
            window[(row * 256 * 4) + (j * 4) + 3] = 0x00; 
        }
    }

    // Sprite rendering (largely unchanged for this specific fix, but see conceptual points later)
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

/**
 * @brief Draws the final composed frame to the screen.
 *
 * This function iterates through each pixel of the 160x144 framebuffer.
 * It composites the background, window, and sprite layers (in that order,
 * respecting transparency and priority where applicable, though current sprite
 * implementation might overlay unconditionally if sprite pixel is not transparent).
 * The resulting framebuffer is then updated to an SDL texture and rendered to the screen.
 */
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