#ifndef PPU_H
#define PPU_H

#include <SDL.h>
#include <array>
#include <memory>

/**
 * @brief Pixel Processing Unit (PPU) class.
 * Handles all graphics rendering, including background, window, and sprites.
 * Manages PPU timing, modes, and interaction with VRAM and OAM.
 */
class PPUObj {
public:
    /**
     * @brief Constructor for the PPUObj (Pixel Processing Unit Object).
     *
     * Initializes SDL, creates a window and renderer, and sets up textures
     * for background, window, sprites, and the final framebuffer.
     * Also initializes PPU-related memory registers (SCY, SCX) and internal state.
     */
    PPUObj();
    /**
     * @brief Destructor for the PPUObj.
     * Defaulted, SDL cleanup might be handled elsewhere or upon program termination.
     */
    ~PPUObj() = default;
    /**
     * @brief Steps the PPU simulation by a given number of CPU cycles.
     *
     * Updates the PPU's internal cycle counter. Based on the cycle count and the
     * current scanline (LY register), it transitions the PPU through its different
     * modes (OAM Scan, Drawing, HBlank, VBlank).
     * It sets the appropriate mode flags in the STAT register (0xFF41) and requests
     * LCD STAT interrupts if enabled and conditions are met.
     * When a scanline is completed (during HBlank), `calculateMaps` is called.
     * When the VBlank period starts (LY=144), a VBlank interrupt is requested,
     * and `drawFrame` is called to render the completed frame.
     *
     * @param cycles The number of CPU M-cycles that have passed. PPU cycles are 4x this.
     */
    void step(int cycles);

private:
    SDL_Window* win;
    SDL_Renderer* renderer;
    SDL_Texture* renderTarget;

    std::array<uint8_t, 262144> background;
    std::array<uint8_t, 262144> window;
    std::array<uint8_t, 262144> sprites;
    std::array<uint8_t, 92160> framebuffer; // 160*144*4 (RGBA)

    uint16_t ppu_cycles;
    uint8_t last_mode;

    bool dFlag; 
    bool lFlag; 

    /**
     * @brief Calculates pixel data for background, window, and sprites for a given scanline.
     * @param row The current scanline number (LY register value).
     */
    void calculateMaps(uint8_t row);
    /**
     * @brief Composites the rendered layers (background, window, sprites) into the framebuffer
     * and draws the final frame to the SDL window.
     */
    void drawFrame();
};

/**
 * @brief Global unique pointer to the PPU object.
 */
inline std::unique_ptr<PPUObj> PPU;

#endif
