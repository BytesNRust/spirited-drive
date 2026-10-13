#include "game/frontend/control_central_art.h"

#include <stddef.h>

/* 00432884 */
void control_central_art_free(ControlCentralArt *a, FrontendSession *f, const ControlCentralArtOps *ops)
{
    ops->free(ops->ctx, a->art_4bafcc), a->art_4bafcc = NULL;
    ops->free(ops->ctx, a->art_4bafc0), a->art_4bafc0 = NULL;
    ops->free(ops->ctx, a->art_4bafc4), a->art_4bafc4 = NULL;
    ops->free(ops->ctx, a->art_4bafc8), a->art_4bafc8 = NULL;
    ops->free(ops->ctx, a->art_4bafd0);
    if (a->art_4bafd4 != NULL) ops->free(ops->ctx, a->art_4bafd4), a->art_4bafd4 = NULL;
    if (a->art_4bafd8 != NULL) ops->free(ops->ctx, a->art_4bafd8), a->art_4bafd8 = NULL;
    for (int i = 0; i < 6; i++) f->unknown_4bafb4[i] = 0x14;
    ops->free_inverse(ops->ctx);
}
