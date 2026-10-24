/* The menus' music on the audio screen's SONG. The exe's pick (004562e8) and the front end's after the intro (0045638c)
 * play a random track of the chosen song's half (4..9 a rock, 10..15 a techno), so a chosen song never played in the
 * menus. With port_menu_song the chosen one plays; 1..3 (the shuffles) and 0 stay the exe's. */
#include "game/race/race_sound.h"
#include "test.h"

typedef struct {
    int played, track, rand_calls;
} Fake;

static int32_t rand_below(void *ctx, int32_t n)
{
    Fake *f = ctx;
    f->rand_calls++;
    return n - 1;
}
static void play(void *ctx, int32_t track)
{
    Fake *f = ctx;
    f->played++, f->track = track;
}

static Fake pick(int16_t music, int32_t port)
{
    Fake f = { 0, -1, 0 };
    const RaceMusicOps ops = { &f, rand_below, play };
    int16_t small = 1;
    race_music_menu_pick(&ops, music, &small, port);
    CHECK_EQ_INT(small, 0);
    return f;
}

static Fake by_idle(int16_t music, int32_t idle, int32_t port)
{
    Fake f = { 0, -1, 0 };
    const RaceMusicOps ops = { &f, rand_below, play };
    race_music_menu_by_idle(&ops, music, idle, port);
    return f;
}

static void test_a_chosen_song_plays_in_the_menus(void)
{
    for (int16_t music = 4; music <= 15; music++) {
        const Fake f = pick(music, 1);
        CHECK_EQ_INT(f.played, 1);
        CHECK_EQ_INT(f.track, music - 4);
        CHECK_EQ_INT(f.rand_calls, 0);
        const Fake g = by_idle(music, 3, 1);
        CHECK_EQ_INT(g.played, 1);
        CHECK_EQ_INT(g.track, music - 4);
    }
    /* the user's: song 9 (Powo) is rock6, also track 5, which 0045638c never plays */
    CHECK_EQ_INT(by_idle(9, 0, 1).track, 5);
}

static void test_the_exe_picks_from_the_half(void)
{
    const Fake rock = pick(9, 0);
    CHECK_EQ_INT(rock.rand_calls, 1);
    CHECK_EQ_INT(rock.track, 5);    /* rand_below(6) = 5: any rock, not the one chosen */
    const Fake techno = pick(10, 0);
    CHECK_EQ_INT(techno.track, 11);
    CHECK_EQ_INT(by_idle(4, 7, 0).track, 1);   /* idle % 6 */
}

static void test_the_shuffles_and_none_stay_the_exe(void)
{
    CHECK_EQ_INT(pick(1, 1).rand_calls, 1);
    CHECK_EQ_INT(pick(1, 1).track, 11);
    CHECK_EQ_INT(pick(2, 1).track, 5);
    CHECK_EQ_INT(pick(3, 1).track, 11);
    CHECK_EQ_INT(pick(0, 1).played, 0);
    CHECK_EQ_INT(by_idle(0, 3, 1).played, 0);
    CHECK_EQ_INT(by_idle(1, 13, 1).track, 1);
    CHECK_EQ_INT(pick(16, 1).played, 0);
}

int main(void)
{
    test_a_chosen_song_plays_in_the_menus();
    test_the_exe_picks_from_the_half();
    test_the_shuffles_and_none_stay_the_exe();
    return TEST_REPORT();
}
