/* The strip of a light line (editor/light_strip.h): the words of a file, the masks they make, the bar's cells. */
#include "light_strip.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int parse(const char *a, const char *b, unsigned *mask)
{
    const char *w[2] = { a, b };
    return light_strip_parse(w, b != NULL ? 2 : 1, mask);
}

static void words(void)
{
    unsigned m = 0;
    CHECK(parse("all", NULL, &m) == 1 && m == 0x7ff);
    CHECK(parse("right", NULL, &m) == 1 && m == 0x03e);
    CHECK(parse("left", "p3", &m) == 1 && m == 0x7c0);   // a word takes one word only
    CHECK(parse("road", NULL, &m) == 1 && m == ((1u << 0) | (1u << 1) | (1u << 6)));
    CHECK(parse("p4", NULL, &m) == 1 && m == 1u << 4);
    CHECK(parse("p2", "p5", &m) == 2 && m == 0x3c);
    CHECK(parse("p10", "p10", &m) == 2 && m == 1u << 10);
    m = 77;
    CHECK(parse("p5", "p2", &m) == 0 && m == 77);          // a range that goes back
    CHECK(parse("p11", NULL, &m) == 0 && m == 77);
    CHECK(parse("p01", NULL, &m) == 0 && parse("p", NULL, &m) == 0 && parse("50left", NULL, &m) == 0 && parse("Left", NULL, &m) == 0);
    CHECK(parse("p3", "70", &m) == 1 && m == 1u << 3);       // the next word is not a point: the strip is p3 alone, the caller reads 70
}

static void text(void)
{
    char t[32];
    CHECK(light_strip_text(0x7ff, t, sizeof t) == 1 && strcmp(t, "all") == 0);
    CHECK(light_strip_text(0x03e, t, sizeof t) == 1 && strcmp(t, "right") == 0);
    CHECK(light_strip_text(0x7c0, t, sizeof t) == 1 && strcmp(t, "left") == 0);
    CHECK(light_strip_text(LIGHT_STRIP_ROAD, t, sizeof t) == 1 && strcmp(t, "road") == 0);
    CHECK(light_strip_text(1u << 7, t, sizeof t) == 1 && strcmp(t, "p7") == 0);
    CHECK(light_strip_text(0x00f, t, sizeof t) == 1 && strcmp(t, "p0 p3") == 0);
    CHECK(light_strip_text(0x7fe, t, sizeof t) == 1 && strcmp(t, "p1 p10") == 0);
    CHECK(light_strip_text(0, t, sizeof t) == 0 && t[0] == '\0');
    CHECK(light_strip_text(0x00a, t, sizeof t) == 0 && t[0] == '\0');   // p1 and p3: a gap
    CHECK(light_strip_text(0x800, t, sizeof t) == 0);                    // a point past 10
    for (unsigned m = 1; m <= 0x7ff; m++) {   // what a line says comes back as the same mask
        unsigned back = 0;
        if (light_strip_text(m, t, sizeof t)) {
            char a[16] = "", b[16] = "";
            const int n = sscanf(t, "%15s %15s", a, b);
            const char *w[2] = { a, b };
            CHECK(light_strip_parse(w, n, &back) == n && back == m);
        }
    }
}

static void bar(void)
{
    CHECK(light_strip_cell_point(0) == 10 && light_strip_cell_point(4) == 6 && light_strip_cell_point(5) == 0 && light_strip_cell_point(6) == 1 && light_strip_cell_point(10) == 5);
    CHECK(light_strip_cell_point(-1) == -1 && light_strip_cell_point(11) == -1);
    CHECK(light_strip_span(0, 10) == 0x7ff);
    CHECK(light_strip_span(10, 0) == 0x7ff);            // either order
    CHECK(light_strip_span(6, 10) == 0x03e);           // the right side
    CHECK(light_strip_span(0, 4) == 0x7c0);            // the left side
    CHECK(light_strip_span(4, 6) == LIGHT_STRIP_ROAD); // p6 p0 p1: the road
    CHECK(light_strip_span(5, 5) == 1u);
    char t[32];
    CHECK(light_strip_text(light_strip_span(5, 8), t, sizeof t) == 1 && strcmp(t, "p0 p3") == 0);   // centre and the right's first three
    CHECK(light_strip_text(light_strip_span(3, 5), t, sizeof t) == 0);                              // p7 p6 p0: points 0, 6, 7 skip p1..p5
}

int main(void)
{
    words();
    text();
    bar();
    return TEST_REPORT();
}
