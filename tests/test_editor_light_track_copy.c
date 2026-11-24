/* The Track light panel's Copy to... model (editor/light_track_copy.h): the plan for each destination time (add, same, clash, overlaps), the apply (replace or skip, all or nothing at the game's
 * limits), and that nothing else in the file moves. */
#include "light_track_copy.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 1200 };

static const char FILE_TEXT[] =
    "track CY1\n"
    "time A\n"
    "  brightness 852 943 60 left\n"
    "  brightness 10 20 50\n"
    "  tone 30 40 ffe080:50\n"
    "  lamp 130 150 ff2000:80 8 left\n"
    "  lamp 140 100\n"
    "time B\n"
    "  brightness 852 943 60 left\n"
    "  tone 30 40 ff0000:50\n"
    "  lamp 130 100\n"
    "time C\n"
    "  brightness 852 943 80 left\n"
    "  brightness 900 950 70 left\n"
    "  lamp 130 90 left\n"
    "time N\n";

static void open_file(const char *text, LightTrack *f)
{
    char why[LT_WHY];
    CHECK(light_track_parse(text, strlen(text), NODES, f, why, sizeof why));
}

static int is(const LightTrack *f, const char *want)
{
    size_t n;
    char *t = light_track_format(f, &n);
    const int eq = t != NULL && strcmp(t, want) == 0;
    if (!eq) printf("--- got\n%s--- want\n%s---\n", t, want);
    free(t);
    return eq;
}

static char *bytes(const LightTrack *f)
{
    size_t n;
    return light_track_format(f, &n);
}

static const unsigned OTHERS = 0xe;   /* B, C and N from A */

static void brightness_plan_and_apply(void)
{
    LightTrack f;
    open_file(FILE_TEXT, &f);
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    CHECK(light_track_copy_plan(&f, LT_BRIGHTNESS, LT_TIME_A, 0, 0xf, &plan, why, sizeof why));
    CHECK_EQ_INT(plan.to[LT_TIME_A].action, LT_COPY_NONE);   // its own time is left out
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_SAME);
    CHECK_EQ_INT(plan.to[LT_TIME_C].action, LT_COPY_CLASH);
    CHECK_EQ_INT(plan.to[LT_TIME_C].clash, 0);
    CHECK_EQ_INT(plan.to[LT_TIME_N].action, LT_COPY_ADD);
    char *before = bytes(&f);
    LightTrackCopyResult r;
    // Skip: the clash stays, the add happens, the same is nothing
    CHECK(light_track_copy_apply(&f, &plan, 0, &r, why, sizeof why));
    CHECK(r.added == 1 && r.replaced == 0 && r.same == 1 && r.skipped == 1);
    CHECK(is(&f, "track CY1\ntime A\n  brightness 852 943 60 left\n  brightness 10 20 50\n  tone 30 40 ffe080:50\n  lamp 130 150 ff2000:80 8 left\n  lamp 140 100\n"
                 "time B\n  brightness 852 943 60 left\n  tone 30 40 ff0000:50\n  lamp 130 100\ntime C\n  brightness 852 943 80 left\n  brightness 900 950 70 left\n  lamp 130 90 left\n"
                 "time N\n  brightness 852 943 60 left\n"));
    // Replace: the clash takes the value, in its own place
    CHECK(light_track_copy_plan(&f, LT_BRIGHTNESS, LT_TIME_A, 0, OTHERS, &plan, why, sizeof why));
    CHECK_EQ_INT(plan.to[LT_TIME_N].action, LT_COPY_SAME);
    CHECK(light_track_copy_apply(&f, &plan, 1, &r, why, sizeof why));
    CHECK(r.added == 0 && r.replaced == 1 && r.same == 2 && r.skipped == 0);
    char *after = bytes(&f);
    CHECK(strstr(after, "time C\n  brightness 852 943 60 left\n  brightness 900 950 70 left\n") != NULL);
    free(before), free(after);
    light_track_free(&f);
}

static void a_time_without_a_block_and_overlaps(void)
{
    static const char TEXT[] = "track CY1\ntime A\n  brightness 852 943 60 left\ntime C\n  brightness 900 950 70 left\n  brightness 852 943 60 right\n";
    LightTrack f;
    open_file(TEXT, &f);
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    CHECK(light_track_copy_plan(&f, LT_BRIGHTNESS, LT_TIME_A, 0, OTHERS, &plan, why, sizeof why));
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_ADD);   // no block: the block is made
    CHECK_EQ_INT(plan.to[LT_TIME_B].overlaps, 0);
    CHECK_EQ_INT(plan.to[LT_TIME_C].action, LT_COPY_ADD);
    CHECK_EQ_INT(plan.to[LT_TIME_C].overlaps, 1);           // 900..950 left shares nodes and points; the same nodes on the right do not
    CHECK(light_track_copy_apply(&f, &plan, 0, NULL, why, sizeof why));
    // the existing lines are not moved except by the order of their first node (the right strip one passes the left one it shares no point with), the new one stays last behind the one it overlaps,
    // and each block that was missing is made at the end
    CHECK(is(&f, "track CY1\ntime A\n  brightness 852 943 60 left\ntime C\n  brightness 852 943 60 right\n  brightness 900 950 70 left\n  brightness 852 943 60 left\n\ntime B\n  brightness 852 943 60 left\n\ntime N\n  brightness 852 943 60 left\n"));
    CHECK_EQ_INT(light_track_count(&f, LT_TIME_B, LT_BRIGHTNESS), 1);
    CHECK_EQ_INT(light_track_count(&f, LT_TIME_N, LT_BRIGHTNESS), 1);
    CHECK_EQ_INT(light_track_count(&f, LT_TIME_C, LT_BRIGHTNESS), 3);
    CHECK(light_track_first_bad(&f) == 0);
    light_track_free(&f);
}

static void tones_and_lamps(void)
{
    LightTrack f;
    open_file(FILE_TEXT, &f);
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    // a tone: same key (nodes and strip), another colour in B is a clash; C and N have none
    CHECK(light_track_copy_plan(&f, LT_TONE, LT_TIME_A, 0, OTHERS, &plan, why, sizeof why));
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_CLASH);
    CHECK_EQ_INT(plan.to[LT_TIME_C].action, LT_COPY_ADD);
    CHECK_EQ_INT(plan.to[LT_TIME_N].action, LT_COPY_ADD);
    CHECK(light_track_copy_apply(&f, &plan, 1, NULL, why, sizeof why));
    LightTrackTone t;
    CHECK(light_track_get_tone(&f, LT_TIME_B, 0, &t) && strcmp(t.colour, "ffe080") == 0 && t.percent == 50);
    CHECK(light_track_get_tone(&f, LT_TIME_C, 0, &t) && t.from == 30 && strcmp(t.colour, "ffe080") == 0);
    CHECK(light_track_get_tone(&f, LT_TIME_N, 0, &t) && t.to == 40);
    // a lamp with a strip: another strip or none at the same node is another lamp (added); the same strip with another value is a clash
    CHECK(light_track_copy_plan(&f, LT_LAMP, LT_TIME_A, 0, OTHERS, &plan, why, sizeof why));   // lamp 130 left, 80 % red radius 8
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_ADD);   // B's lamp 130 has no strip
    CHECK_EQ_INT(plan.to[LT_TIME_C].action, LT_COPY_CLASH);   // C's lamp 130 left, 90 %
    CHECK_EQ_INT(plan.to[LT_TIME_N].action, LT_COPY_ADD);
    CHECK(light_track_copy_apply(&f, &plan, 1, NULL, why, sizeof why));
    LightTrackLamp l;
    CHECK_EQ_INT(light_track_count(&f, LT_TIME_B, LT_LAMP), 2);
    CHECK(light_track_get_lamp(&f, LT_TIME_C, 0, &l) && l.percent == 150 && l.has_colour && l.has_radius && l.radius == 8 && l.has_strip && l.mask == LT_STRIP_LEFT);
    // a lamp with no strip: the same node with no strip is the same key
    CHECK(light_track_copy_plan(&f, LT_LAMP, LT_TIME_A, 1, OTHERS, &plan, why, sizeof why));   // lamp 140 100
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_ADD);
    CHECK(light_track_copy_apply(&f, &plan, 0, NULL, why, sizeof why));
    CHECK(light_track_copy_plan(&f, LT_LAMP, LT_TIME_A, 1, OTHERS, &plan, why, sizeof why));
    CHECK(plan.to[LT_TIME_B].action == LT_COPY_SAME && plan.to[LT_TIME_C].action == LT_COPY_SAME && plan.to[LT_TIME_N].action == LT_COPY_SAME);
    CHECK(light_track_first_bad(&f) == 0);
    light_track_free(&f);
}

static void the_games_limits_are_all_or_nothing(void)
{
    char text[8192] = "track CY1\ntime A\n  brightness 5 6 50\ntime C\n";
    for (int i = 0; i < LT_MAX_PER_TIME; i++) snprintf(text + strlen(text), sizeof text - strlen(text), "  brightness %d %d 50\n", 100 + i, 100 + i);
    LightTrack f;
    open_file(text, &f);
    CHECK_EQ_INT(light_track_count(&f, LT_TIME_C, LT_BRIGHTNESS), LT_MAX_PER_TIME);
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    CHECK(light_track_copy_plan(&f, LT_BRIGHTNESS, LT_TIME_A, 0, OTHERS, &plan, why, sizeof why));
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_ADD);
    CHECK_EQ_INT(plan.to[LT_TIME_C].action, LT_COPY_ADD);
    char *before = bytes(&f);
    LightTrackCopyResult r = { 7, 7, 7, 7 };
    CHECK(!light_track_copy_apply(&f, &plan, 1, &r, why, sizeof why));
    CHECK(strncmp(why, "Not copied to Dusk: ", 20) == 0);
    char *after = bytes(&f);
    CHECK(strcmp(before, after) == 0);   // B, which would have fitted, is untouched too
    CHECK_EQ_INT(r.added, 7);            // the result is left alone on a refusal
    free(before), free(after);
    light_track_free(&f);
}

static void bad_asks(void)
{
    LightTrack f;
    open_file(FILE_TEXT, &f);
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    CHECK(!light_track_copy_plan(&f, LT_SUN, LT_TIME_A, 0, OTHERS, &plan, why, sizeof why) && why[0] != '\0');
    CHECK(!light_track_copy_plan(&f, LT_BRIGHTNESS, LT_TIME_A, 9, OTHERS, &plan, why, sizeof why));
    CHECK(!light_track_copy_plan(&f, LT_BRIGHTNESS, 7, 0, OTHERS, &plan, why, sizeof why));
    CHECK(light_track_copy_plan(&f, LT_BRIGHTNESS, LT_TIME_A, 0, 0, &plan, why, sizeof why));   // no times: nothing to do
    CHECK_EQ_INT(plan.to[LT_TIME_B].action, LT_COPY_NONE);
    char *before = bytes(&f);
    CHECK(light_track_copy_apply(&f, &plan, 1, NULL, why, sizeof why));
    char *after = bytes(&f);
    CHECK(strcmp(before, after) == 0);
    free(before), free(after);
    light_track_free(&f);
}

static const char TUNNELS[] =
    "track CY1\n"
    "tunnel 100 200\n"
    "tunnel 300 400 lit\n"
    "time A\n"
    "  tunnel_look 100\n"
    "  tunnel_look 100 200 50 lamps 80\n"
    "  tunnel_look 120 150 70\n"
    "time B\n"
    "  tunnel_look 100 200 60\n"
    "time C\n"
    "  tunnel_look 300 400 lamps 90\n"
    "time N\n";

static void the_tunnels_rows(void)
{
    LightTrack f;
    open_file(TUNNELS, &f);
    LightTrackLookRow rows[8];
    const int n = light_track_look_rows(&f, rows, 8);
    CHECK_EQ_INT(n, 3);
    CHECK(rows[0].tunnel == 0 && rows[0].from == 100 && rows[0].to == 200 && !rows[0].lit);
    CHECK(rows[0].look[LT_TIME_A] == 1 && rows[0].look[LT_TIME_B] == 0 && rows[0].look[LT_TIME_C] == -1 && rows[0].look[LT_TIME_N] == -1);
    CHECK(rows[1].tunnel == 1 && rows[1].lit && rows[1].look[LT_TIME_C] == 0 && rows[1].look[LT_TIME_A] == -1);
    CHECK(rows[2].tunnel == -1 && rows[2].from == 120 && rows[2].to == 150 && rows[2].look[LT_TIME_A] == 2 && rows[2].look[LT_TIME_B] == -1);   // only a look, only at Dawn
    CHECK_EQ_INT(light_track_look_rows(&f, rows, 2), 2);   // the room given is kept
    int def[LT_TIMES];
    light_track_default_looks(&f, def);
    CHECK(def[LT_TIME_A] == 0 && def[LT_TIME_B] == -1 && def[LT_TIME_C] == -1 && def[LT_TIME_N] == -1);
    light_track_free(&f);
}

static void move_and_remove_a_tunnel_in_every_time(void)
{
    LightTrack f;
    open_file(TUNNELS, &f);
    char why[LT_WHY];
    char *before = bytes(&f);
    // widening: the tunnel and its looks at Dawn and Day move together
    LightTrackTunnel t = { 100, 210, 0, 0, 0 };
    CHECK(light_track_move_tunnel(&f, 0, &t, why, sizeof why));
    char *after = bytes(&f);
    CHECK(strstr(after, "tunnel 100 210\n") != NULL && strstr(after, "  tunnel_look 100 210 50 lamps 80\n") != NULL && strstr(after, "time B\n  tunnel_look 100 210 60\n") != NULL);
    CHECK(strstr(after, "  tunnel_look 120 150 70\n") != NULL && strstr(after, "tunnel_look 300 400 lamps 90") != NULL);   // the others stay
    // narrowing: the looks go first
    t = (LightTrackTunnel){ 110, 190, 0, 0, 0 };
    CHECK(light_track_move_tunnel(&f, 0, &t, why, sizeof why));
    free(after);
    after = bytes(&f);
    CHECK(strstr(after, "tunnel 110 190\n") != NULL && strstr(after, "tunnel_look 110 190 50") != NULL && strstr(after, "tunnel_look 110 190 60") != NULL);
    // a move the game refuses (the inside look would be outside): nothing changes, in any time
    free(before);
    before = bytes(&f);
    t = (LightTrackTunnel){ 130, 140, 0, 0, 0 };
    CHECK(!light_track_move_tunnel(&f, 0, &t, why, sizeof why) && why[0] != '\0');
    free(after);
    after = bytes(&f);
    CHECK(strcmp(before, after) == 0);
    // kind change only
    t = (LightTrackTunnel){ 110, 190, 1, 0, 0 };
    CHECK(light_track_move_tunnel(&f, 0, &t, why, sizeof why));
    // removing: refused while a look over part of it is left, then the rows' own looks first
    free(before);
    before = bytes(&f);
    CHECK(!light_track_remove_tunnel(&f, 0, why, sizeof why));
    free(after);
    after = bytes(&f);
    CHECK(strcmp(before, after) == 0);
    CHECK(light_track_remove_looks(&f, 120, 150, why, sizeof why));
    CHECK(light_track_remove_tunnel(&f, 0, why, sizeof why));
    free(after);
    after = bytes(&f);
    CHECK(strstr(after, "tunnel 110 190") == NULL && strstr(after, "tunnel_look 110 190") == NULL && strstr(after, "tunnel 300 400 lit\n") != NULL && strstr(after, "  tunnel_look 100\n") != NULL);   // the default look stays
    CHECK(light_track_first_bad(&f) == 0);
    CHECK(!light_track_move_tunnel(&f, 7, &t, why, sizeof why) && !light_track_remove_tunnel(&f, 7, why, sizeof why));
    free(before), free(after);
    light_track_free(&f);
}

static const char GRADED[] =
    "track CY1\n"
    "tunnel 100 200 gradient 5\n"
    "tunnel 300 400 gradient 0\n"
    "tunnel 500 600\n"
    "time A\n"
    "  tunnel_look 100 200 50\n"
    "time B\n"
    "  tunnel_look 100 200 60\n"
    "time C\n"
    "time N\n";

static void a_tunnels_gradient_follows_it(void)
{
    LightTrack f;
    open_file(GRADED, &f);
    char why[LT_WHY];
    LightTrackTunnel t = { 100, 210, 0, 0, 0 };   // the row's own cells do not carry it: the model keeps the tunnel's
    CHECK(light_track_move_tunnel(&f, 0, &t, why, sizeof why));
    char *s = bytes(&f);
    CHECK(strstr(s, "tunnel 100 210 gradient 5\n") != NULL && strstr(s, "tunnel_look 100 210 50") != NULL && strstr(s, "tunnel_look 100 210 60") != NULL);
    CHECK(strstr(s, "tunnel 300 400 gradient 0\n") != NULL && strstr(s, "tunnel 500 600\n") != NULL);   // the others' stay as they were
    // typing a new one: it is the new one, and the hall kind drops it (a hall has no mouth)
    t = (LightTrackTunnel){ 100, 210, 0, 1, 7 };
    CHECK(light_track_move_tunnel(&f, 0, &t, why, sizeof why));
    free(s);
    s = bytes(&f);
    CHECK(strstr(s, "tunnel 100 210 gradient 7\n") != NULL);
    t = (LightTrackTunnel){ 100, 210, 1, 0, 0 };
    CHECK(light_track_move_tunnel(&f, 0, &t, why, sizeof why));
    free(s);
    s = bytes(&f);
    CHECK(strstr(s, "tunnel 100 210 lit\n") != NULL);
    // removing one tunnel leaves the others' gradients, the 0 too
    CHECK(light_track_remove_tunnel(&f, 0, why, sizeof why));
    free(s);
    s = bytes(&f);
    CHECK(strstr(s, "tunnel 100") == NULL && strstr(s, "tunnel 300 400 gradient 0\n") != NULL && strstr(s, "tunnel 500 600\n") != NULL);
    CHECK(light_track_first_bad(&f) == 0);
    free(s);
    light_track_free(&f);
}

int main(void)
{
    brightness_plan_and_apply();
    a_time_without_a_block_and_overlaps();
    tones_and_lamps();
    the_games_limits_are_all_or_nothing();
    bad_asks();
    the_tunnels_rows();
    move_and_remove_a_tunnel_in_every_time();
    a_tunnels_gradient_follows_it();
    return TEST_REPORT();
}
