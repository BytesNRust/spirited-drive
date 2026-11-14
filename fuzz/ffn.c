/* FFN: the original's fonts */
#include "fuzz/fuzz.h"
#include "formats/ffn.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Ffn font;
    if (ffn_open(&font, data, size) != FFN_OK) return 0;
    for (unsigned code = 0; code < 256; code++) {
        FfnGlyph g;
        if (!ffn_glyph(&font, code, &g)) continue;
        for (int y = -1; y <= g.height; y++)
            for (int x = -1; x <= g.width; x++) {
                (void)ffn_glyph_level(&font, &g, x, y);
                (void)ffn_glyph_level_scaled(&font, &g, x, y, 3, 2);
                (void)ffn_glyph_coverage_scaled(&font, &g, x, y, 3, 2, 128);
            }
    }
    (void)ffn_text_width(&font, "Spirited Drive 0123456789 !?");
    return 0;
}
