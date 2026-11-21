/* usage: make_test_psd <out.psd> paint|lamps|empty|hand
 * A 4 x 1 mask PSD for the shell flows (tests/psd_fixture.h): layers "texture", "mask" and a hidden painted "draft". paint: texels 0 and 2 reflective; lamps: texel 0 reflective, 1 a lamp;
 * empty: nothing painted in the mask; hand: a mask PNG for the same name painted by "hand" is written next to it by the flow, not here. Not a test. */
#include <stdio.h>
#include <string.h>

#include "psd_fixture.h"

int main(int argc, char **argv)
{
    if (argc != 3) return fprintf(stderr, "usage: make_test_psd <out.psd> paint|lamps|empty\n"), 2;
    static const uint8_t PAINT[16] = { 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0 };
    static const uint8_t LAMPS[16] = { 255, 255, 255, 255, 255, 255, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t NONE[16] = { 0 };
    static const uint8_t DRAFT[16] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    const uint8_t *mask = strcmp(argv[2], "paint") == 0 ? PAINT : strcmp(argv[2], "lamps") == 0 ? LAMPS : NONE;
    FxLayer l[3] = { { "texture", NULL, 0, 0, 0, 4, 1, DRAFT, 0, 1, 0 }, { "mask", NULL, 0, 0, 0, 4, 1, mask, 0, 1, 0 }, { "draft", NULL, 0, 0, 0, 4, 1, DRAFT, 1, 1, 0 } };
    FxBuf f = fx_psd(4, 1, l, 3, 8, 3, 1);
    FILE *out = fopen(argv[1], "wb");
    if (out == NULL || fwrite(f.p, 1, f.n, out) != f.n) return perror(argv[1]), 1;
    return fclose(out) != 0;
}
