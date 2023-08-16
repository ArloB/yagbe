#include "gba.hpp"
#include "timer.hpp"

Timer::Timer() {
    divider = 0;
    timer = 0;
}

void Timer::resetdiv() {    
    divider = 0;
}

void Timer::tick(int cycles) {
    divider += cycles;

    if (divider >= 256) {
        divider -= 256;
        memory->set(0xff04, memory->get(0xff04) + 1);
    }

    uint8_t TAC = memory->get(0xff07);

    if (TAC & 0x04) {
        timer += cycles * 4;

        unsigned int freq = 0;

        switch (TAC & 0x03) {
        case 1:
            freq = 16; break;
        case 2:
            freq = 64; break;
        case 3:
            freq = 256; break;
        default:
            freq = 1024;
        }

        while (timer >= freq) {
            timer -= freq;

            uint8_t TIMA = memory->get(0xff05) + 1;

            if (TIMA == 0x00) {
                TIMA = memory->get(0xff06);
                memory->set(0xff0f, memory->get(0xff0f) | 0x04);
            }

            memory->set(0xff05, TIMA);
         }
    }
}