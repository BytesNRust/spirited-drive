/* 00432ff4: Control Central (control/central.qfs), the race setup screen,
 * single-player path. Four panels (opponent, player car, location, race
 * type), each with left/right arrows, and the RACE / OPTIONS / CANCEL
 * buttons. Focus follows the pointer; the keyboard moves focus by moving the
 * pointer to the area's centre. Field comments name the original's locals. */
#include "game/frontend/frontend.h"

#include "game/frontend/dialog.h"
#include "game/frontend/config.h"
#include "game/frontend/race_settings.h"

#include "formats/fsh.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <string.h>

enum {
    FOCUS_OPPONENT = 0,
    FOCUS_PLAYER = 1,
    FOCUS_LOCATION = 2,
    FOCUS_RACE_TYPE = 3,
    FOCUS_RACE = 4,
    FOCUS_OPTIONS = 5,
    FOCUS_CHAT = 6,    /* network only */
    FOCUS_CANCEL = 7,
    FOCUS_NONE = 8,
    FOCUS_COUNT = 10,  /* 9 is the network player list */
    PANELS = 4,

    ARROW_NONE = 0,
    ARROW_LEFT = 1,
    ARROW_RIGHT = 2,

    IDLE_TIMEOUT = 12000,  /* 120 s, then back to the intro */
    KEY_REPEAT_TICKS = 25
};

/* 004baed0 / 004baee4: pointer position for each focus. */
static const int CENTRE_X[FOCUS_COUNT] = { 482, 175, 444, 211, 45, 228, 400, 570, 0, 372 };
static const int CENTRE_Y[FOCUS_COUNT] = { 158, 197, 318, 349, 454, 454, 454, 454, 0, 225 };

typedef struct {
    int x0, x1, y0, y1;
} Rect;

/* 00463f80 calls, in the original's test order. */
static const Rect PANEL_RECT[PANELS] = {
    { 0x153, 0x271, 0x61, 0xda },   /* opponent */
    { 0x13, 0x14a, 0x70, 0x11a },   /* player */
    { 0x153, 0x225, 0xfc, 0x17f },  /* location */
    { 0x5a, 0x14c, 0x13a, 0x180 }   /* race type */
};
static const Rect LEFT_ARROW[PANELS] = {
    { 0x155, 0x16a, 0x91, 0xa8 }, { 0x15, 0x28, 0xba, 0xd1 },
    { 0x155, 0x168, 0x131, 0x14e }, { 0x5c, 0x6b, 0x154, 0x16d }
};
static const Rect RIGHT_ARROW[PANELS] = {
    { 0x25d, 0x26f, 0x91, 0xa8 }, { 0x136, 0x148, 0xba, 0xd1 },
    { 0x20f, 0x223, 0x131, 0x14e }, { 0x136, 0x145, 0x154, 0x16d }
};
static const Rect OPTIONS_RECT = { 0xa5, 0x125, 0x1ba, 0x1d1 };
static const Rect CANCEL_RECT = { 0x1f8, 0x27f, 0x1ba, 0x1d1 };
static const Rect RACE_RECT = { 0, 0x70, 0x1ba, 0x1d1 };

/* 004af928...: focus frame and its left/right arrow highlights per panel. */
static const char FRAME[PANELS][5] = { "Trb3", "Tlb1", "Brb4", "Blb2" };
static const char ARROW[PANELS][2][5] = {
    { "Aro5", "Aro6" }, { "Aro1", "Aro2" }, { "Aro7", "Aro8" }, { "Aro3", "Aro4" }
};

/* 00432050 / 00432078 / 0043209c: track art, base and "m"/"r" layouts. The
 * hidden track ("moon") has no art on the CD. */
static const char *const TRACK[10] = {
    "city", "coast", "alpine", "springs", "avalley", "bsienna", "vridge", "tpolis", "lvegas", "moon"
};
static const char *const TRACK_M[9] = {
    "citym", "coastm", "alpinem", "springsm", "avalleym", "bsiennam", "vridgem", "tpolism", "lvegasm"
};
static const char *const TRACK_R[9] = {
    "cityr", "coastr", "alpiner", "springsr", "avalleyr", "bsiennar", "vridger", "tpolisr", "lvegasr"
};
/* 004320c0 / 004320f0: player and opponent panels. */
static const char *const PLAYER[12] = {
    "rx71", "nsx1", "sup1", "9111", "vet1", "vip1", "5121", "dia1", "war1", "war1", "war1", "war1"
};
static const char *const OPPONENT[12] = {
    "rx72", "nsx2", "sup2", "9112", "vet2", "vip2", "5122", "dia2", "slew", "watch2", "tourny", "war2"
};
/* 00432120 */
static const char *const RACE_TYPE[RACE_TYPES] = { "watch1", "headhead", "single", "tourny" };

typedef struct {
    char name[16];
    uint8_t *data;
    Fsh fsh;
} Panel;

typedef struct {
    uint8_t *data;
    Fsh fsh;
    uint8_t palette[PALETTE_BYTES];
    Panel panels[PANELS];
    int focus;         /* uStack_2c */
    int arrow;         /* local_3c */
    int redraw;        /* local_28 */
    int fade_in;       /* local_24 */
    int key_armed;     /* local_1c */
    int held_ticks;    /* local_58 */
    int button_armed;  /* local_38 */
    int buttons;       /* 00524792 */
    int done;          /* local_5c */
    int result;        /* local_20 */
} Central;

/* 00524e5b...: kept between visits, as the original's globals. */
static RaceSettings settings;
static Unlocks unlocks;  /* from the unlock level in config.dat (0052ad40) */

static int in_rect(const Rect *r)
{
    return pointer_in_rect(r->x0, r->x1, r->y0, r->y1);
}

static int find(const Fsh *fsh, const char *name, FshEntry *e)
{
    uint32_t index;
    return fsh_find(fsh, name, &index) == FSH_OK && fsh_entry(fsh, index, e) == FSH_OK;
}

static void draw(const Central *c, const char *name, int transparent)
{
    FshEntry e;
    if (!find(&c->fsh, name, &e)) return;
    if (transparent) video_blit_transparent_at_own_position(&e);
    else video_blit_at_own_position(&e);
}

/* Panel art is control/<name>.qfs, entry "0000" at its own position; the
 * original reloads a panel only when its value changes. */
static void draw_panel(Panel *p, const char *name)
{
    if (strcmp(p->name, name) != 0) {
        size_t size;
        char path[48];
        resource_free(p->data);
        snprintf(p->name, sizeof p->name, "%s", name);
        snprintf(path, sizeof path, "control/%s.qfs", name);
        p->data = resource_load_art(path, &size);
        if (p->data == NULL || fsh_open(&p->fsh, p->data, size) != FSH_OK) {
            fprintf(stderr, "control_central: cannot load %s\n", path);
            resource_free(p->data);
            p->data = NULL;
        }
    }
    FshEntry e;
    if (p->data != NULL && find(&p->fsh, "0000", &e)) video_blit_at_own_position(&e);
}

static const char *track_art(void)
{
    if (settings.track < 9 && settings.variant_m) return TRACK_M[settings.track];
    if (settings.track < 9 && settings.variant_r) return TRACK_R[settings.track];
    return TRACK[settings.track];
}

static const char *opponent_art(void)
{
    if (settings.race_type != RACE_TOURNAMENT) return OPPONENT[settings.opponent];
    return settings.player_car < 3 ? "tourny3" : settings.player_car < 6 ? "tourny2" : "tourny1";
}

static void draw_focus(const Central *c)
{
    static const char BUTTON[FOCUS_COUNT][5] = { "", "", "", "", "Ra1l", "optl", "chal", "canl" };
    if (c->focus < PANELS) {
        draw(c, FRAME[c->focus], 1);
        if (c->arrow != ARROW_NONE) draw(c, ARROW[c->focus][c->arrow - 1], 1);
    } else if (c->focus < FOCUS_NONE) {
        draw(c, BUTTON[c->focus], 0);
    }
}

static void redraw(Central *c)
{
    video_clear_back();
    draw(c, "bgnd", 0);
    draw(c, "cham", 1);  /* chat dimmed: single player */
    draw_panel(&c->panels[0], track_art());
    /* TODO(0052b2d6): "flad" over tracks already won in the tournament. */
    draw_panel(&c->panels[1], opponent_art());
    draw_panel(&c->panels[2], PLAYER[settings.player_car]);
    draw_panel(&c->panels[3], RACE_TYPE[settings.race_type]);
    draw_focus(c);
    video_flip();
    pointer_show();
    if (c->fade_in) {
        video_fade_in(0, PALETTE_COLORS, c->palette);
        text_build_styles();
        c->fade_in = 0;
    }
    c->redraw = 0;
}

static int has_opponent(void)
{
    return settings.race_type == RACE_HEAD_TO_HEAD || settings.race_type == RACE_SINGLE;
}

static void set_focus(Central *c, int focus, int arrow)
{
    if (c->focus == focus && c->arrow == arrow) return;
    c->focus = focus;
    c->arrow = arrow;
    c->redraw = 1;  /* TODO(0046492c): move sound 0x47 */
}

static int hovered_arrow(int panel)
{
    if (in_rect(&LEFT_ARROW[panel])) return ARROW_LEFT;
    if (in_rect(&RIGHT_ARROW[panel])) return ARROW_RIGHT;
    return ARROW_NONE;
}

static void follow_pointer(Central *c)
{
    for (int panel = 0; panel < PANELS; panel++) {
        if (panel == FOCUS_OPPONENT && !has_opponent()) continue;
        if (in_rect(&PANEL_RECT[panel])) {
            set_focus(c, panel, hovered_arrow(panel));
            return;
        }
    }
    if (in_rect(&OPTIONS_RECT)) set_focus(c, FOCUS_OPTIONS, c->arrow);
    else if (in_rect(&CANCEL_RECT)) set_focus(c, FOCUS_CANCEL, c->arrow);
    else if (in_rect(&RACE_RECT)) set_focus(c, FOCUS_RACE, c->arrow);
    else set_focus(c, FOCUS_NONE, c->arrow);
}

static void move_focus(Central *c, int focus)
{
    pointer_move_to(CENTRE_X[focus], CENTRE_Y[focus]);
    set_focus(c, focus, ARROW_NONE);
}

static void step_panel(int panel, int dir)
{
    switch (panel) {
    case FOCUS_OPPONENT: opponent_step(&settings, dir); break;
    case FOCUS_PLAYER: player_car_step(&settings, &unlocks, dir); break;
    case FOCUS_LOCATION: track_step(&settings, &unlocks, dir); break;
    default: race_type_step(&settings, &unlocks, dir); break;
    }
}

static void cancel(Central *c)
{
    /* TODO(0052b310): "Quit tournament ?" once a tournament is under way. */
    c->done = 1;
    c->result = 0;
}

static void choose(Central *c)
{
    /* TODO(0046492c): select sound 0x48. */
    if (c->focus < PANELS) {
        if (c->arrow != ARROW_NONE) step_panel(c->focus, c->arrow == ARROW_RIGHT ? +1 : -1);
        /* TODO(00408724 showroom, 004687d0 track slides, 00461540 race type):
         * the panel bodies open the video submenus. */
        else fprintf(stderr, "control_central: submenu for panel %d not ported yet\n", c->focus);
        c->redraw = 1;
    } else if (c->focus == FOCUS_RACE) {
        /* TODO(00427ae0): the race itself. */
        fprintf(stderr, "control_central: race (00427ae0) not ported yet\n");
    } else if (c->focus == FOCUS_OPTIONS) {
        /* TODO(00444bb0): options screen. */
        fprintf(stderr, "control_central: options (00444bb0) not ported yet\n");
    } else if (c->focus == FOCUS_CANCEL) {
        cancel(c);
    }
    timer_set_deadline(IDLE_TIMEOUT);
}

static int key_up(int focus)
{
    const int no_opponent = !has_opponent();
    switch (focus) {
    case FOCUS_OPPONENT: return FOCUS_RACE;
    case FOCUS_PLAYER: return no_opponent ? FOCUS_RACE : FOCUS_OPPONENT;
    case FOCUS_LOCATION: case FOCUS_RACE_TYPE: case FOCUS_RACE: return focus - 1;
    case FOCUS_NONE: return FOCUS_PLAYER;
    default: return FOCUS_RACE_TYPE;
    }
}

static int key_down(int focus)
{
    if (focus <= FOCUS_RACE_TYPE) return focus + 1;
    if (focus == FOCUS_NONE) return FOCUS_PLAYER;
    return has_opponent() ? FOCUS_OPPONENT : FOCUS_PLAYER;
}

/* Left/Right step a panel's value, or cycle RACE -> OPTIONS -> CANCEL. */
static void key_sideways(Central *c, int dir)
{
    static const int NEXT[FOCUS_COUNT] = { 0, 0, 0, 0, FOCUS_OPTIONS, FOCUS_CANCEL, FOCUS_CANCEL, FOCUS_RACE };
    static const int PREV[FOCUS_COUNT] = { 0, 0, 0, 0, FOCUS_CANCEL, FOCUS_RACE, FOCUS_OPTIONS, FOCUS_OPTIONS };
    if (c->focus < PANELS) {
        step_panel(c->focus, dir);
        c->redraw = 1;
    } else if (c->focus < FOCUS_NONE) {
        move_focus(c, dir > 0 ? NEXT[c->focus] : PREV[c->focus]);
    }
}

static void handle_key(Central *c, int key)
{
    if (key == KEY_SPACE || key == KEY_ENTER || (c->buttons == POINTER_BUTTON_SELECT && c->button_armed)) {
        pointer_click_feedback();
        if (c->key_armed) choose(c);
        timer_ticks_since_last();
        c->button_armed = c->key_armed = c->held_ticks = 0;
    } else if (key == KEY_NONE || !c->key_armed) {
        if (key == KEY_NONE) c->key_armed = 1;
    } else {
        if (key == KEY_ESCAPE) cancel(c);
        if (key == KEY_UP) move_focus(c, key_up(c->focus));
        if (key == KEY_DOWN) move_focus(c, key_down(c->focus));
        if (key == KEY_LEFT) key_sideways(c, -1);
        if (key == KEY_RIGHT) key_sideways(c, +1);
        timer_set_deadline(IDLE_TIMEOUT);
        timer_ticks_since_last();
        c->key_armed = c->held_ticks = 0;
    }
}

static void update_repeat(Central *c, int key)
{
    if (!c->key_armed && key != KEY_ESCAPE && key != KEY_SPACE && key != KEY_ENTER) {
        c->held_ticks += timer_ticks_since_last();
        if (c->held_ticks > KEY_REPEAT_TICKS) c->key_armed = 1;
    }
}

static void check_idle(Central *c)
{
    if (c->done) return;
    if (timer_deadline_passed() && settings.race_type != RACE_TOURNAMENT) {
        c->done = 1;
        c->result = 2;  /* back to the intro (attract mode) */
    } else if (c->buttons == POINTER_BUTTON_BACK && c->button_armed) {
        if (dialog_yes_no(resource_german() ? "Spiel verlassen ?" : "Exit game ?")) cancel(c);
        c->button_armed = 0;
        c->redraw = 1;
    }
}

static void step(Central *c)
{
    follow_pointer(c);
    if (c->redraw) redraw(c);
    update_repeat(c, input_get_key());
    if (pointer_active()) c->buttons = pointer_read_buttons();
    if (c->buttons == POINTER_BUTTON_NONE) c->button_armed = 1;
    pointer_activate_if_moved();
    handle_key(c, input_get_key());
    /* TODO(0046417c): service streamed music. */
    check_idle(c);
}

static int load(Central *c)
{
    size_t size;
    FshEntry pal;
    c->data = resource_load_art("control/central.qfs", &size);
    return c->data != NULL && fsh_open(&c->fsh, c->data, size) == FSH_OK && find(&c->fsh, "!pal", &pal)
        && fsh_palette_rgb(&pal, c->palette) == FSH_OK;
}

static void unload(Central *c)
{
    for (int i = 0; i < PANELS; i++) resource_free(c->panels[i].data);
    resource_free(c->data);
}

/* 00432ff4: the race setup comes from config.dat (0042a010) and the unlocks
 * from its unlock level; the track variants always start off. */
static void load_settings(void)
{
    ConfigDat *config = config_current();
    if (!config_dat_get_race(config, &settings)) {
        ConfigDat defaults;
        config_dat_defaults(&defaults, resource_german());
        config_dat_get_race(&defaults, &settings);
    }
    settings.variant_m = settings.variant_r = 0;
    unlocks = config_dat_unlocks(config);
    if (settings.track == TRACK_LOST_VEGAS && !unlocks.lost_vegas) settings.track = 0;
}

int control_central(void)
{
    Central c = { 0 };
    load_settings();
    if (settings.player_car == CAR_WARRIOR && !unlocks.warrior) settings.player_car = 0;
    c.focus = FOCUS_PLAYER;
    c.redraw = c.fade_in = c.key_armed = c.button_armed = 1;
    pointer_move_to(CENTRE_X[c.focus], CENTRE_Y[c.focus]);
    if (!load(&c)) {
        fprintf(stderr, "control_central: cannot load control/central.qfs\n");
        unload(&c);
        return 0;
    }
    timer_set_deadline(IDLE_TIMEOUT);
    while (!c.done) step(&c);
    video_fade_out(0, PALETTE_COLORS);
    video_set_overlay(NULL, 0, 0);
    unload(&c);
    config_dat_set_race(config_current(), &settings);
    return c.result;
}
