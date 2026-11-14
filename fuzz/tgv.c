/* The movies' frames (.tgv, the CD's or a mod's): each 'kVGT' key frame and 'fVGT' frame found in the input decoded in turn, as the
 * player hands them over */
#include "fuzz/fuzz.h"
#include "game/frontend/tgv_frame.h"

#include <string.h>

enum { FRAMES = 32 };

static int tag_at(const uint8_t *p, size_t left)
{
    return left >= 4 && (!memcmp(p, "kVGT", 4) || !memcmp(p, "fVGT", 4) || !memcmp(p, "TGVk", 4) || !memcmp(p, "TGVf", 4));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    TgvState *s = tgv_open();
    if (s == NULL) return 0;
    size_t at = 0;
    while (at < size && !tag_at(data + at, size - at)) at++;
    if (at == size) at = 0;   /* no tag: the whole input as one frame */
    for (int n = 0; n < FRAMES && at < size; n++) {
        size_t end = at + 4;
        while (end < size && !tag_at(data + end, size - end)) end++;
        if (end > size) end = size;
        (void)tgv_decode(s, data + at, end - at);
        at = end;
    }
    tgv_close(s);
    return 0;
}
