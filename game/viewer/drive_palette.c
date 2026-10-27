#include "viewer/drive_palette.h"

#include "game/race/race_palette.h"
#include "game/race/track_palette_repair.h"
#include "game/sys/palette.h"
#include "game/sys/shape_bank.h"

#include "game/race/car_colours.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include <string.h>

enum {
    PALETTE_8_BITS = 0x24,      /* a '!pal' of 8-bit channels; 0x22 is 6-bit */
    SHAPE_PIXELS = 0x10,
    TRACK_FIRST = 0x90,         /* 0044c36c: the track's first 0x60 colours */
    TRACK_COUNT = 0x60
};

static const uint8_t TRACK_PAL[4] = { '!', 'p', 'a', 'l' };
static const uint8_t MODEL_PAL[4] = { '!', 'P', 'A', 'L' };

/* The buffers are the caller's, read from files: writable. */
static uint8_t *bank_of(const Chunk *c)
{
    return c->kind == CHUNK_KIND_BITMAP ? (uint8_t *)(uintptr_t)c->data : NULL;
}

/* A track bank's palette as 6 bits a channel: 0044c36c turns a type 0x24
 * '!pal' into one in place (>> 2). The cars' come from their loader
 * (00457d40), which does the same for each CFM's '!PAL' and '!xxx'. */
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
/* (with `used` set nothing is remapped: the entries the banks, as mapped, show are marked) */
static void map_track_bank(RacePalette *p, uint8_t *bank, const uint8_t *palette, uint8_t *used)
{
    if (used != NULL) race_palette_bank_entries(bank, used);
    else race_palette_remap_track_bank(p, bank, palette);
}

static void remap_track_banks(RacePalette *p, const Chunk *c, const uint8_t *palette, uint8_t *used)
{
    if (c->kind == CHUNK_KIND_BITMAP) {
        map_track_bank(p, bank_of(c), palette, used);
        return;
    }
    Chunk child;
    for (uint32_t i = 0; c->kind == CHUNK_KIND_HEADER && i < c->count; i++)
        if (chunk_child(c, i, &child) == CHUNK_OK && child.kind == CHUNK_KIND_BITMAP)
            map_track_bank(p, bank_of(&child), palette, used);
}

/* 004576f0 (from 004585e4): each scenery model's bank (FAM child 3) queued
 * with its own '!PAL' (004b1160), upper case: the midday FAMs' model banks
 * have only that one, the others a '!pal' too. */
static void remap_model_banks(RacePalette *p, const Chunk *models, uint8_t *used)
{
    Chunk pair, bank;
    for (uint32_t i = 0; models->kind == CHUNK_KIND_HEADER && i < models->count; i++) {
        if (chunk_child(models, i, &pair) != CHUNK_OK || pair.kind != CHUNK_KIND_HEADER) continue;
        for (uint32_t k = 0; k < pair.count; k++) {
            if (chunk_child(&pair, k, &bank) != CHUNK_OK || bank.kind != CHUNK_KIND_BITMAP) continue;
            const uint8_t *own = palette6(bank_of(&bank), MODEL_PAL);
            if (own != NULL) map_track_bank(p, bank_of(&bank), own, used);
        }
    }
}

/* 004574ec's car fields as the loader (00457d40) left them: the high and
 * low detail banks with their '!PAL', the high detail object. */
static void car_art(const RaceState *race, CarLoad *const cars[RACE_CARS], RaceCarArt art[RACE_CARS])
{
    (void)race;
    memset(art, 0, sizeof *art * RACE_CARS);
    for (int i = 0; i < RACE_CARS; i++) {
        CarLoad *c = cars[i];
        if (c == NULL) continue;
        art[i].bank = c->has_bank[0] && c->palette[0] != NULL ? (uint8_t *)(uintptr_t)c->bank[0].data : NULL;
        art[i].palette = c->palette[0];
        art[i].bank2 = c->has_bank[1] && c->palette[1] != NULL ? (uint8_t *)(uintptr_t)c->bank[1].data : NULL;
        art[i].palette2 = c->palette[1];
        art[i].mesh = &c->object[0];
    }
}

static void remap_cars(RacePalette *p, const RaceState *race, CarLoad *const cars[RACE_CARS])
{
    RaceCarArt art[RACE_CARS];
    car_art(race, cars, art);
    race_palette_remap_cars(p, race->car_count, art, race->unknown_4fbab8);
}

/* port (car colours): the cars' slots of the screen palette, fitted to the colours the cars show (car_colours.h) */
/* the track, its scenery, the horizon and the scenery's models, mapped onto the screen palette (or, with `used`, only the entries they show) */
static void map_world(RacePalette *p, const Chunk *fam, const Chunk *terrain, const uint8_t *track, uint8_t *used)
{
    Chunk scenery, horizon, models;
    remap_track_banks(p, terrain, track, used);
    if (chunk_child(fam, FAM_CHILD_SCENERY, &scenery) == CHUNK_OK) remap_track_banks(p, &scenery, track, used);
    if (chunk_child(fam, FAM_CHILD_HORIZON, &horizon) == CHUNK_OK) {
        const uint8_t *sky = palette6(bank_of(&horizon), TRACK_PAL);
        remap_track_banks(p, &horizon, sky != NULL ? sky : track, used);
    }
    if (chunk_child(fam, FAM_CHILD_OBJECTS, &models) == CHUNK_OK) remap_model_banks(p, &models, used);
}

/* port (car colours): the cars' slots of the screen palette, fitted to the colours the cars show (car_colours.h); the entries the world is
 * mapped to are left as they are, so the scenery is what the exe maps it to whatever the cars are */
static void fit_cars(RacePalette *p, const RaceState *race, CarLoad *const cars[RACE_CARS], const uint8_t *carpal, const uint8_t keep[256])
{
    RaceCarArt art[RACE_CARS];
    car_art(race, cars, art);
    CarFitReport report;
    const int changed = car_colours_fit(p, race, art, carpal != NULL && shape_bank_find((uint8_t *)(uintptr_t)carpal, (const uint8_t[4]){ '!', 'P', 'A', 'L' }) != NULL, keep, &report);
    for (int i = 0; i < report.count; i++) {
        const CarFitBlock *b = &report.block[i];
        port_log("car_colours: entries %d..%d for %s: %d colours, %d new, distance %.1f -> %.1f\n", b->first, b->first + b->count - 1,
                 b->car >= 0 ? "one car" : "every car", b->colours, b->changed, b->before, b->after);
    }
    port_log("car_colours: %d entries fitted\n", changed);
}

/* port (car colours): with the palette as the exe has it at this point (the dash and the search set made), the world is mapped as the exe maps
 * it, the entries it and the dash show are kept, and the cars' slots are fitted to the rest; the fitted entries are then the cars' alone for the
 * port's tables of the world (palette_world_hide), and the search a car is mapped by is made again for the new colours */
static void fit_after_world(RacePalette *p, const RaceState *race, CarLoad *const cars[RACE_CARS], const uint8_t *carpal, const Chunk *fam,
                            const Chunk *terrain, const uint8_t *track, const uint8_t *dash_bank)
{
    uint8_t used[RACE_PALETTE_COLOURS] = { 0 }, table[RACE_PALETTE_COLOURS], before[sizeof p->screen_rgbx], hidden[RACE_PALETTE_COLOURS] = { 0 };
    memcpy(table, p->table, sizeof table);
    map_world(p, fam, terrain, track, NULL);
    map_world(p, fam, terrain, track, used);
    if (dash_bank != NULL) race_palette_bank_entries(dash_bank, used);
    memcpy(p->table, table, sizeof table);   /* the cars' mapping starts from the exe's identity (0xfa.. map to themselves) */
    memcpy(before, p->screen_rgbx, sizeof before);
    fit_cars(p, race, cars, carpal, used);
    for (int e = 0; e < RACE_PALETTE_COLOURS; e++) hidden[e] = memcmp(before + 4 * e, p->screen_rgbx + 4 * e, 3) != 0;
    palette_world_hide(hidden);
    if (p->inverse != NULL) race_palette_inverse(p);
}

/* the search set without the entries the fit gave the cars (the inverse made again for it, where there is one) */
static void hide_fitted(RacePalette *p)
{
    const uint8_t *hidden = palette_world_hidden();
    for (int e = 0; e < RACE_PALETTE_COLOURS; e++)
        if (hidden[e]) p->enabled[e] = 0;
    if (p->inverse != NULL) race_palette_inverse(p);
}

typedef struct {
    const DrivePaletteLate *late;
    RacePalette *p;
} AfterBanks;

/* 00447098's last call, 00456948 (the smoke's colours), when --drive has smoke. */
static void after_banks(void *ctx)
{
    const AfterBanks *a = ctx;
    if (a->late != NULL && a->late->after_banks != NULL) a->late->after_banks(a->late->ctx, a->p);
}

int drive_palette_build(const RaceState *race, const Chunk *fam, CarLoad *const cars[RACE_CARS], uint8_t *carpal,
                        DriveHud *hud, const DrivePaletteLate *late, const RacePaletteTint *tint, uint8_t rgb[768])
{
    static RacePalette p;
    Chunk terrain, first;
    if (chunk_child(fam, FAM_CHILD_TERRAIN, &terrain) != CHUNK_OK || terrain.kind != CHUNK_KIND_HEADER ||
        chunk_child(&terrain, 0, &first) != CHUNK_OK)
        return 0;
    const uint8_t *track = palette6(bank_of(&first), TRACK_PAL);
    if (track == NULL) return 0;
    track_palette_repair((uint8_t *)(uintptr_t)track);  /* port: CL1's rally FAM has a stray green at colour 2 */
    memset(&p, 0, sizeof p);
    /* 004c79b4 starts as the identity in the exe: the entries a car bank's
     * remap leaves alone (0xff, which the transparent mappers skip, and
     * 0xfa..0xfd outside the police slot) map to themselves. */
    for (int i = 0; i < RACE_PALETTE_COLOURS; i++) p.table[i] = (uint8_t)i;
    p.tint = tint != NULL ? *tint : (RacePaletteTint){ 0x10000, 0x100, 0, 0x100, 0x100 };
    p.tint_ready = 1;
    race_palette_queue(&p, TRACK_FIRST, TRACK_COUNT, track);
    /* own[i]: slot i's '!xxx' (car +0x526), as its loader set it. */
    const uint8_t *own[RACE_CARS] = { 0 };
    for (int i = 0; i < RACE_CARS; i++) own[i] = cars[i] != NULL ? cars[i]->own : NULL;
    race_palette_cars(&p, race, own, carpal);
    if (hud != NULL && hud->cockpit_ready && hud->cockpit.palette != NULL)
        cockpit_queue_palette(&p, hud->cockpit.palette);  /* 0041f988: the dashboard's 0..0x2f, after 004572e0 */
    if (hud != NULL) drive_hud_setup(hud, &p);  /* 00450838 after 0041f988, as 00455a5c */
    race_palette_apply(&p);
    if (hud != NULL && hud->cockpit_ready)  /* 0041f818 whole: the dash bank onto the palette, 004f9744, ... */
        cockpit_colours(&p, race, &hud->cockpit, &hud->cockpit_world, &hud->hud, &hud->colours);
    else
        race_palette_enable(&p, race);
    const int fitted = port_car_colours();
    palette_world_hide(NULL);
    if (fitted) fit_after_world(&p, race, cars, carpal, fam, &terrain, track, hud != NULL && hud->cockpit_ready ? hud->cockpit.bank : NULL);
    remap_cars(&p, race, cars);
    if (!fitted) map_world(&p, fam, &terrain, track, NULL);
    uint8_t enabled[RACE_PALETTE_COLOURS];
    memcpy(enabled, p.enabled, sizeof enabled);
    if (fitted) hide_fitted(&p);   /* the smoke's, the dash's and the mask's colours are the world's: not the slots the cars were given */
    if (late != NULL) late->run(late->ctx, &p);
    if (hud != NULL) drive_hud_start(hud, &p), hud->rgb = rgb;
    AfterBanks after = { late, &p };
    race_palette_map_banks(&p, after_banks, &after);  /* 00447098: the mask's and the smoke's banks */
    if (fitted) memcpy(p.enabled, enabled, sizeof enabled), p.inverse != NULL ? (void)race_palette_inverse(&p) : (void)0;
    for (int i = 0; i < 256; i++) {
        rgb[3 * i] = p.screen_rgbx[4 * i];
        rgb[3 * i + 1] = p.screen_rgbx[4 * i + 1];
        rgb[3 * i + 2] = p.screen_rgbx[4 * i + 2];
    }
    return 1;
}
