#include "game/race/single_rival.h"

int single_rival_car(int from_menus, int menu_pick)
{
    return from_menus && menu_pick >= 0 && menu_pick <= SINGLE_RIVAL_FIELD ? menu_pick : SINGLE_RIVAL_FIELD;
}
