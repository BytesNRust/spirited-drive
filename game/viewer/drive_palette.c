#include "viewer/drive_palette.h"

#include "game/race/race_palette.h"
#include "game/sys/shape_bank.h"

#include <string.h>

enum {
    PALETTE_BYTES = 0x300,
    PALETTE_8_BITS = 0x24,      /* a '!pal' of 8-bit channels; 0x22 is 6-bit */
    SHAPE_PIXELS = 0x10,
    TRACK_FIRST = 0x90,         /* 0044c36c: the track's first 0x60 colours */
    TRACK_COUNT = 0x60
};

static const uint8_t TRACK_PAL[4] = { '!', 'p', 'a', 'l' };
static const uint8_t CAR_PAL[4] = { '!', 'P', 'A', 'L' };
static const uint8_t CAR_OWN[4] = { '!', 'x', 'x', 'x' };

/* The buffers are the caller's, read from files: writable. */
static uint8_t *bank_of(const Chunk *c)
{
    return c->kind == CHUNK_KIND_BITMAP ? (uint8_t *)(uintptr_t)c->data : NULL;
}

/* A bank's palette as 6 bits a channel: 0044c36c turns a type 0x24 '!pal'
 * into one in place (>> 2). TODO(00457d40): that the car loader does the
 * same for a CFM's '!PAL' is assumed. */
static const uint8_t *palette6(uint8_t *bank, const uint8_t name[4])
{
    uint8_t *shape = bank != NULL ? shape_bank_find(bank, name) : NULL;
    if (shape == NULL) return NULL;
    if (shape[0] == PALETTE_8_BITS) {
        for (int i = 0; i < PALETTE_BYTES; i++) shape[SHAPE_PIXELS + i] >>= 2;
        shape[0] = 0x22;
    }
    return shape + SHAPE_PIXELS;
}

/* The banks of a wwww's children (or the chunk itself when it is one). */
static void remap_track_banks(RacePalette *p, const Chunk *c, const uint8_t *palette)
{
    if (c->kind == CHUNK_KIND_BITMAP) {
        race_palette_remap_track_bank(p, bank_of(c), palette);
        return;
    }
    Chunk child;
    for (uint32_t i = 0; c->kind == CHUNK_KIND_HEADER && i < c->count; i++)
        if (chunk_child(c, i, &child) == CHUNK_OK && child.kind == CHUNK_KIND_BITMAP)
            race_palette_remap_track_bank(p, bank_of(&child), palette);
}

/* TODO: which call maps the scenery models' banks (FAM child 3, each with
 * its own '!pal') is not read; here each with its own. */
static void remap_model_banks(RacePalette *p, const Chunk *models)
{
    Chunk pair, bank;
    for (uint32_t i = 0; models->kind == CHUNK_KIND_HEADER && i < models->count; i++) {
        if (chunk_child(models, i, &pair) != CHUNK_OK || pair.kind != CHUNK_KIND_HEADER) continue;
        for (uint32_t k = 0; k < pair.count; k++) {
            if (chunk_child(&pair, k, &bank) != CHUNK_OK || bank.kind != CHUNK_KIND_BITMAP) continue;
            const uint8_t *own = palette6(bank_of(&bank), TRACK_PAL);
            if (own != NULL) race_palette_remap_track_bank(p, bank_of(&bank), own);
        }
    }
}

/* 004574ec's car fields from each CFM: the high and low detail banks with
 * their '!PAL'. TODO(00490cc0's use): no mesh, the flat sink matches rgb. */
static void remap_cars(RacePalette *p, const RaceState *race, const Chunk cfm[RACE_CARS])
{
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    for (int i = 0; i < RACE_CARS; i++) {
        Chunk high, low;
        if (cfm[i].kind != CHUNK_KIND_HEADER || chunk_child(&cfm[i], CFM_CHILD_BITMAPS_HIGH, &high) != CHUNK_OK ||
            chunk_child(&cfm[i], CFM_CHILD_BITMAPS_LOW, &low) != CHUNK_OK)
            continue;
        art[i].bank = bank_of(&high);
        art[i].palette = palette6(art[i].bank, CAR_PAL);
        art[i].bank2 = bank_of(&low);
        art[i].palette2 = palette6(art[i].bank2, CAR_PAL);
        if (art[i].palette == NULL) art[i].bank = NULL;
        if (art[i].palette2 == NULL) art[i].bank2 = NULL;
    }
    race_palette_remap_cars(p, race->car_count, art, race->unknown_4fbab8);
}

int drive_palette_build(const RaceState *race, const Chunk *fam, const Chunk cfm[RACE_CARS], uint8_t *carpal,
                        uint8_t rgb[768])
{
    static RacePalette p;
    Chunk terrain, scenery, horizon, models, first;
    if (chunk_child(fam, FAM_CHILD_TERRAIN, &terrain) != CHUNK_OK || terrain.kind != CHUNK_KIND_HEADER ||
        chunk_child(&terrain, 0, &first) != CHUNK_OK)
        return 0;
    const uint8_t *track = palette6(bank_of(&first), TRACK_PAL);
    if (track == NULL) return 0;
    memset(&p, 0, sizeof p);
    p.tint = (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    p.tint_ready = 1;
    race_palette_queue(&p, TRACK_FIRST, TRACK_COUNT, track);
    /* own[i]: slot i's '!xxx' (car +0x526), in its high detail bank. */
    const uint8_t *own[RACE_CARS] = { 0 };
    for (int i = 0; i < RACE_CARS; i++) {
        Chunk high;
        uint8_t *bank = cfm[i].kind == CHUNK_KIND_HEADER && chunk_child(&cfm[i], CFM_CHILD_BITMAPS_HIGH, &high) == CHUNK_OK
                            ? bank_of(&high) : NULL;
        const uint8_t *xxx = bank != NULL ? shape_bank_find(bank, CAR_OWN) : NULL;
        own[i] = xxx != NULL ? xxx + SHAPE_PIXELS : NULL;
    }
    race_palette_cars(&p, race, own, carpal);
    race_palette_apply(&p);
    race_palette_enable(&p, race);
    remap_cars(&p, race, cfm);
    remap_track_banks(&p, &terrain, track);
    if (chunk_child(fam, FAM_CHILD_SCENERY, &scenery) == CHUNK_OK) remap_track_banks(&p, &scenery, track);
    if (chunk_child(fam, FAM_CHILD_HORIZON, &horizon) == CHUNK_OK) {
        const uint8_t *sky = palette6(bank_of(&horizon), TRACK_PAL);
        remap_track_banks(&p, &horizon, sky != NULL ? sky : track);
    }
    if (chunk_child(fam, FAM_CHILD_OBJECTS, &models) == CHUNK_OK) remap_model_banks(&p, &models);
    for (int i = 0; i < 256; i++) {
        rgb[3 * i] = p.screen_rgbx[4 * i];
        rgb[3 * i + 1] = p.screen_rgbx[4 * i + 1];
        rgb[3 * i + 2] = p.screen_rgbx[4 * i + 2];
    }
    return 1;
}
