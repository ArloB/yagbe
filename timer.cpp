#include "gba.hpp"
#include "timer.hpp"

Timer::Timer() {
    divider = 0;
    timer = 0;
}

void Timer::reset() {
    divider = 0;
    timer = 0;
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
        case 0:
            freq = 1024;
        }

        while (timer >= freq) {
            timer -= freq;

            uint8_t TIMA = memory->get(0xff05);

            if (TIMA == 0xFF) {
                TIMA = memory->get(0xff06);
                memory->set(0xff0f, memory->get(0xff0f) | 0x04);
            }
            else {
                TIMA++;
            }

            memory->set(0xff05, TIMA);
         }
    }
}