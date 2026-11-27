#include "game/race/replay_bare.h"

int32_t replay_bare_frame(ReplayBare *b, int32_t down, int32_t on_controls, uint32_t now_ms)
{
    const int32_t pressed = down != 0 && b->was_down == 0;
    b->was_down = down != 0;
    if (!pressed) return 0;
    if (b->bare == 0 && on_controls != 0) {   /* a button's click: it is no half of a double click either */
        b->armed = 0;
        return 0;
    }
    if (b->armed != 0 && now_ms - b->first_ms <= REPLAY_BARE_DOUBLE_MS) {   /* unsigned: right across the clock's wrap */
        b->armed = 0;
        b->bare = !b->bare;
        return 1;
    }
    b->armed = 1, b->first_ms = now_ms;
    return 0;
}
