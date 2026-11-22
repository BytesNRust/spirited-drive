/* The Track light window's Tunnels list (editor/light_tunnels.h): the places' tunnels with the own look the time's file gives them, rows of this time of day only, and Own look / Remove own look. */
#include "light_tunnels.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char HEAD[] = "# tnfs track light 1\ntrack CY1 time N nodes 200 flags 1\nrows 0 199 255 255 255 255 255 255 255 255 255 255 255\n";

static LightFile parse(const char *text)
{
    LightFile f;
    char why[LIGHT_WHY];
    CHECK(light_file_parse(text, strlen(text), &f, why, sizeof why));
    return f;
}

static void test_the_list(void)
{
    LightFile places = parse("track CY1\ntunnel 10 20\ncanyon 50 60\ntunnel 100 120\ntunnel 150 160\n");
    char text[1024];
    snprintf(text, sizeof text, "%stunnel_tone 104 ff5a0d\ntunnel_lamp 300 ffc880 1.8\ntunnel 100 120 90 ff5a0d\ntunnel_lamp 100 120 220 ffffff 1.4\ntunnel 150 160 lit\ntunnel 170 180 80\ntunnel_lamp 190 195 60\n", HEAD);
    LightFile time = parse(text);
    LightTunnelRow rows[LIGHT_TUNNEL_ROWS];
    const int n = light_tunnels_list(&time, &places, rows, LIGHT_TUNNEL_ROWS);
    CHECK_EQ_INT(n, 5);   // three place tunnels (the canyon is not one), a zone and a lamp line that match no place
    CHECK(rows[0].from == 10 && rows[0].to == 20 && rows[0].place == 0 && rows[0].zone < 0 && rows[0].lamp < 0);   // as all
    CHECK(rows[1].from == 100 && rows[1].place == 1 && rows[1].zone == 0 && rows[1].lamp == 1 && !rows[1].hall);
    CHECK(rows[2].from == 150 && rows[2].place == 2 && rows[2].zone == 1 && rows[2].hall);
    CHECK(rows[3].from == 170 && rows[3].to == 180 && rows[3].place == -1 && rows[3].zone == 2);   // this time of day only
    CHECK(rows[4].from == 190 && rows[4].to == 195 && rows[4].place == -1 && rows[4].zone < 0 && rows[4].lamp == 2);
    LightTunnelLook all, look;
    light_tunnels_all(&time, &all);
    CHECK(all.has_light && all.light == 104.0 && strcmp(all.colour, "ff5a0d") == 0 && all.strength == 300.0 && strcmp(all.lamp_colour, "ffc880") == 0 && all.reach == 1.8);
    light_tunnels_look(&time, &rows[0], &look);   // inherited
    CHECK(look.light == 104.0 && !look.light_own && look.strength == 300.0 && !look.strength_own && !look.reach_own);
    light_tunnels_look(&time, &rows[1], &look);   // its own
    CHECK(look.light == 90.0 && look.light_own && look.colour_own && look.strength == 220.0 && look.strength_own && strcmp(look.lamp_colour, "ffffff") == 0 && look.reach == 1.4 && look.reach_own);
    light_tunnels_look(&time, &rows[4], &look);   // a lamp line alone: its strength, the rest inherited
    CHECK(look.strength == 60.0 && look.strength_own && !look.reach_own && look.reach == 1.8 && look.light == 104.0 && !look.light_own);
    CHECK_EQ_INT(light_tunnels_list(&time, NULL, rows, LIGHT_TUNNEL_ROWS), 4);   // no places file: the zone lines and the lamp lines, each its own row
    light_file_free(&time);
    light_file_free(&places);
}

static void test_own_look_and_remove(void)
{
    LightFile places = parse("track CY1\ntunnel 10 20\ntunnel 30 40\n");
    char text[512], why[LIGHT_WHY];
    snprintf(text, sizeof text, "%stunnel_tone 104 ff5a0d\ntunnel_lamp 250 ffc880 2.2\n", HEAD);
    LightFile time = parse(text);
    LightTunnelRow rows[LIGHT_TUNNEL_ROWS];
    CHECK_EQ_INT(light_tunnels_list(&time, &places, rows, LIGHT_TUNNEL_ROWS), 2);
    CHECK(light_tunnels_own_look(&time, &rows[0], why, sizeof why));
    size_t size = 0;
    char *out = light_file_format(&time, &size);
    CHECK(out != NULL && strstr(out, "tunnel 10 20 104 ff5a0d\n") != NULL && strstr(out, "tunnel_lamp 10 20 250 ffc880 2.2\n") != NULL);   // the general values: nothing changes until edited
    free(out);
    CHECK_EQ_INT(light_tunnels_list(&time, &places, rows, LIGHT_TUNNEL_ROWS), 2);
    CHECK(rows[0].zone >= 0 && rows[0].lamp >= 1);
    LightTunnelLook look;
    light_tunnels_look(&time, &rows[0], &look);
    CHECK(look.light_own && look.strength_own && look.reach_own);
    CHECK(light_tunnels_own_look(&time, &rows[0], why, sizeof why));   // again: the lines it has stay
    CHECK_EQ_INT(light_file_count(&time, LIGHT_KIND_ZONE), 1);
    CHECK_EQ_INT(light_file_count(&time, LIGHT_KIND_TUNNEL_LAMP), 2);
    CHECK(light_tunnels_remove_own(&time, &rows[0]));
    out = light_file_format(&time, &size);
    CHECK(out != NULL && strcmp(out, text) == 0);   // back to the file as it was, byte for byte
    free(out);
    CHECK(!light_tunnels_remove_own(&time, &rows[1]));   // a tunnel with no look of its own: nothing to remove
    light_file_free(&time);
    // no general lines: the game's values (300 at night, 150 by day), no zone line when there is no ambient to copy
    snprintf(text, sizeof text, "%s", HEAD);
    time = parse(text);
    CHECK_EQ_INT(light_tunnels_list(&time, &places, rows, LIGHT_TUNNEL_ROWS), 2);
    CHECK(light_tunnels_own_look(&time, &rows[1], why, sizeof why));
    out = light_file_format(&time, &size);
    CHECK(out != NULL && strstr(out, "tunnel_lamp 30 40 300\n") != NULL && strstr(out, "tunnel 30 40") == NULL);
    free(out);
    light_file_free(&time);
    // a hall has no look of its own
    snprintf(text, sizeof text, "%stunnel 10 20 lit\n", HEAD);
    time = parse(text);
    CHECK_EQ_INT(light_tunnels_list(&time, &places, rows, LIGHT_TUNNEL_ROWS), 2);
    CHECK(rows[0].hall && !light_tunnels_own_look(&time, &rows[0], why, sizeof why) && why[0] != '\0');
    CHECK(!light_tunnels_remove_own(&time, &rows[0]));   // and its zone line is not a look: it stays
    light_file_free(&time);
    light_file_free(&places);
}

int main(void)
{
    test_the_list();
    test_own_look_and_remove();
    return TEST_REPORT();
}
