/* The in-race menu (00447564, 004b90dc[0], Esc): the track's map with the
 * cars on it and six items (continue, replay, audio, graphics, chat, quit),
 * three of them opening the frontend's option screens. */
#ifndef TNFS_RACE_MENU_H
#define TNFS_RACE_MENU_H

#include <stdint.h>

enum {
    RACE_MENU_ITEMS = 6, RACE_MENU_NONE = 6,  /* the item under nothing */
    RACE_MENU_TRACKS = 9, RACE_MENU_VARIANTS = 3, RACE_MENU_OFFSETS = 27, RACE_MENU_CARS = 8,
    RACE_MENU_POINTS = 600,                   /* the frame's room for the map (0x960 bytes) */
    RACE_MENU_SETTINGS = 0xac,                /* 00524df9 / 0052b1ec: 0x2b dwords */
    RACE_MENU_PALETTE = 0x300,
    RACE_MENU_NAME = 0x50                     /* the map file's name (the frame's 0x1260..0x12b0) */
};

/* Offsets of the words read in the settings block (00524df9 +) */
enum {
    RACE_MENU_MUSIC = 0x6a,        /* 00524e63 */
    RACE_MENU_MUSIC_VOLUME = 0x6c, /* 00524e65 */
    RACE_MENU_UNKNOWN_E67 = 0x6e,  /* 00524e67 */
    RACE_MENU_UNKNOWN_E69 = 0x70,  /* 00524e69 */
    RACE_MENU_KEPT = 0x78,         /* 00524e71..00524e79: 004054cc's first word, 004fb789, 785, 787, 786 */
    RACE_MENU_VARIANT = 0x8a,      /* 00524e83 */
    RACE_MENU_TRACK = 0x8c         /* 00524e85 */
};

/* One track's (and variant's) map: x, y words, each divided by `divisor` (y negated) */
typedef struct {
    const int16_t (*points)[2];
    int16_t count;
    int16_t divisor;
} RaceMenuMap;

extern int16_t race_menu_item_x[RACE_MENU_ITEMS];       /* 00447410 */
extern int16_t race_menu_item_y[RACE_MENU_ITEMS];       /* 0044741c */
extern int16_t race_menu_x_offset[RACE_MENU_OFFSETS];   /* 00447428 */
extern int16_t race_menu_y_offset[RACE_MENU_OFFSETS];   /* 0044745e */
extern const char *race_menu_track_names[RACE_MENU_TRACKS];  /* 00447494 */
extern char race_menu_marker_names[RACE_MENU_CARS][5];       /* 004474b8 */
extern RaceMenuMap race_menu_maps[RACE_MENU_TRACKS][RACE_MENU_VARIANTS];

/* The words 00447564 reads and writes */
typedef struct {
    uint8_t settings[RACE_MENU_SETTINGS];  /* 00524df9 */
    uint8_t saved[RACE_MENU_SETTINGS];     /* 0052b1ec: copied to the settings at the start, back at the end */
    int32_t in_menu;                       /* 004fb78c */
    int32_t loading;                       /* 004fcdac */
    int16_t unknown_4c5cca;
    int32_t high;                          /* 004fa0c0: 640 x 480 */
    uint8_t unknown_4fb784, unknown_4fb785, sky_off, unknown_4fb787, screen_kind;  /* 004fb784..787, 789 */
    int32_t race_type;                     /* 004fb7d0 */
    int32_t player;                        /* 004fbb08 */
    int32_t car_node[RACE_MENU_CARS];      /* 00509e1c[k] + 0x48 */
    int32_t player_unknown_512;            /* 00509e1c[player] + 0x512 */
    int16_t leader;                        /* 00509e60: the car "lear" marks (race types 2, 3) */
    uint32_t sounds;                       /* 00524824: 00487e10's argument at the start */
    uint32_t view_surface;                 /* 004c2d90 */
    uint32_t menu_surface;                 /* 00524834 */
    uint32_t inventory;                    /* 004c5ff4 */
    int32_t blink_clock;                   /* 004bb8dc */
    uint8_t esc_down;                      /* 004c693d */
    int16_t playback;                      /* 0052b312 */
    int16_t dropped;                       /* 0052b314 */
    int16_t german;                        /* 0052b31c */
    int16_t mouse_on;                      /* 00524790 */
    int16_t mouse_buttons;                 /* 00524792 */
    int32_t end_state;                     /* 00509d70 */
    int16_t unknown_50a5b6;
    int32_t net_game;                      /* 004fb7d4 */
    int32_t unknown_4fbb0c;
    int16_t unknown_4c5cbe;
    int16_t unknown_52b322, unknown_52b326, unknown_52b328;
    int32_t unknown_4fb817[RACE_MENU_CARS];  /* 004fb817 + 0x4b x player */
    int32_t unknown_4fb81b[RACE_MENU_CARS];  /* 004fb81b + 0x4b x player */
    int32_t unknown_4b6bb0;
    int32_t unknown_4f9b7c;
    int32_t follow;                        /* 004f801c */
    int32_t phase;                         /* 004c5390 */
    int32_t unknown_509e04;
    int32_t unknown_513e50;
    int32_t unknown_4bafe0;
} RaceMenuWorld;

/* 00487e10's two arguments */
typedef enum { RACE_MENU_SOUNDS_OWN = 0, RACE_MENU_SOUNDS_RACE = 1 } RaceMenuSounds;  /* [00524824], 004c6fac */

/* The calls, each the original's (the frontend's menu library mostly) */
typedef struct {
    void *ctx;
    void (*call)(void *ctx, uint32_t address);  /* the calls with no arguments and no result, by address */
    void (*sounds)(void *ctx, RaceMenuSounds which);                     /* 00487e10 */
    int16_t (*kept)(void *ctx);                                          /* 004054cc: its first word (the second unused) */
    void (*fill)(void *ctx, uint32_t address, uint8_t *palette);         /* 00463df0 / 004766b8(0, 0x100, p) */
    void (*fade)(void *ctx, int32_t first, int32_t count);              /* 00463ef0 */
    void (*set_palette)(void *ctx, uint32_t address, const uint8_t *palette);  /* 00476460 / 00463e7c (0, 0x100, p) */
    void (*mode)(void *ctx, int32_t width, int32_t height, int32_t bits);      /* 00477980 */
    void (*clip)(void *ctx, int32_t left, int32_t top, int32_t right, int32_t bottom);  /* 0047723c */
    void (*surface_free)(void *ctx, uint32_t surface);                   /* 004746a0 */
    uint32_t (*surface_new)(void *ctx, int32_t width, int32_t height);   /* 0047310c(w, h, 0) */
    void (*select)(void *ctx, uint32_t surface);                         /* 00473680 */
    void (*present)(void *ctx, uint32_t surface);                        /* 004737ec([surface + 0x2c]) */
    void (*pointer_to)(void *ctx, int16_t x, int16_t y);                 /* 00464038 */
    void (*music)(void *ctx, const char *name);                          /* 00464428 */
    uint8_t *(*load)(void *ctx, const char *name);                       /* 00466c34 */
    uint32_t (*load_inventory)(void *ctx, const char *name);             /* 00466c00 */
    void (*unload)(void *ctx, uint8_t *bank);                            /* 00475260 */
    void (*palette)(void *ctx, uint8_t *to, const uint8_t *from, int16_t count);  /* 00463e08(to, 0, 0x100, from, count) */
    void (*number)(void *ctx, uint32_t address, int32_t value);          /* 00441530, 0046492c, 0044f534, 004015dc, 00423d28 */
    int32_t (*inside)(void *ctx, int16_t left, int16_t right, int16_t top, int16_t bottom);  /* 00463f80 */
    int16_t (*chat)(void *ctx);                                          /* 004109b4 */
    void (*blit_own)(void *ctx, const uint8_t *shape);                   /* 004737ec */
    void (*blit_at)(void *ctx, const uint8_t *shape, int32_t x, int32_t y);  /* 00473ad0 */
    int16_t (*buttons)(void *ctx);                                       /* 004417f8 */
    int32_t (*ticks)(void *ctx);                                         /* 00473334 */
    uint16_t (*key)(void *ctx);                                          /* 00464bd0 */
    int16_t (*ask)(void *ctx, const char *question);                     /* 00465f4c */
    void (*notice)(void *ctx, const char *text, int32_t time);           /* 00467214 */
    void (*net_send)(void *ctx, int32_t to);                             /* 00410b7c */
    int32_t (*net_wait)(void *ctx, int32_t what);                        /* 00410c18 */
    void (*take)(void *ctx, int32_t kind);                               /* 00405b00(004f801c, kind) */
    void (*view_screen)(void *ctx, int32_t a, int32_t b);                /* 0040540c */
} RaceMenuOps;

/* 00447564 (watcall ()): the menu shown until an item ends it, the race's
 * screen and settings put back after. */
void race_menu(RaceMenuWorld *w, const RaceMenuOps *ops);

#endif
