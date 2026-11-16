/* The race's results screen (0045cea0), shown after a race before the standings. */
#ifndef TNFS_RESULTS_SCREEN_H
#define TNFS_RESULTS_SCREEN_H

#include "game/frontend/palette_put.h"
#include "game/frontend/yes_no_box.h"
#include "game/race/car_registry.h"
#include "game/sys/race_records.h"

#include <stddef.h>
#include <stdint.h>

/* The calls 0045cea0 makes. Shapes are 004732dd's; a palette argument is
 * one of the screen's 0x300-byte palettes. Text goes out through a 0x5c
 * byte buffer (the original's on its stack). */
typedef struct ResultsOps {
    int32_t (*records)(void *ctx);                             /* 0045bad8: the records beaten, 1 / 2 / 3 */
    void (*flush)(void *ctx);                                  /* 00464a30 */
    void (*pointer_move_to)(void *ctx, int32_t x, int32_t y);  /* 00464038: eax, edx */
    int32_t (*load_art)(void *ctx, const char *name);          /* 00466c34: eax the name; the bank */
    int32_t (*load_inverse)(void *ctx, const char *name);      /* 00466c00: eax the name */
    void (*music_feed)(void *ctx);                             /* 004640d0 */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);  /* 004732dd (cdecl) */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    void (*text_styles)(void *ctx);                            /* 0046376c */
    void (*pointer_poll)(void *ctx);                           /* [004c6194] */
    int32_t (*pointer_in_rect)(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1);  /* 00463f80 */
    void (*select)(void *ctx, int32_t picture);                /* 00473680 (cdecl) */
    void (*blit)(void *ctx, const uint8_t *shape);             /* 004737ec (cdecl): at its own place */
    void (*text_select)(void *ctx, int32_t style);             /* 004638c0 */
    void (*text_at)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463cd8 */
    void (*text_fit)(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);  /* 00463d34 */
    void (*time_text)(void *ctx, char *out, int32_t time);     /* 00466d5c: eax out, edx a time */
    void (*average_text)(void *ctx, char *out, int32_t time, int32_t distance);  /* 00466e5c: eax, edx, ebx */
    void (*speed_text)(void *ctx, char *out, int32_t speed);   /* 00466c9c: eax out, edx the speed */
    void (*palette_write)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00476460 (cdecl) */
    void (*pointer_draw)(void *ctx);                           /* [004c6190] */
    void (*select_default)(void *ctx);                         /* 00473604 */
    void (*pointer_hide)(void *ctx);                           /* [004c618c] */
    void (*lock)(void *ctx);                                   /* 00473578 */
    void (*blit_word)(void *ctx, int32_t shape);               /* 004737ec on a picture's +0x2c */
    void (*unlock)(void *ctx);                                 /* 004733b0 */
    void (*flip)(void *ctx);                                   /* 004733ec */
    void (*pointer_show)(void *ctx);                           /* 00441798 */
    void (*pointer_mark)(void *ctx, DialogPointer *p);         /* 00441858: the marks = the pointer's low halves */
    int32_t (*pointer_buttons)(void *ctx);                     /* 004417f8 */
    void (*track_music)(void *ctx, const char *name);          /* 00464788: eax the name */
    void (*track_play)(void *ctx);                             /* 00464850 */
    void (*delay)(void *ctx, int32_t time);                    /* 0047334f (cdecl) */
    void (*sound)(void *ctx, int32_t sound);                   /* 0046492c */
    int32_t (*ticks)(void *ctx);                               /* 00473334 */
    void (*pointer_activate_if_moved)(void *ctx);              /* 00464088 */
    int32_t (*key)(void *ctx);                                 /* 00464bd0 */
    void (*pointer_click)(void *ctx);                          /* 00441814 */
    void (*pointer_off)(void *ctx);                            /* 00464060 */
    void (*music_service)(void *ctx);                          /* 0046417c */
    int32_t (*deadline_passed)(void *ctx);                     /* 0047336d */
    void (*track_stop)(void *ctx);                             /* 0046486c */
    void (*music_free)(void *ctx);                             /* 004648a8 */
    void (*fade_out)(void *ctx, int32_t first, int32_t count); /* 00463ef0: eax, edx */
    void (*free)(void *ctx, int32_t bank);                     /* 00475260 (cdecl) */
    void (*free_inverse)(void *ctx);                           /* 00474558 */
    void (*blit_masked)(void *ctx, const uint8_t *shape);      /* 00473ab4 (cdecl): 0045c45c's */
    int32_t (*totals)(void *ctx);                              /* 0045be50: 0045de08's, the row or 0x14 (ax) */
    int32_t (*top_record)(void *ctx);                          /* 0045c2d0: 1 when beaten (ax) */
    int32_t (*leads)(void *ctx);                               /* 0045b73c: the player leads the tournament */
    void (*best_totals)(void *ctx, int32_t row, int32_t top);  /* 0045c45c: eax, edx */
    int32_t (*lap_record)(void *ctx);                          /* 0045b908: 0045eff4's, 1 when beaten (ax) */
    void (*results_order)(void *ctx);                          /* 0045b6c8: the finishers (finish) by total */
    int32_t (*lap_times)(void *ctx, int32_t record, int32_t tune);  /* 0046067c: eax, edx; ax */
    void *ctx;
    /* port: a word of the header band (results_header_marks): `text` from x, its letters' middle on y_mid, at the font's size or
     * smaller so it stays within `room` pixels (central_glue's cg_band_text). NULL: text_at at RESULTS_LINE_* as before. */
    void (*band_text)(void *ctx, const char *text, int32_t x, int32_t y_mid, int32_t room);
    /* port (dep55): a record's conditions in the row's style at num/den of the font's size from (x, y), their width and their letters'
     * rows (record_conditions.h's pen; NULL: none drawn) */
    void (*small_text)(void *ctx, const char *text, int32_t x, int32_t y, int32_t num, int32_t den);
    int32_t (*small_width)(void *ctx, const char *text, int32_t num, int32_t den);
    void (*small_ink)(void *ctx, const char *text, int32_t num, int32_t den, int32_t *top, int32_t *bottom);
} ResultsOps;

enum { RESULTS_GAME = 0x5bc, RESULTS_IMAGE = 0x5f34, RESULTS_CARS = 12 };

/* port: the cheats a records table's rows were set with (records_route_shown's slots of the table shown) and the laps choice its
 * circuit totals take (records_slot_totals); and, with Restomod's records, the table's words ("MIRROR - NO CHEATS - 6 LAPS") drawn under
 * the header band (RESULTS_LINE_*). */
typedef struct ResultsMarks {
    const uint8_t *slots;   /* RECORDS_MARK_SLOTS bytes, EXTRAS_MARK_* a slot */
    int choice;
    const char *line;       /* NULL: none (Stock, Restored) */
    const RecordMarks *conds;   /* RECORDS_MARK_SLOTS, each slot's row's conditions (Restomod's records); NULL: none */
} ResultsMarks;

/* port: the letters start here, in the gap after the record's name (names on the cheats table fit 200, to 207) and
 * before the time (305); "Best Time (c)" is the header's short mark (the full words run past the edge) */
enum { RESULTS_MARK_X = 0xd7 };
/* port (owner, 2026-11-16): with the table line (Restomod) the cheats are words in the conditions' small style (record_cheat_draw),
 * centred in that gap, 4 px clear of the name and of the time */
enum { RESULTS_TAGS_CENTRE = (207 + 305) / 2, RESULTS_TAGS_ROOM = 305 - 207 - 8 };

typedef struct ResultsInputs {
    const uint8_t *game;        /* 0052ad44: the game state, RESULTS_GAME bytes (tournament.h) */
    const uint8_t *image;       /* 00524df0: config.dat's image, RESULTS_IMAGE bytes (the race setup, the records) */
    const int16_t *dropped;     /* 004f82a0: a short per player, set when it did not finish */
    int32_t player;             /* 004fbb08: this machine's player */
    int place;                  /* the short 0051dcc2: the player's place, from 0 */
    int rival_place;            /* the short 0051dcc0 */
    int rival;                  /* the short 0051dcc6: the rival player */
    int rival_car;              /* the short 0051dcc8: its car's short name */
    int german;                 /* the short 0052b31c: the words */
    int kmh;                    /* port: the distances' units (resource_kmh), which the language does not change */
    int unknown_50a5b6;         /* the short 0050a5b6: 1 ends the screen */
    int32_t screen;             /* 00524834 */
    int32_t screen_shape;       /* [00524834] + 0x2c */
    const char *const *car_names;  /* 004c5c84: RESULTS_CARS short names; read through results_car_name */
    int32_t lost_vegas;         /* 004baf18: 0045de08's, a ninth track counted */
    int32_t variant_r;          /* 004fbadc: 0046067c's, the track's "r" layout */
    int circuit_place;          /* the short 0051dcc4: 0045eff4's player's place, from 0 */
    int circuit_rival_place;    /* the short 0051dcbe: 0045eff4's rival's */
    const int32_t *finish;      /* 0051dccc: 8 dwords, the players by total (results_order fills it) */
    const ResultsMarks *marks;  /* port: the table with cheats the records shown are from; NULL: the table without */
} ResultsInputs;

/* port: the table shown is the one with cheats (its marks are there; marks NULL or without slots is the table without) */
static inline int results_cheats(const ResultsInputs *in) { return in->marks != NULL && in->marks->slots != NULL; }
/* port: the header's "(CHEATS)" is drawn: a table with cheats and no table line (results_header_marks, standings.c) */
static inline int results_banner_shown(int cheats, const char *line) { return cheats && line == NULL; }
/* port: the header band's "(CHEATS)" on every post-race screen of a race with cheats, right of the Road & Track logo (its
 * art ends near x 270); the same place on the segment, totals, circuit, track, lap and best-totals screens. port (owner, 2026-11-16):
 * not when the table line is drawn (Restomod), which says CHEATS already */
#define CHEATS_BANNER "(CHEATS)"
enum { CHEATS_BANNER_X = 0x122, CHEATS_BANNER_Y = 0x14 };
/* port (dep55): the table line, under the banner's place (drawn there only without a band_text op) */
enum { RESULTS_LINE_X = 0x122, RESULTS_LINE_Y = 0x28 };
/* port: the header band right of the Road & Track logo and above the separator under it (y 57): the banner and the table line
 * are drawn in it, the line alone with its middle on the band's, the two stacked when there is a banner; never wider than the
 * band from x to the right margin */
enum { RESULTS_BAND_X = 0x122, RESULTS_BAND_ROOM = 0x27a - 0x122, RESULTS_BAND_MID = 0x1c, RESULTS_BAND_TOP_MID = 0x10,
       RESULTS_BAND_LOW_MID = 0x2a };
/* port: where in the band the banner and the line go (their letters' middles), 0 when not drawn; inline, as the standings use it
 * without the results screens */
static inline void results_band_places(int banner, int line, int32_t *banner_mid, int32_t *line_mid)
{
    *banner_mid = !banner ? 0 : line ? RESULTS_BAND_TOP_MID : RESULTS_BAND_MID;
    *line_mid = !line ? 0 : banner ? RESULTS_BAND_LOW_MID : RESULTS_BAND_MID;
}
/* port (dep55): the banner and the table line of the records shown, as every post-race screen draws them */
void results_header_marks(const ResultsInputs *in, const ResultsOps *ops);

typedef struct ResultsState {
    int32_t inverse;            /* 004c5ff4 */
    uint8_t *screen_palette;    /* 00520dd8 (palette_put) */
    int16_t record_music;       /* the short 0051dd14: a record's tune playing */
} ResultsState;

/* port: a record row's cheat letters at y (the table with cheats only; nothing for a slot out of the track's), and the
 * row of the track's totals (row 0: the best time) with its laps choice */
void results_mark_slot(const ResultsInputs *in, int slot, int32_t y, const ResultsOps *ops);
void results_mark_total(const ResultsInputs *in, int row, int32_t y, const ResultsOps *ops);
/* port (dep55): a record row of the image (`row` at its name) is drawn: every row in Stock and Restored (the original's file, no
 * table line); in Restomod a row with a record only (its set word, race_records_row_set), as an empty row's longest time, car 0
 * and gearbox 0 are no record (the original's empty table shows nothing either) */
int results_row_shown(const ResultsInputs *in, const uint8_t *row);
/* port (dep55): Restomod's record row's conditions as Best Times shows them (record_conditions_draw): the small two-line block ending just
 * left of the car's column, within the row's line; a speed's (`speed`) smaller, as it runs wider than a time. Nothing in Stock and Restored. */
void results_conditions(const ResultsInputs *in, int slot, int32_t y, int speed, const ResultsOps *ops);

/* 0045cea0 (ax: ask 0045bad8 for the records beaten): stats/standseg.qfs
 * for the segment (00524e83, the last but one when 0) on the track: the
 * place ("%d / 8", "1 / 2" / "2 / 2" against the other player or the
 * segment's best, or DNF), the player's name, car, time, total (0 when
 * flagged), average and top speed against the rival's (the best time's on
 * a time trial), then the track's two records (the record lines lit when
 * beaten), each with its car and gearbox. A record beaten with the track
 * music on (00524e6b) plays a fanfare. The pointer over "Done" (0,
 * 0x1b3)..(0x82, 0x1e0) selects it, an arrow moves back onto it; Enter,
 * space or the left button on it (once armed) ends, as do Escape, the
 * timer (0047336d), the right button and 0050a5b6. */
void results_screen(int32_t ask_records, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                    const ResultsOps *ops);

/* port (dep59): the short name of `car` the screens show: car_names' for the twelve, an added car's own, "" for a number that is no car
 * (car_registry_short_name: never what lies after the table) */
static inline const char *results_car_name(const ResultsInputs *in, int car)
{
    return car_registry_short_name(car, in->car_names, RESULTS_CARS);
}

#endif
