/* The Studio's model of a track light file (editor/light_track.c) and the game's parser (game/race/track_light_track.c) say the same about the same text: a file the model lists no bad line in is a
 * file the game reads, and the other way round, so the Studio saves no file the game refuses and shows no line as bad that the game takes (dev2's review, 2026-11-23). The limits the two count across kinds of
 * lines (the zones, the tone slots), the words they read the same (a sun's reach, sscanf numbers) and a run of generated files. The Studio shares only the core with the game, so this test is the game's. */
#include "editor/light_track.h"
#include "game/race/track_light_track.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 1200 };

static int game_reads(const char *text, char letter)
{
    TrackLightFile out;
    int bad = 0;
    const int ok = track_light_track_parse(text, strlen(text), "CY1", letter, NODES, &out, &bad);
    if (ok) track_light_file_free(&out);
    return ok;
}

// 1 when the model and the game agree about `text` (time B)
static int agree(const char *text)
{
    LightTrack f;
    char why[LT_WHY];
    if (!light_track_parse(text, strlen(text), NODES, &f, why, sizeof why)) return 1;   // not a light file: nothing to compare
    const int model = light_track_first_bad(&f) == 0, game = game_reads(text, 'B');
    if (model != game) printf("DISAGREE model=%d game=%d (line %d):\n%s---\n", model, game, light_track_first_bad(&f), text);
    light_track_free(&f);
    return model == game;
}

static void append(char **text, const char *add)
{
    const size_t n = strlen(*text), m = strlen(add);
    char *t = realloc(*text, n + m + 1);
    CHECK(t != NULL);
    memcpy(t + n, add, m + 1);
    *text = t;
}

static void zones_and_tones(void)
{
    for (int looks = 28; looks <= 36; looks++) {   // halls + looks across 64
        char *t = calloc(1, 1), line[96];
        append(&t, "track CY1\ntime B\n");
        for (int i = 0; i < looks; i++) snprintf(line, sizeof line, "  tunnel_look %d %d 50\n", i, i), append(&t, line);
        CHECK(agree(t));
        free(t);
    }
    for (int halls = 30; halls <= 34; halls++) {
        char *t = calloc(1, 1), line[96];
        append(&t, "track CY1\n");
        for (int i = 0; i < halls; i++) snprintf(line, sizeof line, "tunnel %d %d lit\n", i, i), append(&t, line);
        append(&t, "time B\n");
        for (int i = 0; i < 32; i++) snprintf(line, sizeof line, "  tunnel_look %d %d 50\n", i, i), append(&t, line);
        CHECK(agree(t));
        free(t);
    }
    for (int tones = 14; tones <= 17; tones++) {   // tone lines and coloured looks across 16
        for (int looks = 0; looks <= 3; looks++) {
            char *t = calloc(1, 1), line[96];
            append(&t, "track CY1\ntunnel 0 40\ntime B\n");
            for (int i = 0; i < tones; i++) snprintf(line, sizeof line, "  tone 50 60 %02x0000:50\n", i + 1), append(&t, line);
            for (int i = 0; i < looks; i++) snprintf(line, sizeof line, "  tunnel_look %d %d 50 ff88%02x\n", i, i, i), append(&t, line);
            CHECK(agree(t));
            free(t);
        }
    }
}

static void words(void)
{
    const char *files[] = {
        "track CY1\ntime B\n  sun 150 50 55 800\n", "track CY1\ntime B\n  sun 150 50 55 -\n", "track CY1\ntime B\n  sun 150 50 55 0.5\n", "track CY1\ntime B\n  sun 150 50 55 2000\n", "track CY1\ntime B\n  sun 150 50 55 2001\n",
        "track CY1\ntime B\n  sun - - - 10\n", "track CY1\ntime B\n  sun 1 2 3 4 5\n", "track CY1\ntime B\n  sun 1 2\n",
        "track CY1\ntime B\n  brightness 0 -4294967295 50\n", "track CY1\ntime B\n  brightness 0 +3 50\n", "track CY1\ntime B\n  brightness 0 -1 50\n", "track CY1\ntime B\n  brightness +0 3 +50\n",
        "track CY1\ntime B\n  rows 0 1 +1 2 3 4 5 6 7 8 9 10 11\n", "track CY1\ntime B\n  rows 0 1 -1 2 3 4 5 6 7 8 9 10 11\n", "track CY1\ntime B\n  rows 0 1 256 2 3 4 5 6 7 8 9 10 11\n",
        "track CY1\ntime B\n  tunnel_look 50 ff8800\n", "track CY1\ntunnel 0 9\ntime B\n  tunnel_look 0 5 50 ff8800\n  tunnel_look 6 9 60 ff8800\n",
        "track CY1\ntime B\n  tunnel_look 50\n  tunnel_look 60\n", "track CY1\ntime B\n  tunnel_look 50 ff0000\n  tunnel_look 50 00ff00\n",
    };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) CHECK(agree(files[i]));
}

static unsigned rnd_state = 12345u;
static unsigned rnd(unsigned n) { rnd_state = rnd_state * 1664525u + 1013904223u; return (rnd_state >> 8) % n; }

// generated files: facts, then a time B of random lines from the words the panel writes
static void generated(void)
{
    static const char *colours[] = { "ff8800", "ff8801", "00ff00:50", "1/0.5/0.25", "ffffff", "ff0000:100" };
    for (int round = 0; round < 400; round++) {
        char *t = calloc(1, 1), line[160];
        append(&t, "track CY1\n");
        const int facts = (int)rnd(4);
        for (int i = 0; i < facts; i++) snprintf(line, sizeof line, "tunnel %u %u%s\n", rnd(30), 30 + rnd(30), rnd(2) ? " lit" : ""), append(&t, line);
        append(&t, "time B\n");
        const int n = 1 + (int)rnd(40);
        for (int i = 0; i < n; i++) {
            const unsigned a = rnd(60), b = a + rnd(6);
            switch (rnd(8)) {
            case 0: snprintf(line, sizeof line, "  tunnel_look %u %u %u\n", a, b, 10 + rnd(4) * 10); break;
            case 1: snprintf(line, sizeof line, "  tunnel_look %u %u %s\n", a, b, colours[rnd(6)]); break;
            case 2: snprintf(line, sizeof line, "  tunnel_look %u %u %u %s\n", a, b, 10 + rnd(3) * 10, colours[rnd(6)]); break;
            case 3: snprintf(line, sizeof line, "  tunnel_look %u %s\n", 90 + rnd(3) * 10, colours[rnd(6)]); break;
            case 4: snprintf(line, sizeof line, "  tunnel_look %u\n", 90 + rnd(3) * 10); break;
            case 5: snprintf(line, sizeof line, "  tone %u %u %02x%02x00:%u\n", a, b, rnd(3), rnd(3), 10 * rnd(5)); break;
            case 6: snprintf(line, sizeof line, "  brightness %u %u %u\n", a, b, rnd(200)); break;
            default: snprintf(line, sizeof line, "  tunnel_look %u %u lamps %u\n", a, b, 10 * rnd(30)); break;
            }
            append(&t, line);
        }
        CHECK(agree(t));
        free(t);
    }
}

int main(void)
{
    zones_and_tones();
    words();
    generated();
    return TEST_REPORT();
}
