#ifndef TIMER_H
#define TIMER_H

#include <cstdint> // For uint16_t

/**
 * @brief Game Boy Timer class.
 * Emulates the Game Boy's internal timer system, including the DIV, TIMA, TMA, and TAC registers.
 */
class Timer {
public:
    /**
     * @brief Constructor for the Timer.
     * Initializes timer registers.
     */
    Timer();
    /**
     * @brief Resets the DIV register (0xFF04) to 0.
     * This typically occurs when 0xFF04 is written to.
     */
    void resetdiv();
    /**
     * @brief Advances the timer state by a given number of CPU M-cycles.
     * Updates DIV, TIMA, and handles TIMA overflow and interrupt requests.
     * @param cycles The number of CPU M-cycles that have passed.
     */
    void tick(int cycles);
private:
    uint16_t divider;    // Internal counter for DIV register increments
    unsigned int timer;  // Internal counter for TIMA increments
};

#endif