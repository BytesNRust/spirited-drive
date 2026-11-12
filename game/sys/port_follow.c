#include "game/sys/port_follow.h"

int port_follow(PortFollow *f, int (*read)(void))
{
    const uint32_t now = port_settings_generation();
    if (f->seen != now) f->value = read(), f->seen = now;
    return f->value;
}
