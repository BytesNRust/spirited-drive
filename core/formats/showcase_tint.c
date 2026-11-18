/* port (dep59 step 5, T7d): see showcase_tint.h. */
#include "formats/showcase_tint.h"

const uint8_t SHOWCASE_TINT_SEPIA[3] = { 0x70, 0x42, 0x14 };

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int showcase_tint_parse(const char *text, uint8_t rgb[3])
{
    if (text == NULL || text[0] != '#') return 0;
    uint8_t out[3];
    for (int i = 0; i < 3; i++) {
        const int hi = hex_digit(text[1 + 2 * i]);
        const int lo = hi < 0 ? -1 : hex_digit(text[2 + 2 * i]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    if (text[7] != '\0') return 0;
    rgb[0] = out[0], rgb[1] = out[1], rgb[2] = out[2];
    return 1;
}

static int brightness(const uint8_t *p) { return (299 * p[0] + 587 * p[1] + 114 * p[2]) / 1000; }

void showcase_tint_apply(uint8_t *rgb, size_t entries, const uint8_t tint[3])
{
    if (rgb == NULL || tint == NULL) return;
    const int tl = brightness(tint);
    for (size_t i = 0; i < entries; i++) {
        uint8_t *p = rgb + 3 * i;
        const int l = brightness(p);
        for (int c = 0; c < 3; c++) {
            const int v = tl > 0 ? tint[c] * l / tl : l;
            p[c] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}
