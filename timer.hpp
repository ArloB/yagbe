#ifndef TIMER_H
#define TIMER_H

class Timer {
public:
    Timer();
    void reset();
    void tick(int cycles);
private:
    uint16_t divider;
    unsigned int timer;
};

#endif