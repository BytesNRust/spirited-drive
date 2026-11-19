/* The games box of the network screen (game/frontend/net_host.c) when the network service did not start (its UDP port taken, say):
 * 00442684 gives NULL, as the original's does after its "BUG: init network service FAILED". The box is not opened, nothing is
 * hosted or joined, the pictures it saved are freed and the screen is left (0): it read the NULL's count and crashed. */
#include "game/frontend/net_host.h"

#include "tests/test.h"

#include <stddef.h>
#include <stdint.h>

static int made, freed, games_asked, hosted, connected;

static int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)w, (void)h, (void)flags; return 100 + made++; }
static void select_picture(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static int32_t picture_shape(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void save(void *ctx, int32_t shape, int32_t x, int32_t y) { (void)ctx, (void)shape, (void)x, (void)y; }
static RaceNetGames *games(void *ctx, int32_t timeout) { (void)ctx, (void)timeout; games_asked++; return NULL; }
static void picture_free(void *ctx, int32_t picture) { (void)ctx, (void)picture; freed++; }
static int32_t host(void *ctx, const char *name) { (void)ctx, (void)name; hosted++; return 1; }
static int32_t connect_to(void *ctx, const uint8_t *address) { (void)ctx, (void)address; connected++; return 1; }

int main(void)
{
    /* every operation the box would use once open is left NULL: calling one is a crash, not a pass */
    NetHostOps ops = { 0 };
    ops.picture_new = picture_new, ops.select = select_picture, ops.picture_shape = picture_shape, ops.save = save;
    ops.games = games, ops.picture_free = picture_free, ops.host = host, ops.connect = connect_to;
    NetHostState st = { 0 };
    st.name = "R";
    CHECK_EQ_INT(net_host_join(&st, &ops), 0);
    CHECK_EQ_INT(games_asked, 1);
    CHECK_EQ_INT(made, 2);
    CHECK_EQ_INT(freed, 2);   /* both pictures it saved, none left */
    CHECK_EQ_INT(hosted, 0);
    CHECK_EQ_INT(connected, 0);
    return test_failures == 0 ? 0 : 1;
}
