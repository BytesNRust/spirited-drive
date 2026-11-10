/* port: the race's viewport tables read from NFS_WIN.EXE (the 1:1 port of tools/gen_view_tables.py): the viewport layouts
 * (004b7080, 2 screen modes x 17 rows of 0x28 bytes), the view rows (004b75d0, 2 modes x 7 rows of 0x34 bytes) and 004055d0's
 * heights by model (004b790c, 2 modes x 12 models of 3 words), as the exe starts. */
#include "game/exe/exe_tables.h"

#include <stdio.h>

static int32_t le32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int fail(char *err, size_t errsz, uint32_t va, const char *what)
{
    if (err != NULL && errsz > 0) snprintf(err, errsz, "the exe has no data at %08x (%s)", va, what);
    return -1;
}

int exe_tables_view(const ExeImage *img, ExeTables *t, char *err, size_t errsz)
{
    enum { LAYOUTS = 0x4B7080, ROWS = 0x4B75D0, MODEL_Y = 0x4B790C, LAYOUT_MODE = 0x2A8, ROW_MODE = 0x16C, MODEL_MODE = 0x90 };
    uint8_t b[0x34];
    for (int mode = 0; mode < VIEW_SCREEN_MODES; mode++) {
        for (int r = 0; r < VIEW_LAYOUTS; r++) {
            const uint32_t va = (uint32_t)(LAYOUTS + mode * LAYOUT_MODE + r * 0x28);
            if (exe_va_read(img, va, b, 0x28) != 0) return fail(err, errsz, va, "the viewport layouts");
            ViewLayout *l = &t->race_view_layouts[mode][r];
            l->unknown_00 = le32(b), l->scale = le32(b + 4), l->x = le32(b + 8), l->y = le32(b + 12), l->width = le32(b + 16);
            l->height = le32(b + 20), l->left = le32(b + 24), l->top = le32(b + 28), l->unknown_20 = le32(b + 32), l->unknown_24 = le32(b + 36);
        }
        for (int r = 0; r < VIEW_ROWS; r++) {
            const uint32_t va = (uint32_t)(ROWS + mode * ROW_MODE + r * 0x34);
            if (exe_va_read(img, va, b, 0x34) != 0) return fail(err, errsz, va, "the view rows");
            ViewRow *row = &t->race_view_rows[mode][r];
            row->next = le32(b);
            for (int k = 0; k < VIEW_SIZES; k++) row->layouts[k] = le32(b + 4 + 4 * k);
            for (int k = 0; k < 8; k++) row->unknown_14[k] = le32(b + 20 + 4 * k);
        }
        uint8_t m[MODEL_MODE];
        const uint32_t va = (uint32_t)(MODEL_Y + mode * MODEL_MODE);
        if (exe_va_read(img, va, m, MODEL_MODE) != 0) return fail(err, errsz, va, "the heights by model");
        for (int model = 0; model < VIEW_MODELS; model++)
            for (int k = 0; k < 3; k++) t->race_view_model_y[mode][model][k] = le32(m + 4 * (3 * model + k));
    }
    return 0;
}
