/* The TGV frames (tgv_frame.h) where the port parts from the original: the frames it refuses (short, over 256
 * colours, a picture that does not unpack, a move out of the last picture) decode nothing. The decoding itself is
 * oracle_movie's (movie). */
#include "game/frontend/tgv_frame.h"
#include "test.h"

#include <stdint.h>
#include <string.h>

enum { W = 8, H = 8, PIXELS = W * H };

static size_t header(uint8_t *f, const char *tag, int a, int b, int c, int d, uint32_t flags)
{
    memset(f, 0, 0x14);
    memcpy(f, tag, 4);
    const int v[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) f[8 + 2 * i] = (uint8_t)v[i], f[9 + 2 * i] = (uint8_t)(v[i] >> 8);
    for (int i = 0; i < 4; i++) f[0x10 + i] = (uint8_t)(flags >> (8 * i));
    return 0x14;
}

/* A raw 8 x 8 key frame of two colours, pixel i = i */
static size_t key_frame(uint8_t *f)
{
    size_t n = header(f, "kVGT", W, H, 0, 2, 1);
    for (int i = 0; i < 6; i++) f[n++] = (uint8_t)(0x10 + i);
    for (int i = 0; i < PIXELS; i++) f[n++] = (uint8_t)i;
    return n;
}

/* A frame of one move (dx, dy) and every block moved by it: 4 blocks, codes of 1 bit */
static size_t move_frame(uint8_t *f, int dx, int dy)
{
    size_t n = header(f, "fVGT", 1, 0, 0, 1, 0);
    const uint32_t fields = ((uint32_t)dx & 0x3ff) | ((uint32_t)dy & 0x3ff) << 10;
    for (int i = 0; i < 4; i++) f[n++] = (uint8_t)(fields >> (8 * i));
    for (int i = 0; i < 4 + 4; i++) f[n++] = 0;  /* the codes' dword, all 0, and the 4 bytes read past them */
    return n;
}

static void key_frames(void)
{
    static uint8_t f[0x200];
    TgvState *s = tgv_open();
    CHECK(s != NULL);
    if (s == NULL) return;
    const size_t n = key_frame(f);
    uint8_t *shape = tgv_decode(s, f, n);
    CHECK(shape != NULL && shape == s->shape);
    if (shape == NULL) {
        tgv_close(s);
        return;
    }
    CHECK(shape[0] == 0x7b && shape[4] == W && shape[6] == H && s->colors == 2 && s->palette[5] == 0x15);
    CHECK(shape[0x10] == 0 && shape[0x10 + PIXELS - 1] == PIXELS - 1);
    CHECK(s->rows[0] == -H * W && s->rows[2 * H - 1] == (H - 1) * W);

    CHECK(tgv_key_frame(s, f, 0x13) == 0);                   /* shorter than its header */
    CHECK(tgv_key_frame(s, f, 0x14 + 5) == 0);               /* its palette cut */
    f[0xe] = 1, f[0xf] = 1;                                  /* 257 colours */
    CHECK(tgv_key_frame(s, f, sizeof f) == 0);
    f[0xe] = 2, f[0xf] = 0, f[0x10] = 0;                     /* packed, but not QFS */
    CHECK(tgv_key_frame(s, f, n) == 0);
    CHECK(s->width == W && s->colors == 2 && shape[0x10 + 9] == 9);
    tgv_close(s);
}

static void frames(void)
{
    static uint8_t f[0x200], m[0x40];
    TgvState *s = tgv_open();
    if (s == NULL) return;
    tgv_decode(s, f, key_frame(f));
    uint8_t *const key = s->shape;
    size_t n = move_frame(m, -1, 0);                        /* every block one pixel left of the key's */
    CHECK(tgv_decode(s, m, n) != key && s->shape != NULL);
    CHECK(s->moves == 1 && s->work[0] == -1);
    CHECK(s->shape[0x10] == 0 && s->shape[0x11] == 0 && s->shape[0x12] == 1 && s->shape[0x10 + 8] == 7);
    CHECK(tgv_frame(s, m, n - 5) == 0);                      /* the codes cut (a byte short of their dword) */
    n = move_frame(m, 1, 0);                                 /* the last block's last line a byte past the picture */
    CHECK(tgv_frame(s, m, n) == 0);
    n = move_frame(m, 0, -3);                                /* three lines up: 8 bytes before the header */
    CHECK(tgv_frame(s, m, n) == 0);
    n = move_frame(m, 0, -9);                                /* above the row offsets */
    CHECK(tgv_frame(s, m, n) == 0);
    n = move_frame(m, 0, -2);                                /* two lines up: the header, still inside */
    CHECK(tgv_frame(s, m, n) == 1 && memcmp(s->shape + 0x10, s->previous, 4) == 0);

    uint8_t *const last = s->shape;
    memcpy(m, "junk", 4);
    CHECK(tgv_decode(s, m, n) == last);                      /* neither tag: the last picture */
    CHECK(tgv_decode(s, m, 2) == last);
    tgv_close(s);
    tgv_close(NULL);
}

int main(void)
{
    key_frames();
    frames();
    return TEST_REPORT();
}
