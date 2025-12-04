#include "timer.hpp"
#include "memory.hpp"

/**
 * @brief Constructor for the Timer object.
 * Initializes the divider register and timer counter to 0.
 */
Timer::Timer() {
    counter = 0;
    divider = 0;
    timer = 0;
    tima_reload_countdown = -1;
}

/**
 * @brief Resets the divider register (DIV, 0xFF04) to 0.
 * This typically happens when 0xFF04 is written to.
 */
void Timer::resetdiv() {
    divider = 0;
}

/**
 * @brief Ticks the timer system by the given number of CPU M-cycles.
 *
 * Updates the DIV register (0xFF04) based on elapsed cycles. The DIV register
 * increments at a rate of 16384 Hz (every 256 T-cycles / 64 M-cycles).
 * If the timer is enabled (TAC bit 2 is set), it updates the TIMA register (0xFF05)
 * at a frequency determined by TAC bits 0-1.
 * If TIMA overflows (goes past 0xFF), it is reset to the value of TMA (0xFF06),
 * and a timer interrupt flag (bit 2) is set in IF (0xFF0F).
 *
 * @param cycles The number of CPU M-cycles that have passed.
 */
void Timer::tick(int cycles) {
    uint32_t temp_counter = counter + cycles;
    divider += temp_counter / 64;
    counter = temp_counter % 64;

    for (int i = 0; i < cycles; ++i) {
        if (tima_reload_countdown > 0) {
            tima_reload_countdown--;
            if (tima_reload_countdown == 0) {
                uint8_t tma = memory->get(0xff06);
                memory->set(0xff05, tma); // Reload TIMA
                memory->set(0xff0f, memory->get(0xff0f) | 0x04);
            }
        }

        if (tima_reload_countdown <= 0) {
            uint8_t TAC = memory->get(0xff07);
            if (TAC & 0x04) { 
                timer += 4; 

                unsigned int freq = 0;
                switch (TAC & 0x03) {
                    case 0: freq = 1024; break;
                    case 1: freq = 16;   break;
                    case 2: freq = 64;   break;
                    case 3: freq = 256;  break;
                    default: freq = 1024;
                }

                if (timer >= freq) {
                    timer -= freq; 
                    
                    uint8_t current_tima = memory->get(0xff05);

                    current_tima++;

                    if (current_tima == 0x00) {
                        memory->set(0xff05, 0x00);
                        tima_reload_countdown = 1;
                    } else {
                        memory->set(0xff05, current_tima);
                    }
                }
            }
        }
    }
}

uint8_t Timer::readdiv() {
    return (divider >> 8) & 0xff;
};