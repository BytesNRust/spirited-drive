/* psx_ratio_dump <psx_dir> <track> [A|B|C] [limit_percent]: the light ratio a car gets at each node of a track (the one lamp_state reads: the script's
 * context colour or the LGT byte of the road's centre, game/race/psx_lgs.h psx_lgs_car_ratio), and the runs of nodes where the darkest channel is under
 * the limit (default 70, the lamps' switch-on). For each run: the nodes, the ratio's lowest, the LGT centre byte's range, whether a script context
 * covers it (FIXED / RAMP and its colour), and the 11 LGT bytes of its first node. Read only; prints, writes nothing. */
#include "game/race/psx_light.h"
#include "game/race/psx_lgs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) free(data), data = NULL;
    fclose(f);
    *size = data != NULL ? (size_t)n : 0;
    return data;
}

static const PsxLgsContext *context_of(const PsxLgs *lgs, int node)
{
    const int block = node >> 2;
    for (int i = 0; i < lgs->context_count; i++)
        if ((lgs->contexts[i].mode == PSX_LGS_FIXED || lgs->contexts[i].mode == PSX_LGS_RAMP) && block >= lgs->contexts[i].seg_ini && block <= lgs->contexts[i].seg_fin)
            return &lgs->contexts[i];
    return NULL;
}

static float lowest(const float r[3]) { return r[0] < r[1] ? (r[0] < r[2] ? r[0] : r[2]) : (r[1] < r[2] ? r[1] : r[2]); }

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: psx_ratio_dump <psx_dir> <track> [A|B|C] [limit_percent]\n");
        return 2;
    }
    const char letter = argc > 3 ? (char)toupper((unsigned char)argv[3][0]) : 'B';
    const float limit = (argc > 4 ? (float)atoi(argv[4]) : 70.0f) / 100.0f;
    char track[16], path[512];
    size_t i = 0;
    for (; argv[2][i] != '\0' && i < sizeof track - 1; i++) track[i] = (char)toupper((unsigned char)argv[2][i]);
    track[i] = '\0';
    size_t size;
    snprintf(path, sizeof path, "%s/Z%s%c.LGT", argv[1], track, letter);
    uint8_t *lgt = slurp(path, &size);
    if (lgt == NULL) return fprintf(stderr, "cannot read %s\n", path), 1;
    const uint32_t nodes = (uint32_t)(lgt[0] | lgt[1] << 8 | lgt[2] << 16 | (uint32_t)lgt[3] << 24);
    PsxLight light;
    if (!psx_light_parse(lgt, size, nodes, &light)) return fprintf(stderr, "%s does not parse as %u nodes\n", path, nodes), 1;
    snprintf(path, sizeof path, "%s/Z%s%c.LGS", argv[1], track, letter);
    size_t lgs_size;
    uint8_t *text = slurp(path, &lgs_size);
    PsxLgs lgs;
    memset(&lgs, 0, sizeof lgs);
    const int have = text != NULL && psx_lgs_parse(text, lgs_size, &lgs);
    printf("%s %c: %u nodes, LGS %s, %d contexts\n", track, letter, nodes, have ? "read" : "MISSING", have ? lgs.context_count : 0);
    for (int c = 0; c < lgs.context_count; c++)
        printf("  context %d: blocks %d..%d mode %d c1 %d,%d,%d c2 %d,%d,%d\n", c, lgs.contexts[c].seg_ini, lgs.contexts[c].seg_fin, lgs.contexts[c].mode,
               lgs.contexts[c].c1[0], lgs.contexts[c].c1[1], lgs.contexts[c].c1[2], lgs.contexts[c].c2[0], lgs.contexts[c].c2[1], lgs.contexts[c].c2[2]);
    int run = -1, runs = 0;
    float run_low = 9;
    int byte_lo = 255, byte_hi = 0, ctx_nodes = 0;
    for (uint32_t n = 0; n <= nodes; n++) {
        float r[3] = { 1, 1, 1 };
        int dark = 0;
        const int b = n < nodes ? psx_light_byte(&light, n, 0) : 255;
        if (n < nodes) {
            psx_lgs_car_ratio(have ? &lgs : NULL, nodes, (int32_t)n, b, 16, r);
            dark = lowest(r) < limit;
        }
        if (dark) {
            if (run < 0) run = (int)n, run_low = 9, byte_lo = 255, byte_hi = 0, ctx_nodes = 0;
            if (lowest(r) < run_low) run_low = lowest(r);
            if (b < byte_lo) byte_lo = b;
            if (b > byte_hi) byte_hi = b;
            if (have && context_of(&lgs, (int)n) != NULL) ctx_nodes++;
        } else if (run >= 0) {
            printf("  dark run nodes %d..%d (%d): lowest ratio %.2f, LGT centre byte %d..%d, %d of them in a script context; first node's 11 bytes:", run, (int)n - 1,
                   (int)n - run, run_low, byte_lo, byte_hi, ctx_nodes);
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) printf(" %d", psx_light_byte(&light, (uint32_t)run, p));
            printf("\n");
            runs++;
            run = -1;
        }
    }
    printf("  %d dark runs under %.0f%%\n", runs, limit * 100.0f);
    psx_light_free(&light);
    free(lgt);
    free(text);
    return 0;
}
