/* The race's pause menu (race_menu.c) with its art missing, refused (shape_bank_valid) or without its '!pal': the menu opens and
 * closes on the palette it has, where it read the palette from no shape (a mod's pause.fsh without one crashed the race) */
#include "tests/test.h"

#include "game/race/race_menu.h"

#include <string.h>

static uint8_t *art;   /* what load gives for every bank */
static int loads, blits;

static void call(void *c, uint32_t a) { (void)c, (void)a; }
static void sounds(void *c, RaceMenuSounds w) { (void)c, (void)w; }
static int16_t zero16(void *c) { (void)c; return 0; }
static void fill(void *c, uint32_t a, uint8_t *p) { (void)c, (void)a, (void)p; }
static void fade(void *c, int32_t a, int32_t b) { (void)c, (void)a, (void)b; }
static void set_palette(void *c, uint32_t a, const uint8_t *p) { (void)c, (void)a, (void)p; }
static void mode(void *c, int32_t a, int32_t b, int32_t d) { (void)c, (void)a, (void)b, (void)d; }
static void clip(void *c, int32_t a, int32_t b, int32_t d, int32_t e) { (void)c, (void)a, (void)b, (void)d, (void)e; }
static void surface_free(void *c, uint32_t s) { (void)c, (void)s; }
static uint32_t surface_new(void *c, int32_t w, int32_t h) { (void)c, (void)w, (void)h; return 1; }
static void surface(void *c, uint32_t s) { (void)c, (void)s; }
static void pointer_to(void *c, int16_t x, int16_t y) { (void)c, (void)x, (void)y; }
static void music(void *c, const char *n) { (void)c, (void)n; }
static uint8_t *load(void *c, const char *n) { (void)c, (void)n, loads++; return art; }
static uint32_t load_inventory(void *c, const char *n) { (void)c, (void)n; return 0; }
static void unload(void *c, uint8_t *b) { (void)c, (void)b; }
static void palette(void *c, uint8_t *to, const uint8_t *from, int16_t n) { (void)c, (void)n, memcpy(to, from, 16); }
static void number(void *c, uint32_t a, int32_t v) { (void)c, (void)a, (void)v; }
static int32_t inside(void *c, int16_t a, int16_t b, int16_t d, int16_t e) { (void)c, (void)a, (void)b, (void)d, (void)e; return 0; }
static void blit_own(void *c, const uint8_t *s) { (void)c, blits += s != NULL; }
static void blit_at(void *c, const uint8_t *s, int32_t x, int32_t y) { (void)c, (void)x, (void)y, blits += s != NULL; }
static int32_t ticks(void *c) { (void)c; return 0; }
static uint16_t key(void *c) { (void)c; return 0; }
static int16_t ask(void *c, const char *q) { (void)c, (void)q; return 0; }
static void notice(void *c, const char *t, int32_t n) { (void)c, (void)t, (void)n; }
static void net_send(void *c, int32_t to) { (void)c, (void)to; }
static int32_t net_wait(void *c, int32_t w) { (void)c, (void)w; return 1; }
static void take(void *c, int32_t k) { (void)c, (void)k; }
static void view_screen(void *c, int32_t a, int32_t b) { (void)c, (void)a, (void)b; }

static const RaceMenuOps OPS = { NULL, call, sounds, zero16, fill, fade, set_palette, mode, clip, surface_free, surface_new, surface,
                                 surface, pointer_to, music, load, load_inventory, unload, palette, number, inside, zero16,
                                 blit_own, blit_at, zero16, ticks, key, ask, notice, net_send, net_wait, take, view_screen };

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

/* the menu opened and left at once (the player dropped from the game) */
static void open_and_close(void)
{
    static RaceMenuWorld w;
    memset(&w, 0, sizeof w);
    w.dropped = 1;
    loads = blits = 0;
    race_menu(&w, &OPS);
    CHECK(loads >= 2);   /* the art and the map */
}

/* no art at all: as when the file is missing or refused */
static void test_no_art(void)
{
    art = NULL;
    open_and_close();
}

/* art with its 'bgnd' and no '!pal' */
static void test_art_without_palette(void)
{
    static uint8_t bank[0x40];
    memset(bank, 0, sizeof bank);
    memcpy(bank, "SHPI", 4), put32(bank + 4, sizeof bank), put32(bank + 8, 1), memcpy(bank + 12, "GIMX", 4);
    memcpy(bank + 16, "bgnd", 4), put32(bank + 20, 0x18);
    bank[0x18] = 0x7b, bank[0x1c] = 2, bank[0x1e] = 2;   /* a 2 x 2 bitmap */
    art = bank;
    open_and_close();
    CHECK(blits > 0);   /* its background still drawn */
}

int main(void)
{
    test_no_art();
    test_art_without_palette();
    return TEST_REPORT();
}
