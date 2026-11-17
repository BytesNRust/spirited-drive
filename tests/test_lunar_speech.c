/* dep59 (Lunar Springs' entry, L3): the two CD lines joined in memory with a pause (game/audio/lunar_speech.h): the header, the frames,
 * the data in order with the pause silent, the refusals; and on the CD's own files when original/ is there */
#include "game/audio/lunar_speech.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* an EACS sample of `frames` 16-bit mono frames at `rate`, each frame's two bytes `fill`, `extra` bytes past its data */
static size_t sample(uint8_t *out, uint32_t rate, uint32_t frames, uint8_t fill, size_t extra)
{
    memset(out, 0, EACS_HEADER_SIZE);
    memcpy(out, "EACS", 4);
    put32(out + 4, rate), out[8] = 2, out[9] = 1, out[0xa] = 0, out[0xb] = 0xff;
    put32(out + 0xc, frames), put32(out + 0x10, 0xffffffffu), put32(out + 0x18, EACS_HEADER_SIZE), put32(out + 0x1c, 0x227);
    memset(out + EACS_HEADER_SIZE, fill, frames * 2u + extra);
    return EACS_HEADER_SIZE + frames * 2u + extra;
}

static void test_join(void)
{
    static uint8_t a[EACS_HEADER_SIZE + 64], b[EACS_HEADER_SIZE + 64], out[4096];
    const size_t an = sample(a, 16000, 10, 0x11, 2), bn = sample(b, 16000, 20, 0x22, 0);   /* a's 2 bytes past its data dropped */
    const uint32_t gap = 16000u * LUNAR_SPEECH_GAP_MS / 1000u;   /* 5600 frames */
    const size_t need = lunar_speech_join(a, an, b, bn, LUNAR_SPEECH_GAP_MS, NULL, 0);
    CHECK_EQ_INT((int)need, (int)(EACS_HEADER_SIZE + (10 + gap + 20) * 2));
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 0, out, sizeof out), EACS_HEADER_SIZE + 60);   /* no pause */
    CHECK(memcmp(out, a, 0xc) == 0 && get32(out + 0xc) == 30 && memcmp(out + 0x10, a + 0x10, 0x10) == 0);   /* a's header, the frames summed */
    CHECK(out[EACS_HEADER_SIZE] == 0x11 && out[EACS_HEADER_SIZE + 19] == 0x11 && out[EACS_HEADER_SIZE + 20] == 0x22 && out[EACS_HEADER_SIZE + 59] == 0x22);
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 1, out, sizeof out), EACS_HEADER_SIZE + (10 + 16 + 20) * 2);   /* 1 ms: 16 frames of 0 */
    CHECK(out[EACS_HEADER_SIZE + 19] == 0x11 && out[EACS_HEADER_SIZE + 20] == 0 && out[EACS_HEADER_SIZE + 51] == 0 && out[EACS_HEADER_SIZE + 52] == 0x22);
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 1, out, 10), (int)(EACS_HEADER_SIZE + (10 + 16 + 20) * 2));   /* too small: the size, nothing written */
    /* 8-bit samples are unsigned: their silence is 0x80 */
    const size_t a8 = sample(a, 1000, 4, 0x11, 0), b8 = sample(b, 1000, 4, 0x22, 0);
    a[8] = b[8] = 1;
    CHECK_EQ_INT((int)lunar_speech_join(a, a8 - 4, b, b8 - 4, 2, out, sizeof out), EACS_HEADER_SIZE + 4 + 2 + 4);
    CHECK(out[EACS_HEADER_SIZE + 3] == 0x11 && out[EACS_HEADER_SIZE + 4] == 0x80 && out[EACS_HEADER_SIZE + 5] == 0x80 && out[EACS_HEADER_SIZE + 6] == 0x22);
}

static void test_refusals(void)
{
    static uint8_t a[EACS_HEADER_SIZE + 64], b[EACS_HEADER_SIZE + 64], out[256];
    const size_t an = sample(a, 16000, 10, 1, 0), bn = sample(b, 16000, 10, 2, 0);
    CHECK(lunar_speech_join(a, an, b, bn, 0, out, sizeof out) != 0);
    sample(b, 22050, 10, 2, 0);   /* another rate */
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 0, out, sizeof out), 0);
    sample(b, 16000, 10, 2, 0), b[9] = 2;   /* stereo */
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 0, out, sizeof out), 0);
    sample(b, 16000, 10, 2, 0), b[0xa] = 1;   /* packed */
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 0, out, sizeof out), 0);
    sample(b, 16000, 10, 2, 0), memcpy(b, "EACX", 4);
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 0, out, sizeof out), 0);
    sample(b, 16000, 10, 2, 0), put32(b + 0x18, 0x40);   /* its data elsewhere */
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn, 0, out, sizeof out), 0);
    sample(b, 16000, 10, 2, 0);   /* frames past the file: cut */
    CHECK_EQ_INT((int)lunar_speech_join(a, an, b, bn - 1, 0, out, sizeof out), 0);
    CHECK_EQ_INT((int)lunar_speech_join(a, EACS_HEADER_SIZE - 1, b, bn, 0, out, sizeof out), 0);
    CHECK_EQ_INT((int)lunar_speech_join(NULL, 0, b, bn, 0, out, sizeof out), 0);
}

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *d = malloc(1u << 20);
    *size = d ? fread(d, 1, 1u << 20, f) : 0;
    fclose(f);
    return d;
}

/* the CD's: WEL 35109 frames, GOL 48298, at 16 kHz: about 5.6 s with the pause */
static void test_the_cd(void)
{
    static const char *const DIRS[] = { "speech", "gspeech" };
    for (int i = 0; i < 2; i++) {
        char wp[512], gp[512];
        snprintf(wp, sizeof wp, "%s/original/FRONTEND/%s/LUNARWEL.EAS", TNFS_SOURCE_DIR, DIRS[i]);
        snprintf(gp, sizeof gp, "%s/original/FRONTEND/%s/LUNARGOL.EAS", TNFS_SOURCE_DIR, DIRS[i]);
        size_t wn = 0, gn = 0;
        uint8_t *w = read_all(wp, &wn), *g = read_all(gp, &gn);
        if (w == NULL || g == NULL) {
            printf("skip the CD's %s (no original/)\n", DIRS[i]);
        } else {
            const size_t n = lunar_speech_join(w, wn, g, gn, LUNAR_SPEECH_GAP_MS, NULL, 0);
            uint8_t *out = malloc(n);
            CHECK(n > 0 && out != NULL && lunar_speech_join(w, wn, g, gn, LUNAR_SPEECH_GAP_MS, out, n) == n);
            const uint32_t frames = out ? get32(out + 0xc) : 0;
            CHECK_EQ_INT((int)frames, (int)(get32(w + 0xc) + 5600 + get32(g + 0xc)));
            const double seconds = frames / 16000.0;
            CHECK(seconds > 5.0 && seconds < 7.5);
            printf("the CD's %s: %u frames, %.2f s\n", DIRS[i], frames, seconds);
            free(out);
        }
        free(w), free(g);
    }
}

int main(void)
{
    test_join();
    test_refusals();
    test_the_cd();
    return TEST_REPORT();
}
