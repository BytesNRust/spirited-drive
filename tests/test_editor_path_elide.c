/* A long path in one line (editor/path_elide.h): the start and the end kept, "..." in the middle; a short one as it is. */
#include "path_elide.h"
#include "test.h"

#include <string.h>

static void check(const char *path, int max, const char *want)
{
    char out[256];
    path_elide(path, max, out, sizeof out);
    CHECK(strcmp(out, want) == 0);
    if (strcmp(out, want) != 0) fprintf(stderr, "  path_elide(\"%s\", %d) = \"%s\", want \"%s\"\n", path, max, out, want);
    CHECK((int)strlen(out) <= (max < 7 ? 7 : max) || strlen(path) <= (size_t)max);
}

int main(void)
{
    check("/a/b", 20, "/a/b");                                            /* fits: as it is */
    check("/home/ana/tracks/xyz", 20, "/home/ana/tracks/xyz");        /* exactly the room */
    check("/srv/game/mods/stages/vp/tarmac", 20, "/srv/gam...vp/tarmac");   /* 20: 8 of the start, 9 of the end (the end tells which) */
    check("/srv/game/mods/stages/vp/tarmac", 7, "/...mac");      /* very little room: still a start and an end */
    check("/srv/game/mods/stages/vp/tarmac", 3, "/...mac");      /* less than that: the least, 7 */
    check("", 10, "");
    check("/abcd", 3, "/abcd");                                           /* shorter than the least an elided one keeps: as it is */
    char small[8];
    path_elide("/srv/game/mods/stages/vp/tarmac", 30, small, sizeof small);   /* a small buffer: cut, ended */
    CHECK(strlen(small) < sizeof small);
    return TEST_REPORT();
}
