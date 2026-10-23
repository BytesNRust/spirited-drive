/* Control Central's (00432ff4) frame, shared by its parts. */
#ifndef TNFS_CENTRAL_RUN_H
#define TNFS_CENTRAL_RUN_H

#include "game/frontend/central.h"

enum {
    CC_OPPONENT, CC_CAR, CC_TRACK, CC_RACE_TYPE, CC_RACE, CC_OPTIONS, CC_CHAT, CC_CANCEL, CC_NONE, CC_PLAYERS,
    CC_ITEMS
};

/* the art banks' slots (004bafc0..) and the art's last values' (004bafb4..) */
enum { ART_TRACK, ART_RACE, ART_CAR, ART_OPPONENT, ART_CENTRAL, ART_ANI, ART_PANEL };
enum { WAS_OPPONENT, WAS_RACE, WAS_TRACK, WAS_CAR, WAS_M, WAS_R };

typedef struct CentralRun {
    ControlCentralState *st;
    const ControlCentralOps *ops;
    int16_t item;            /* esp+504 */
    int32_t sub;             /* esp+4f4: 1 the left arrow, 2 the right one */
    int32_t redraw;          /* esp+508 */
    int32_t banner_shown;    /* port: CHEATS ENABLED drawn on the frame (central_banner.h) */
    int32_t first;           /* esp+50c */
    int32_t panel;           /* esp+4fc: the players' panel drawn again */
    int32_t keys_armed;      /* esp+514 */
    int32_t buttons_armed;   /* esp+4f8 */
    int32_t done;            /* esp+4d4 */
    int16_t result;          /* esp+510 */
    int32_t changed;         /* esp+4e8 */
    int32_t blink_ticks;     /* esp+4cc */
    int32_t blink;           /* esp+4c8 */
    int32_t tick_sum;        /* esp+4d8 */
    int16_t track_was;       /* esp+4e4 */
    int16_t race_was;        /* esp+4e0 */
    int16_t players_was;     /* esp+4d0 */
    int32_t answer;          /* esp+500 */
    const uint8_t *pal;      /* esp+4c0: central.qfs's !pal */
    uint8_t palette[0x300];  /* esp+0 */
} CentralRun;

/* the pointer's places, by item (004baed0, 004baee4) */
extern const int16_t CENTRAL_X[CC_ITEMS], CENTRAL_Y[CC_ITEMS];
/* a tournament's car by track (004baef8) */
extern const int16_t CENTRAL_TOURNAMENT_CAR[10];

void central_move(CentralRun *r);                  /* the pointer to the item's place */
void central_load(CentralRun *r);                  /* central.inv, central.qfs, ani.qfs and the panel, !pal */
void central_palette(CentralRun *r);               /* 00463e08 with !pal */
void central_laps(CentralRun *r);                  /* the laps by difficulty and track */
void central_follow(CentralRun *r);                /* 00433301..00433ad9: the pointer picks the item */
void central_after_follow(CentralRun *r);          /* 00433ad9..00433cc6 */
void central_draw(CentralRun *r);                  /* 00433cd5..00434b54 */
void central_changed(CentralRun *r);               /* 00434b69..00434c88 */
void central_cheats(CentralRun *r);                /* 00434cdf..00434ff8 */
int central_select(CentralRun *r);                 /* 00435120..004360bc; 1 to return the frame's result */
void central_keys(CentralRun *r, int16_t key);     /* 004360df..0043700e */
void central_opponent_up(CentralRun *r, int mouse);
void central_opponent_down(CentralRun *r, int mouse);
void central_car_start(ControlCentralState *st);   /* 00432ff4's player car at the start: 11 goes to 0 */
void central_car_up(CentralRun *r);
void central_car_down(CentralRun *r);

#endif
