#include "game/frontend/screen_clear.h"

/* 00464a30 */
void screen_clear_both(const ScreenClearOps *ops)
{
    for (int page = 0; page < 2; page++) {
        ops->select_default(ops->ctx);
        ops->fill(ops->ctx, 0);
        ops->unlock(ops->ctx);
        ops->flip(ops->ctx);
    }
}

/* 00464a10 */
void screen_clear_one(const ScreenClearOps *ops)
{
    ops->pointer_hide(ops->ctx);
    ops->fill(ops->ctx, 0);
    ops->unlock(ops->ctx);
    ops->flip(ops->ctx);
}
