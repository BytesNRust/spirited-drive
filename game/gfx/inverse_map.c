/* The inverse colour map (00473d80 with 00473f98, 0047411c, 004743c0):
 * Spencer Thomas's incremental inv_cmap (Graphics Gems II), without its
 * min / max tracking. It is not an exact nearest search: once a row's
 * cells stop improving the scan stops, so a far cell can keep a colour a
 * later entry would beat. The globals 0052bb10.. are the fields below. */
#include "game/gfx/inverse_map.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t colormax, x, xsqr, rstride, gstride;  /* 0052bb1c, 0052bb18, 0052bb20, 0052bb28, 0052bb24 */
    uint8_t cindex;                               /* 0052bb10 */
    int32_t rcenter, gcenter, bcenter;            /* 0052bb50, 0052bb40, 0052bb44 */
    uint32_t cdist, crinc, cginc, cbinc;          /* 0052bb4c, 0052bb2c, 0052bb34, 0052bb30 */
    uint32_t *cdp;                                /* 0052bb3c */
    uint8_t *crgbp;                               /* 0052bb14 */
    /* the red loop's (00473f98) */
    uint32_t rdist, rinc;                         /* 0052bb54, 0052bb64 */
    uint32_t *rdp;                                /* 0052bb38 */
    uint8_t *rrgbp;                               /* 0052bb58 */
    /* the green loop's (0047411c) */
    int32_t ghere, gmin, gmax;                    /* 0052bb84, 0052bb80, 0052bb7c */
    uint32_t ginc, gdist, gxx, lgdist;            /* 0052bb78, 0052bb60, 0052bb74, 0052bb70 */
    uint32_t *gdp, *lgdp;                         /* 0052bb48, 0052bb6c */
    uint8_t *grgbp, *lgrgbp;                      /* 0052bb5c, 0052bb68 */
    /* the blue loop's (004743c0) */
    int32_t bhere, bmin, bmax;                    /* 0052bb94, 0052bb90, 0052bb8c */
    uint32_t binc;                                /* 0052bb88 */
} InvMap;

/* 004743c0 */
static int blue_loop(InvMap *m, int restart)
{
    if (restart) {
        m->bhere = m->bcenter;
        m->bmax = m->colormax - 1;
        m->bmin = 0;
        m->binc = m->cbinc;
    }
    const uint32_t txsqr = (uint32_t)m->xsqr * 2;
    const int32_t max = m->bmax, min = m->bmin;
    int detect = 0;
    uint32_t bxx = m->binc, bdist = m->gdist, *dp = m->gdp;
    uint8_t *rgbp = m->grgbp;
    int32_t b;
    for (b = m->bhere; b <= m->bmax; b++) {
        if (bdist < *dp) {
            if (b > m->bhere) {
                m->gdp = dp, m->grgbp = rgbp, m->gdist = bdist, m->binc = bxx;
                m->bhere = b;
            }
            detect = 1;
            break;
        }
        dp++, rgbp++;
        bdist += bxx;
        bxx += txsqr;
    }
    for (; b <= max && bdist < *dp; b++, dp++, rgbp++, bdist += bxx, bxx += txsqr) {
        *dp = bdist;
        *rgbp = m->cindex;
    }
    b = m->bhere - 1;
    rgbp = m->grgbp - 1;
    bxx = m->binc - txsqr;
    bdist = m->gdist - bxx;
    dp = m->gdp - 1;
    if (!detect) {
        for (; m->bmin <= b; b--) {
            if (bdist < *dp) {
                detect = 1;
                m->gdp = dp, m->grgbp = rgbp, m->gdist = bdist, m->binc = bxx;
                m->bhere = b;
                break;
            }
            dp--, rgbp--;
            bxx -= txsqr;
            bdist -= bxx;
        }
    }
    for (; min <= b && bdist < *dp; b--, dp--, rgbp--, bxx -= txsqr, bdist -= bxx) {
        *dp = bdist;
        *rgbp = m->cindex;
    }
    return detect;
}

/* 0047411c */
static int green_loop(InvMap *m, int restart)
{
    if (restart) {
        m->ghere = m->gcenter;
        m->gmax = m->colormax - 1;
        m->gmin = 0;
        m->ginc = m->cginc;
    }
    const uint32_t txsqr = (uint32_t)m->xsqr * 2;
    m->gdist = m->lgdist = m->rdist;
    m->gdp = m->lgdp = m->rdp;
    m->grgbp = m->lgrgbp = m->rrgbp;
    int detect = 0, first = 1;
    m->gxx = m->ginc;
    for (int32_t g = m->ghere; g <= m->gmax; g++) {
        if (blue_loop(m, first)) {
            if (!detect) {
                if (g > m->ghere) {
                    m->rdp = m->lgdp, m->ginc = m->gxx, m->rrgbp = m->lgrgbp, m->rdist = m->lgdist;
                    m->ghere = g;
                }
                detect = 1;
            }
        } else if (detect) {
            break;
        }
        m->gdp += m->gstride, m->lgdp += m->gstride;
        m->grgbp += m->gstride, m->lgrgbp += m->gstride;
        first = 0;
        m->gdist += m->gxx, m->lgdist += m->gxx;
        m->gxx += txsqr;
    }
    m->gxx = m->ginc - txsqr;
    m->gdist = m->rdist - m->gxx;
    m->gdp = m->rdp - m->gstride;
    m->grgbp = m->rrgbp - m->gstride;
    m->lgrgbp = m->grgbp, m->lgdp = m->gdp, m->lgdist = m->gdist;
    first = 1;
    for (int32_t g = m->ghere - 1; m->gmin <= g; g--) {
        if (blue_loop(m, first)) {
            if (!detect) {
                detect = 1;
                m->rdp = m->lgdp, m->rrgbp = m->lgrgbp, m->ginc = m->gxx, m->rdist = m->lgdist;
                m->ghere = g;
            }
        } else if (detect) {
            return detect;
        }
        m->gdp -= m->gstride, m->lgdp -= m->gstride;
        m->lgrgbp -= m->gstride, m->grgbp -= m->gstride;
        m->gxx -= txsqr;
        m->gdist -= m->gxx, m->lgdist -= m->gxx;
        first = 0;
    }
    return detect;
}

/* 00473f98 */
static int red_loop(InvMap *m)
{
    const uint32_t txsqr = (uint32_t)m->xsqr * 2;
    int detect = 0, first = 1;
    m->rdist = m->cdist;
    m->rdp = m->cdp;
    m->rrgbp = m->crgbp;
    m->rinc = m->crinc;
    for (int32_t r = m->rcenter; r < m->colormax; r++) {
        if (green_loop(m, first)) detect = 1;
        else if (detect) break;
        m->rdp += m->rstride;
        m->rrgbp += m->rstride;
        m->rdist += m->rinc;
        first = 0;
        m->rinc += txsqr;
    }
    m->rinc = m->crinc - txsqr;
    m->rdist = m->cdist - m->rinc;
    m->rdp = m->cdp - m->rstride;
    m->rrgbp = m->crgbp - m->rstride;
    first = 1;
    for (int32_t r = m->rcenter - 1; r >= 0; r--) {
        if (green_loop(m, first)) detect = 1;
        else if (detect) return detect;
        m->rdp -= m->rstride;
        m->rrgbp -= m->rstride;
        m->rinc -= txsqr;
        m->rdist -= m->rinc;
        first = 0;
    }
    return detect;
}

/* 00473d80(colors, colormap, bits, dist_buf, rgbmap) with bits 5 */
static void build(InvMap *m, const uint8_t *entries, const uint8_t *enabled, uint32_t *dist, uint8_t *map)
{
    const int nbits = 8 - INVERSE_MAP_BITS;
    m->colormax = 1 << INVERSE_MAP_BITS;
    m->x = 1 << nbits;
    m->xsqr = 1 << (2 * nbits);
    m->rstride = m->colormax * m->colormax;
    m->gstride = m->colormax;
    for (int32_t k = 0; k < m->colormax * m->colormax * m->colormax; k++) dist[k] = 0xffffffffu;
    for (int i = 0; i < INVERSE_MAP_COLOURS; i++) {
        if (enabled[i] == 0) continue;
        const uint8_t *e = entries + 4 * i;
        const int32_t red = e[2], green = e[1], blue = e[0];
        m->cindex = (uint8_t)i;
        m->rcenter = red >> nbits;
        m->gcenter = green >> nbits;
        m->bcenter = blue >> nbits;
        const int32_t half = m->x / 2;
        const uint32_t rd = (uint32_t)(red - (m->rcenter * m->x + half));
        const uint32_t gd = (uint32_t)(green - (m->gcenter * m->x + half));
        const uint32_t bd = (uint32_t)(blue - (half + m->bcenter * m->x));
        m->cdist = rd * rd + gd * gd + bd * bd;
        m->crinc = (uint32_t)((m->rcenter + 1) * m->xsqr - red * m->x) * 2;
        m->cginc = (uint32_t)((m->gcenter + 1) * m->xsqr - green * m->x) * 2;
        m->cbinc = (uint32_t)((m->bcenter + 1) * m->xsqr - blue * m->x) * 2;
        const int32_t at = m->rcenter * m->rstride + m->gcenter * m->gstride + m->bcenter;
        m->cdp = dist + at;
        m->crgbp = map + at;
        red_loop(m);
    }
}

int inverse_map_build(const uint8_t entries[INVERSE_MAP_COLOURS * 4], const uint8_t enabled[INVERSE_MAP_COLOURS],
                      uint8_t map[INVERSE_MAP_SIZE])
{
    uint32_t *dist = malloc(INVERSE_MAP_SIZE * sizeof *dist);
    if (dist == NULL) return 0;
    InvMap m;
    memset(&m, 0, sizeof m);
    build(&m, entries, enabled, dist, map);
    free(dist);
    return 1;
}
