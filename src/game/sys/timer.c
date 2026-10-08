#include "game/sys/timer.h"

#include "platform/platform.h"

static uint32_t start_ms;
static uint32_t deadline;  /* 004c61ac */

void timer_init(void)
{
    start_ms = platform_ticks_ms();
    deadline = 0;
}

uint32_t timer_ticks(void)
{
    return (platform_ticks_ms() - start_ms) / (1000 / TIMER_HZ);
}

void timer_set_deadline(int ticks)
{
    deadline = timer_ticks() + (uint32_t)ticks;
}

int timer_deadline_passed(void)
{
    return timer_ticks() >= deadline;
}
