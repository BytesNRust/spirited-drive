#include "card_photo.h"

#include "formats/png.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CARD_PATH 4200

static int fail(char *why, size_t cap, const char *what, const char *detail)
{
    if (why != NULL && cap > 0) snprintf(why, cap, detail != NULL ? "%s: %s" : "%s", what, detail);
    return 0;
}

int card_photo_render(ModelScene *scene, const Orbit *o, const CarNightState *night, const uint8_t *backdrop, uint8_t *rgba)
{
    if (!model_scene_render(scene, o, CARD_PHOTO_W, CARD_PHOTO_H, night, rgba)) return 0;
    for (size_t i = 0; i < (size_t)CARD_PHOTO_W * CARD_PHOTO_H; ++i) {   /* the car where it is drawn (alpha 0 is the clear background), else the backdrop; the card is opaque */
        uint8_t *px = rgba + 4 * i;
        if (px[3] == 0) memcpy(px, backdrop + 4 * i, 3);
        px[3] = 255;
    }
    return 1;
}

int card_photo_exists(const char *piece)
{
    char path[CARD_PATH];
    struct stat st;
    return piece != NULL && snprintf(path, sizeof path, "%s/card.png", piece) < (int)sizeof path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int card_photo_write(ModelScene *scene, const Orbit *o, const CarNightState *night, const uint8_t *backdrop, const char *piece, char *why, size_t why_cap)
{
    if (scene == NULL || o == NULL || backdrop == NULL || piece == NULL) return fail(why, why_cap, "there is no car picture to use", NULL);
    char path[CARD_PATH], tmp[CARD_PATH + 8];
    if (snprintf(path, sizeof path, "%s/card.png", piece) >= (int)sizeof path || snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
        return fail(why, why_cap, "the path is too long", piece);
    const size_t pixels = (size_t)CARD_PHOTO_W * CARD_PHOTO_H;
    uint8_t *rgba = malloc(pixels * 4);
    if (rgba == NULL) return fail(why, why_cap, "out of memory", NULL);
    if (!card_photo_render(scene, o, night, backdrop, rgba)) {
        free(rgba);
        return fail(why, why_cap, "the car cannot be drawn at the card's size", NULL);
    }
    uint8_t *png = NULL;
    size_t len = 0;
    const PngResult r = png_write_rgba(rgba, CARD_PHOTO_W, CARD_PHOTO_H, &png, &len);
    free(rgba);
    if (r != PNG_OK) return fail(why, why_cap, "cannot make the picture", png_result_str(r));
    FILE *f = fopen(tmp, "wb");
    const int wrote = f != NULL && fwrite(png, 1, len, f) == len;
    const int closed = f == NULL || fclose(f) == 0;
    png_free(png);
    if (!wrote || !closed || rename(tmp, path) != 0) {
        const int err = errno;
        (void)unlink(tmp);
        return fail(why, why_cap, "cannot write card.png", strerror(err));
    }
    return 1;
}
