/* The viewer's sink for the 3D engine: a flat polygon's colour. The mesh
 * stores it as 0x00BBGGRR-packed bytes read against 0052f178, whose entries
 * are B, G, R (00476460), so 0x00c81414 (the 512TR's "red") is red. */
#include "viewer/object_sink.h"
#include "test.h"

#include <string.h>

enum { W = 16, H = 16 };

static uint8_t flat_colour(uint32_t rgb)
{
    static uint8_t pixels[W * H];
    uint8_t palette[768] = { 0 };
    palette[3 * 1 + 0] = 200, palette[3 * 1 + 1] = 20, palette[3 * 1 + 2] = 20;  /* red */
    palette[3 * 2 + 0] = 20, palette[3 * 2 + 1] = 20, palette[3 * 2 + 2] = 200;  /* blue */
    memset(pixels, 0x7f, sizeof pixels);
    const QuadTarget target = { pixels, W, 0, 0, W, H };
    uint8_t shade[256] = { 0 };
    ObjectSink s = { &target, palette, shade, NULL };
    const Object3DSink sink = object_sink(&s);
    const Object3DCorners c = { { 2, 2, 12, 2, 12, 12, 2, 12 }, { 0 }, { 0 }, 4 };
    sink.flat(sink.user, rgb, &c);
    return pixels[6 * W + 6];
}

int main(void)
{
    CHECK_EQ_INT(flat_colour(0x00c81414u), 1);  /* R 0xc8 in the third byte */
    CHECK_EQ_INT(flat_colour(0x001414c8u), 2);
    return TEST_REPORT();
}
