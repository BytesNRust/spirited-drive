/* The live race's panel picks (race_view_pick.h). */
#include "game/race/race_view_pick.h"

#include <stddef.h>

enum {
    KEY_SPACE = 0x20, KEY_RETURN = 0xd, KEY_UP = 0x4800, KEY_DOWN = 0x5000, KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00,
    VIEW_FIRST = 1, VIEW_OWN = 2, VIEW_SECOND = 3, VIEW_CARS_FROM = 4, VIEW_CARS_TO = 10, VIEW_LAST_CAR = 10,
    VIEW_ANY = 11, VIEW_HUMAN = 12, VIEW_CARS_BASE = 2
};

/* The number keys: the flag, the view, whether it needs racers, more than how many */
static const struct { uint8_t flag; int32_t view, needs, racers_above; } NUMBER_KEYS[] = {
    { 0x33, VIEW_OWN, 0, 0 }, { 0x34, VIEW_ANY, 0, 0 }, { 2, VIEW_SECOND, 0, 0 },
    { 3, 4, 1, 1 }, { 4, 5, 1, 2 }, { 5, 6, 1, 3 }, { 6, 7, 1, 4 }, { 7, 8, 1, 5 }, { 8, 9, 1, 6 }, { 9, 10, 1, 7 }
};

static int32_t take(RaceViewPick *p, int32_t view, int32_t key)
{
    p->view = view;
    p->redraw = 1;
    p->last_key = key;
    return 1;
}

static int32_t number_key(RaceViewPick *p, const uint8_t key_flags[KEY_FLAGS], const RaceViewPickInputs *in)
{
    for (size_t i = 0; i < sizeof NUMBER_KEYS / sizeof NUMBER_KEYS[0]; i++) {
        if (key_flags[NUMBER_KEYS[i].flag] == 0) continue;
        if (NUMBER_KEYS[i].needs && in->racers <= NUMBER_KEYS[i].racers_above) break;
        return take(p, NUMBER_KEYS[i].view, 0);
    }
    p->last_key = 0;
    return 0;
}

static void left(RaceViewPick *p, const RaceViewPickInputs *in)
{
    p->cursor--;
    int32_t at = p->cursor;
    if (at == VIEW_FIRST && in->replay == 0) at = 0;
    if (at < VIEW_FIRST) p->cursor = in->viewed < in->humans ? VIEW_HUMAN : VIEW_ANY;
    else if (at == VIEW_LAST_CAR) p->cursor = in->racers + VIEW_CARS_BASE;
}

static void right(RaceViewPick *p, const RaceViewPickInputs *in)
{
    p->cursor++;
    const int32_t at = p->cursor;
    if (at > VIEW_HUMAN || (at == VIEW_HUMAN && in->viewed >= in->humans))
        p->cursor = in->replay != 0 ? VIEW_FIRST : VIEW_OWN;
    else if (at >= VIEW_CARS_FROM && at <= VIEW_CARS_TO && at > in->racers + VIEW_CARS_BASE)
        p->cursor = VIEW_ANY;
}

/* A new key's move of the cursor; 1 when it moved */
static int32_t move(RaceViewPick *p, int32_t key, const RaceViewPickInputs *in)
{
    if (key == KEY_LEFT) {
        left(p, in);
    } else if (key == KEY_RIGHT) {
        right(p, in);
    } else if (key == KEY_UP) {
        if (p->cursor == VIEW_OWN) p->cursor = VIEW_SECOND;
        else if (p->cursor == VIEW_ANY) p->cursor = in->racers + VIEW_CARS_BASE;
        else return 0;
    } else if (key == KEY_DOWN) {
        if (p->cursor == VIEW_SECOND) p->cursor = VIEW_OWN;
        else if (p->cursor == in->racers + VIEW_CARS_BASE) p->cursor = VIEW_ANY;
        else return 0;
    } else {
        return 0;
    }
    p->redraw = 1;
    return 1;
}

int32_t race_view_pick(RaceViewPick *p, const uint8_t key_flags[KEY_FLAGS], JoyState *j, const JoyOps *ops,
                       const RaceViewPickInputs *in)
{
    const int32_t key = menu_key(key_flags, j, in->present, in->device, ops);
    if (key == 0) return number_key(p, key_flags, in);
    const int32_t moved = key != p->last_key ? move(p, key, in) : 0;
    if (key == KEY_SPACE || key == KEY_RETURN) return take(p, p->cursor, key);
    p->last_key = key;
    return moved;
}
