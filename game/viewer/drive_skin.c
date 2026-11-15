/* port (dep59 step 3): an added car's skin and colour in the race (drive_skin.h) */
#include "game/viewer/drive_skin.h"

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/png.h"
#include "game/platform/platform.h"
#include "game/race/car_registry.h"
#include "game/sys/data_path.h"
#include "game/sys/palette.h"
#include "game/sys/port_log.h"
#include "game/sys/shape_bank.h"

#include <stdio.h>
#include <string.h>

enum { LEVELS = 2, SHAPE_DATA = 0x10, PALETTE_6BIT = 0x22, PALETTE_8BIT = 0x24, LAST_PLAIN = 0xf9 };

static const uint8_t PAL_NAME[4] = { '!', 'P', 'A', 'L' };

/* 1 when the entry's name can name a file: four letters or digits */
static int name_plain(const char name[4])
{
    for (int i = 0; i < 4; i++) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

/* the bank's palette at 8 bits a channel; 0 for a type that is neither */
static int palette_rgb(const uint8_t *shape, uint8_t out[768])
{
    if (shape[0] != PALETTE_8BIT && shape[0] != PALETTE_6BIT) return 0;
    for (int i = 0; i < 768; i++) out[i] = shape[0] == PALETTE_8BIT ? shape[SHAPE_DATA + i] : (uint8_t)(shape[SHAPE_DATA + i] << 2);
    return 1;
}

/* the texture `e` from `path` when the PNG is of its size: 1 when it was put */
static int put_texture(const char *path, const FshEntry *e, const uint8_t rgb[768], const uint8_t usable[256])
{
    if (!data_path_exists(path)) return 0;   /* a skin need not bring every texture */
    size_t size = 0;
    uint8_t *data = platform_read_file(path, &size);
    uint32_t w = 0, h = 0;
    const int sized = data != NULL && png_size(data, size, &w, &h) == PNG_OK;
    const int fits = sized && w == e->width && h == e->height && (size_t)w * h <= e->payload_size;   /* checked before the pixels are */
    PngImage img;
    const int read = fits && png_read(data, size, &img) == PNG_OK;
    if (data != NULL) platform_free_file(data);
    if (!fits) return port_log("car skin: %s is not %ux%u: passed over\n", path, (unsigned)e->width, (unsigned)e->height), 0;
    if (!read) return port_log("car skin: %s does not read as a PNG\n", path), 0;
    if (img.width != w || img.height != h) return port_log("car skin: %s does not read as a PNG\n", path), png_free(img.pixels), 0;   /* a second header */
    uint8_t *to = (uint8_t *)(uintptr_t)e->payload;
    for (size_t i = 0; i < (size_t)w * h; i++)
        to[i] = img.indexed ? img.pixels[i]
                            : (uint8_t)palette_nearest(rgb, usable, img.pixels[4 * i], img.pixels[4 * i + 1], img.pixels[4 * i + 2]);
    png_free(img.pixels);
    return 1;
}

int drive_skin_apply(const char *skin_dir, uint8_t *cfm, size_t size)
{
    Chunk file;
    if (skin_dir == NULL || skin_dir[0] == '\0' || chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    uint8_t usable[256];
    for (int i = 0; i < 256; i++) usable[i] = i <= LAST_PLAIN;
    int put = 0;
    for (int level = 0; level < LEVELS; level++) {
        Chunk shpi;
        Fsh bank;
        if (chunk_child(&file, (uint32_t)(2 * level + 1), &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) continue;
        if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK || !shape_bank_valid(shpi.data, shpi.size)) continue;
        const uint8_t *pal = shape_bank_find((uint8_t *)(uintptr_t)bank.data, PAL_NAME);
        uint8_t rgb[768];
        if (pal == NULL || !palette_rgb(pal, rgb)) continue;
        for (uint32_t i = 0; i < bank.count; i++) {
            FshEntry e;
            if (fsh_entry(&bank, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP || !name_plain(e.name)) continue;
            char path[DRIVE_SKIN_DIR_MAX + 16];
            snprintf(path, sizeof path, "%s/%.4s%s.png", skin_dir, e.name, level != 0 ? "_lo" : "");
            put += put_texture(path, &e, rgb, usable);
        }
    }
    if (put > 0) port_log("car skin: %s, %d textures\n", skin_dir, put);
    return put;
}

void drive_skin_choose(const CarPieceCard *card, const CarLook *look, DriveSkinLook *out)
{
    memset(out, 0, sizeof *out);
    int skin = 0;
    if (look != NULL && look->skin[0] != '\0') {
        for (int k = 1; k < card->skins && k <= CAR_PIECE_SKINS_MAX; k++)
            if (strcmp(card->skin[k], look->skin) == 0) skin = k;
        if (skin == 0) port_log("car skin: %s has no skin %s now: the skin as it comes\n", card->info.id, look->skin);
    }
    if (skin > 0) {
        snprintf(out->skin_dir, sizeof out->skin_dir, "%s/skins/%s", card->dir, card->skin[skin]);
        snprintf(out->mask_dir, sizeof out->mask_dir, "%s", out->skin_dir);
    } else {
        snprintf(out->mask_dir, sizeof out->mask_dir, "%s/car_masks/%s", card->dir, card->info.stem);
    }
    if (look == NULL || look->paint[0] == '\0') return;
    int paint = -1;
    for (int k = 0; k < card->info.paint_count; k++)
        if (strcmp(card->info.paints[k].name, look->paint) == 0) paint = k;
    if (paint < 0) {
        port_log("car skin: %s has no paint %s now: its own colour\n", card->info.id, look->paint);
        return;
    }
    if (!car_pieces_colour_on(card, skin)) return;   /* the colour kept for a skin with masks */
    out->paint_on = 1;
    memcpy(out->rgb, card->info.paints[paint].rgb, 3);
}

int drive_skin_look(int car, int player, DriveSkinLook *out)
{
    memset(out, 0, sizeof *out);
    CarPieceCard card;
    if (!car_registry_is_added(car) || !car_pieces_card(car, &card)) return 0;
    const CarLook look = car_looks_get(card.info.id);
    drive_skin_choose(&card, player ? &look : NULL, out);
    return 1;
}
