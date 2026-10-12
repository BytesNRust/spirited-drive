/* The race's screen palette and the colour table that maps a car's or a
 * track bank's own colours onto it (00455a5c builds it; see STATE.md, Race
 * palette). */
#ifndef TNFS_RACE_PALETTE_H
#define TNFS_RACE_PALETTE_H

#include "game/gfx/object3d.h"
#include "game/gfx/palette_match.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    RACE_PALETTE_COLOURS = 256,
    RACE_PALETTE_SLOTS = 16,     /* 0051c798: one byte a car slot */
    RACE_PALETTE_WRITES = 50,    /* 0050a9dc: 12 bytes each */
    RACE_PALETTE_BANKS = 100,    /* 0050b034: 16 bytes each */
    RACE_PALETTE_MODEL_LIGHTS = 26
};

/* The exe's colours 004572e0 queues: one a car model (004c578c, 3 bytes
 * each), and the police lights (004c5610, 4 colours). */
extern const uint8_t race_palette_model_light[RACE_PALETTE_MODEL_LIGHTS * 3];
extern const uint8_t race_palette_police_light[4 * 3];

/* 0050b774..0050b787: the tint 004471dc gives the race's colours (the
 * time of day). */
typedef struct {
    int32_t brightness;   /* 0050b774: 16.16 */
    int32_t red;          /* 0050b778: x / 256, negatives become 0 */
    int32_t saturation;   /* 0050b77c: 16.16 toward the brightest channel, at most 1 */
    int32_t green;        /* 0050b780 */
    int32_t blue;         /* 0050b784 */
} RacePaletteTint;

/* A queued write of screen palette entries: count 6-bit colours from first. */
typedef struct {
    int32_t first;
    int32_t count;
    const uint8_t *colours;
} RacePaletteWrite;

/* A shape bank queued for 00447098 to map onto the race palette with its
 * own 6-bit palette (00446fd0); first and count are kept, 00447098 does
 * not read them. */
typedef struct {
    uint8_t *bank;
    const uint8_t *palette;
    int32_t first;
    int32_t count;
} RacePaletteBank;

typedef struct {
    uint8_t screen[RACE_PALETTE_COLOURS * PALETTE_MATCH_ENTRY_BYTES];  /* 0052f178: B, G, R, 0xff */
    uint8_t screen_rgbx[RACE_PALETTE_COLOURS * 4];                     /* 0052ced8: R, G, B, 0 */
    uint8_t enabled[RACE_PALETTE_COLOURS];                             /* 004c5ff8: entries the search may pick */
    const uint8_t *inverse;                                            /* 004c5ff4: NULL for the search */
    uint8_t inverse_table[PALETTE_MATCH_INVERSE_SIZE];  /* port: the block 0047463c builds 004c5ff4 in */
    uint8_t table[RACE_PALETTE_COLOURS];                               /* 004c79b4 */
    uint8_t unknown_50ad2e[5];  /* 0050ad2e: 0xfa..0xfd for the police lights, then the slot's own colour */
    uint8_t copy[RACE_PALETTE_COLOURS * 3];  /* 0050ad34: the screen palette as 6-bit R, G, B (004766b8) */
    uint8_t slot_colours[RACE_PALETTE_SLOTS];  /* 0051c798: a slot's own entry (0x47, 0x5f), kept out of the search */
    RacePaletteWrite writes[RACE_PALETTE_WRITES];  /* 0050a9dc, applied by 00447048 */
    int32_t write_count;                           /* 004bcbc8 */
    RacePaletteBank banks[RACE_PALETTE_BANKS];     /* 0050b034, mapped by 00447098 */
    int32_t bank_count;                            /* 004bcbc4 */
    uint8_t changed;            /* 0052d2d8: set by a write, cleared once the screen took the palette */
    int32_t screen_bits;        /* 004c6130: up to 8, the screen takes palettes */
    int32_t palette_device;     /* 004c6108: bit 0, a palette the screen can take */
    int32_t handed;             /* port: how often 0047de34 handed the palette to the screen */
    RacePaletteTint tint;       /* 0050b774 */
    int32_t tint_ready;         /* 004bcbd4: 004471dc hangs while it is 0 */
    const uint8_t *carpal;      /* 004c5788: the CarPal bank, loaded for races of type 2 and up */
} RacePalette;

/* 004a2bf0: table[first .. first + count) = the screen entry nearest each
 * of `rgb`'s 8-bit colours (R, G, B), through 00473140's 8-bit path
 * (palette_match_rgb). */
void race_palette_table(RacePalette *p, int first, int count, const uint8_t *rgb);

/* 004470e0: maps a car's shape bank (an SHPI in memory) from its own 6-bit
 * palette onto the screen: the table for colours 0..0xf9 (0048f220), entry
 * 0xfe the slot's own colour when it has one, 0xfa..0xfd themselves for slot
 * 2 when `police` is set, then every shape not named "!..." through it, last
 * shape first. */
void race_palette_remap_bank(RacePalette *p, uint8_t *bank, const uint8_t *palette6, int32_t slot, int32_t police);

/* 00446fd0: the same for a track, sky or object bank (00447098 runs it on
 * the banks 00446f58 queued): table 0..0xfe from the palette, 0xff itself,
 * no slot or police colours. */
void race_palette_remap_track_bank(RacePalette *p, uint8_t *bank, const uint8_t *palette6);

/* 00446f90: queues a write of `count` colours (6 bits a channel) to the
 * screen entries from `first`. The original hangs on a 51st (it writes the
 * entry past the queue first); the port drops it. */
void race_palette_queue(RacePalette *p, int32_t first, int32_t count, const uint8_t *colours);

/* 00446f58: queues a shape bank and its 6-bit palette for 00447098. The
 * original writes the 101st past the table and then hangs; the port drops
 * it, and a count outside the table. */
void race_palette_queue_bank(RacePalette *p, uint8_t *bank, const uint8_t *palette, int32_t first, int32_t count);

/* 00476460: `count` 6-bit colours into the screen entries from `first`, each
 * channel v << 2 | v >> 4 (a byte): 0052ced8 as R, G, B, 0 and 0052f178 as
 * B, G, R, 0xff; then `changed`, and the palette goes to the screen
 * (0047de34, counted in `handed`) when it has 8 bits or fewer and the
 * device bit, which clears `changed` again. */
void race_palette_write(RacePalette *p, int32_t first, int32_t count, const uint8_t *colours);

/* 004766b8: `count` screen entries from `first` (0052f178) out as 6-bit
 * R, G, B (each channel >> 2). */
void race_palette_read(const RacePalette *p, int32_t first, int32_t count, uint8_t *out);

/* 00446880: the palette read into `copy` (004766b8), then the entry among
 * 0..0xfe nearest the 6-bit (red, green, blue) by the sum of the channels'
 * differences, the first of equals. The original returns its stack when no
 * entry is nearer than 99999 (a colour far outside 0..63); the port 0. */
int32_t race_palette_nearest(RacePalette *p, int32_t red, int32_t green, int32_t blue);

/* 00447048: every queued write in order (00476460 each), then the queue is
 * empty. */
void race_palette_apply(RacePalette *p);

/* 004471dc: tints 6-bit colours in place, entries first .. end - 1 taken
 * from `colours` on: each channel x its scale / 256, pulled toward the
 * brightest channel by `saturation`, x `brightness`, kept to 0..0x3f. A
 * negative channel scale and a saturation above 1 are corrected in `tint`
 * itself. The original hangs while tint_ready is 0; the port returns. */
void race_palette_tint(RacePalette *p, int32_t first, int32_t end, uint8_t *colours);

/* 004471ac (from the track's Info reader 0043a370, its time-of-day group):
 * the tint 004471dc applies, and tint_ready set. */
void race_palette_set_tint(RacePalette *p, int32_t red, int32_t green, int32_t blue, int32_t saturation,
                           int32_t brightness);

/* 004473d8 (from 00457d40 for each car bank's palette): a palette shape
 * tinted whole, 004471dc(0, width x height / 3, its data at +0x10), the
 * width and height the shorts at +4 and +6 (the product wraps; idiv). */
void race_palette_tint_shape(RacePalette *p, uint8_t *shape);

/* 0041f818, its first part (up to 0041f8dc; the rest sets up the dash):
 * the colour search may pick entries 0..0xfe again, except 0x47, 0x5f
 * (race types below 2), 0xfa..0xff and each car slot's own colour; the
 * inverse table is dropped. */
void race_palette_enable(RacePalette *p, const RaceState *race);

/* 0047463c: the inverse table built again from the screen entries (0052f178
 * copied out, 00476688) the search may pick (004c5ff8), 5 bits a channel
 * (00474578 -> 00473d80, inverse_map_build: Thomas's incremental scan,
 * near but not always the nearest entry); a cell no entry claims stays 0
 * (the port's block starts zeroed). Returns 0 with no table when the
 * distance buffer cannot be had, as when 00474cdc fails. */
int race_palette_inverse(RacePalette *p);

enum {
    RACE_FADE_LEVELS = 16,
    RACE_FADE_BYTES = 4 + RACE_FADE_LEVELS * RACE_PALETTE_COLOURS  /* 0x1004, 00482a36's block */
};

/* 00482a36 (watcall red, green, blue, the 6-bit palette `copy6` in ecx):
 * out[0] = 00473140(red << 16 | green << 8 | blue), out[1..3] the three
 * bytes; then for level i (0..15) and colour c, out[4 + 256 i + c] =
 * 00473140 of each channel copy6[3 c + k] x 4 moved toward red / green /
 * blue by (i + 1) / 16 (0049f380: a + (b - a) x t >> 16, rounded at bit
 * 15), the three ORed unmasked as red << 16 | green << 8 | blue. The match
 * is 00473140's 8-bit path (the race's surface) with the inverse table
 * when it is set. The original allocates the block (00474c8c). */
void race_palette_fade(const RacePalette *p, int32_t red, int32_t green, int32_t blue, const uint8_t *copy6,
                       uint8_t out[RACE_FADE_BYTES]);

/* 004572e0: queues the cars' colours. Race types below 2: each car not
 * flagged 8 at +0x174 (and not slot 2 in a type 1 race played back) puts its
 * own colours (`own`, its '!xxx') at its slot's range (0x30 and 0x48, 0x17
 * each; slot 2 none; slots 3.. 8 each from 0x60) and slots 0 and 1 their own
 * entry (0x47, 0x5f) a colour of cars[0]'s model; then the 4 police-light
 * colours at 0xfa. Types 2 and up: the CarPal bank (`carpal`, the load of
 * "%sCarPal") has its '!PAL' tinted and queued at 0x48 (0x48 colours), the
 * followed car's own at 0x30 (0x18), and only the followed slot gets its own
 * entry 0x47. Where the original hangs or faults (no CarPal, no '!PAL', a
 * '!PAL' of type 0x24) the port returns. */
void race_palette_cars(RacePalette *p, const RaceState *race, const uint8_t *const own[RACE_CARS], uint8_t *carpal);

/* What a car slot holds for 004574ec; in the original these are car record
 * fields. */
typedef struct {
    uint8_t *bank;              /* +0x51e, with its 6-bit palette +0x522 */
    const uint8_t *palette;
    uint8_t *bank2;             /* +0x656, with its 6-bit palette +0x65a */
    const uint8_t *palette2;
    Object3D *mesh;             /* +0x52e */
} RaceCarArt;

/* 004574ec: each of the race's cars (004fb7e8, up to RACE_CARS) has its
 * two banks mapped onto the race palette (004470e0, the slot passed) and,
 * for slots 0 and 1, its mesh's texture colours found again (00490cc0).
 * `police` is 004fbab8. A missing bank or mesh is skipped (the original
 * faults). */
void race_palette_remap_cars(RacePalette *p, int32_t car_count, const RaceCarArt art[RACE_CARS], int32_t police);

#endif
