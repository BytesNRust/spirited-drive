/* port (dep59, Lunar Springs' entry, L4): Lunar Springs' bank put as a bank the menus draw from (lunar_art.h); apart from
 * lunar_art_glue.c so the tests take that without ea_lib */
#include "game/frontend/lunar_art.h"
#include "game/frontend/ea_lib.h"

int32_t lunar_art_bank(LunarArtKind kind)
{
    char name[32];
    size_t size = 0;
    uint8_t *bank = lunar_art_cached(kind, &size, name, sizeof name);
    return bank != NULL ? ea_bank_put(bank, size, name) : 0;
}
