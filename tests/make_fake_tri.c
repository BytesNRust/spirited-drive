/* Writes a synthetic TRI (tests/fake_tri.h) for the shell tests of the Studio that need a track's node count and no CD data. usage: make_fake_tri FILE SEGMENTS */
#include "fake_tri.h"

int main(int argc, char **argv)
{
    if (argc != 3) return fprintf(stderr, "usage: make_fake_tri FILE SEGMENTS\n"), 2;
    return fake_tri_write(argv[1], atoi(argv[2])) ? 0 : 1;
}
