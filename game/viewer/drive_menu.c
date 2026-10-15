/* --drive's in-race menu (drive_menu.h). */
#include "viewer/drive_menu.h"

#include "viewer/drive_audio.h"
#include "game/frontend/dialog.h"
#include "game/gfx/blit.h"
#include "game/platform/platform.h"
#include "game/race/race_camera.h"
#include "game/race/race_chase.h"
#include "game/race/race_menu.h"
#include "game/race/race_sound.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 004bb8dc is counted by the master timer's callback 0043b9e8 (TIMER_HZ) */
enum { KEY_ESC_SCANCODE = 1, TICK_HZ = TIMER_HZ, IDLE_MS = 1, RAW_PALETTE = 0x24, MENU_SURFACE = 1 };

typedef struct {
    DriveMenu *m;
    RaceMenuWorld *w;
    uint8_t *pixels;            /* 00524834: the menu's surface, VIDEO_WIDTH x VIDEO_HEIGHT */
    uint32_t clock_ms;          /* where 004bb8dc's ticks were last counted */
} MenuCtx;

/* Ticks at TICK_HZ since *since (moved on by the ticks counted) */
static int32_t ticks_since(uint32_t *since)
{
    const uint32_t now = platform_ticks_ms();
    const int32_t ticks = (int32_t)((now - *since) * TICK_HZ / 1000u);
    *since += (uint32_t)ticks * 1000u / TICK_HZ;
    return ticks;
}

/* The window's events read; closed, the process ends as the frontend's loops end it (input.c) */
static void poll(DriveMenu *m)
{
    if (platform_poll_events()) return;
    drive_menu_close(m);
    drive_audio_close();
    platform_shutdown();
    exit(EXIT_SUCCESS);
}

/* 004c693d: Esc's state as the keyboard's handler keeps it */
static uint8_t esc_down(void) { return platform_key_down(KEY_ESC_SCANCODE) ? 0x80 : 0; }

static QuadTarget target(const MenuCtx *c)
{
    return (QuadTarget){ c->pixels, VIDEO_WIDTH, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
}

static void call(void *ctx, uint32_t address)
{
    MenuCtx *c = ctx;
    RaceState *race = c->m->race;
    switch (address) {
    case 0x4104d0:  /* the loop's network poll; the race's clock 004bb8dc and the keyboard's 004c693d run on */
        drive_audio_pump();
        poll(c->m);
        c->w->esc_down = esc_down();
        c->w->blink_clock += ticks_since(&c->clock_ms);
        platform_sleep_ms(IDLE_MS);
        break;
    case 0x46417c:  /* the keys read again */
        poll(c->m);
        c->w->esc_down = esc_down();
        platform_sleep_ms(IDLE_MS);
        break;
    case 0x4c618c: video_clear_back(); break;
    case 0x4733ec: video_flip(); break;
    case 0x474558: video_set_overlay(NULL, 0, 0); break;  /* the pointer hidden */
    case 0x441798: pointer_show(); break;
    case 0x441814: pointer_click_feedback(); break;
    case 0x464088: pointer_activate_if_moved(); break;
    case 0x41e364: drive_hud_clear(c->m->hud); break;
    case 0x443c34: race_sound_channels_stop(race); break;
    case 0x443820: race_sound_start_voices(race); break;
    case 0x444394: if (race->sounds.start_engine != NULL) race->sounds.start_engine(race->sounds.user); break;
    case 0x44435c: if (race->sounds.stop_engine != NULL) race->sounds.stop_engine(race->sounds.user); break;
    case 0x43cbcc: race_forces_disable(race); break;
    case 0x43cbe0: race_forces_enable(race); break;
    case 0x43cbf4: race_forces_clear(race); break;
    case 0x4557e8: c->m->keys->last = race->tick; break;  /* 00513e54 = 00513e70 */
    default: break;  /* the menu's own sounds and music, the network and the screen modes: not in --drive */
    }
}

static void sounds(void *ctx, RaceMenuSounds which) { (void)ctx, (void)which; }
static int16_t kept(void *ctx) { return (void)ctx, 0; }
static void fill(void *ctx, uint32_t address, uint8_t *palette) { (void)ctx, (void)address, memset(palette, 0, RACE_MENU_PALETTE); }
static void fade(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }

/* 00463e7c fades the menu's palette in (shown at once here); 00476460's are the race's, which --drive keeps */
static void set_palette(void *ctx, uint32_t address, const uint8_t *palette)
{
    (void)ctx;
    if (address == 0x463e7c) video_set_palette(0, RACE_MENU_PALETTE / 3, palette);
}

static void mode(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static void clip(void *ctx, int32_t l, int32_t t, int32_t r, int32_t b) { (void)ctx, (void)l, (void)t, (void)r, (void)b; }
static void surface_free(void *ctx, uint32_t s) { (void)ctx, (void)s; }
static uint32_t surface_new(void *ctx, int32_t w, int32_t h) { return (void)ctx, (void)w, (void)h, MENU_SURFACE; }
static void select_surface(void *ctx, uint32_t s) { (void)ctx, (void)s; }

static void present(void *ctx, uint32_t s)
{
    (void)s, memcpy(video_back_buffer(), ((MenuCtx *)ctx)->pixels, VIDEO_WIDTH * VIDEO_HEIGHT);
}

static void pointer_to(void *ctx, int16_t x, int16_t y) { (void)ctx, pointer_move_to(x, y); }
static void music(void *ctx, const char *name) { (void)ctx, (void)name; }

static uint8_t *load(void *ctx, const char *name)
{
    size_t size;
    return (void)ctx, resource_load_art(name, &size);
}

static uint32_t load_inventory(void *ctx, const char *name) { return (void)ctx, (void)name, 0; }
static void unload(void *ctx, uint8_t *bank) { (void)ctx, resource_free(bank); }

/* 00463e08(to, 0, 0x100, from, kind): kind 0x24 copied, else each 6-bit value shifted to 8 */
static void palette(void *ctx, uint8_t *to, const uint8_t *from, int16_t count)
{
    (void)ctx;
    for (int i = 0; i < RACE_MENU_PALETTE; i++) to[i] = count == RAW_PALETTE ? from[i] : (uint8_t)(from[i] << 2);
}

/* 0044f534: the replay's request (00513738); the option screens and the sounds are not here */
static void number(void *ctx, uint32_t address, int32_t value)
{
    MenuCtx *c = ctx;
    if (address == 0x44f534 && c->m->replay != NULL && c->m->replay->recording != NULL) c->m->replay->dir.request = value;
}

static int32_t inside(void *ctx, int16_t l, int16_t r, int16_t t, int16_t b)
{
    return (void)ctx, pointer_active() ? pointer_in_rect(l, r, t, b) : 0;
}

static int16_t chat(void *ctx) { return (void)ctx, 0; }
static void blit_own(void *ctx, const uint8_t *shape)
{
    const QuadTarget t = target(ctx);
    if (shape != NULL) blit_shape_placed(&t, shape);
}

/* 00473ad0: 0xff skipped, at x, y less the hot spot (the words at +8, +0xa) */
static void blit_at(void *ctx, const uint8_t *shape, int32_t x, int32_t y)
{
    const QuadTarget t = target(ctx);
    if (shape == NULL) return;
    const int16_t hx = (int16_t)(shape[8] | shape[9] << 8), hy = (int16_t)(shape[10] | shape[11] << 8);
    blit_shape_at_transparent(&t, shape, x - hx, y - hy);
}

static int16_t buttons(void *ctx) { return (void)ctx, (int16_t)pointer_read_buttons(); }
static int32_t ticks(void *ctx) { return (void)ctx, timer_ticks_since_last(); }
static uint16_t key(void *ctx) { return (void)ctx, (uint16_t)input_get_key(); }
static int16_t ask(void *ctx, const char *question) { return (void)ctx, (int16_t)dialog_yes_no(question); }
static void notice(void *ctx, const char *text, int32_t time) { (void)ctx, (void)text, (void)time; }
static void net_send(void *ctx, int32_t to) { (void)ctx, (void)to; }
static int32_t net_wait(void *ctx, int32_t what) { return (void)ctx, (void)what, 1; }

static void take(void *ctx, int32_t kind)
{
    RaceState *race = ((MenuCtx *)ctx)->m->race;
    race_chase_take(race, &race->chase, kind);
}

static void view_screen(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }

static void put16(uint8_t *b, int at, int16_t v) { b[at] = (uint8_t)v, b[at + 1] = (uint8_t)((uint16_t)v >> 8); }

static void world_from_race(const DriveMenu *m, RaceMenuWorld *w)
{
    const RaceState *race = m->race;
    put16(w->settings, RACE_MENU_MUSIC, drive_audio_music_setting());
    put16(w->settings, RACE_MENU_TRACK, m->track);
    put16(w->settings, RACE_MENU_VARIANT, m->variant);
    memcpy(w->saved, w->settings, RACE_MENU_SETTINGS);
    w->high = 1;
    w->race_type = race->race_type, w->player = race->player;
    for (int k = 0; k < RACE_MENU_CARS; k++) w->car_node[k] = race->cars[k] != NULL ? race->cars[k]->node : 0;
    if (race->cars[race->player] != NULL) w->player_unknown_512 = race->cars[race->player]->physics;
    w->view_surface = MENU_SURFACE;
    w->playback = race->playback, w->dropped = race->dropped, w->german = (int16_t)resource_german();
    w->mouse_on = (int16_t)pointer_active();
    w->esc_down = esc_down();
    w->end_state = race->end_state, w->unknown_4fbb0c = race->unknown_4fbb0c;
    w->follow = race->chase.kind, w->phase = race->phase;
    w->unknown_513e50 = m->keys->pending;
}

void drive_menu_run(void *menu)
{
    DriveMenu *m = menu;
    if (!m->ready || m->track < 0) return;
    static uint8_t pixels[VIDEO_WIDTH * VIDEO_HEIGHT];
    RaceMenuWorld w = { 0 };
    world_from_race(m, &w);
    MenuCtx c = { m, &w, pixels, platform_ticks_ms() };
    const RaceMenuOps ops = { &c, call, sounds, kept, fill, fade, set_palette, mode, clip, surface_free, surface_new,
                              select_surface, present, pointer_to, music, load, load_inventory, unload, palette, number,
                              inside, chat, blit_own, blit_at, buttons, ticks, key, ask, notice, net_send, net_wait,
                              take, view_screen };
    race_menu(&w, &ops);
    m->race->dropped = w.dropped, m->race->end_state = w.end_state, m->keys->pending = w.unknown_513e50;
}

/* The map by the track's stem: CY / CL / AL 1..3 the tracks 0..2 and their variants; TR1, TR2, TR6, TR3, TR7
 * the loops 3..7 ("loop1", "loop2", "loop6", "loop3", "loop7"), TR4 taken as "loopx" (8, Lost Vegas) */
static void track_of(const char *name, int16_t *track, int16_t *variant)
{
    static const char *const CITIES[3] = { "CY", "CL", "AL" };
    static const char LOOPS[] = "126374";
    char up[3] = { (char)toupper((unsigned char)name[0]), name[0] ? (char)toupper((unsigned char)name[1]) : 0, 0 };
    const char digit = name[0] && name[1] ? name[2] : 0;
    *track = -1, *variant = 0;
    for (int16_t k = 0; k < 3; k++)
        if (strcmp(up, CITIES[k]) == 0 && digit >= '1' && digit <= '3') *track = k, *variant = (int16_t)(digit - '1');
    if (strcmp(up, "TR") == 0 && digit != 0 && strchr(LOOPS, digit) != NULL) *track = (int16_t)(3 + (strchr(LOOPS, digit) - LOOPS));
}

int drive_menu_open(DriveMenu *m, const char *data_root, const char *name)
{
    track_of(name, &m->track, &m->variant);
    timer_init();
    if (!video_init()) return 0;
    resource_init(data_root, 0);
    if (!pointer_load(0) || !text_init()) {
        drive_menu_close(m);
        return 0;
    }
    m->ready = 1;
    return 1;
}

void drive_menu_close(DriveMenu *m)
{
    text_free();
    pointer_free();
    video_shutdown();
    m->ready = 0;
}
