/* path_expand (editor/path_expand.h): ~, $HOME and ${HOME} at the start of a path stand for the home folder; nothing else is touched. */
#include "path_expand.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

static void is(const char *in, const char *home, const char *want, int expanded)
{
    char out[256];
    CHECK_EQ_INT(path_expand(in, home, out, sizeof out), expanded);
    CHECK(strcmp(out, want) == 0);
    if (strcmp(out, want) != 0) printf("  '%s' with home '%s' gave '%s', wanted '%s'\n", in, home != NULL ? home : "(null)", out, want);
}

int main(void)
{
    is("~/workspace/data", "/Volumes/user", "/Volumes/user/workspace/data", 1);
    is("~", "/Volumes/user", "/Volumes/user", 1);
    is("$HOME/x", "/Volumes/user", "/Volumes/user/x", 1);
    is("${HOME}/x", "/Volumes/user/", "/Volumes/user/x", 1);   /* a home with its slash: one slash */
    is("$HOME", "/h", "/h", 1);
    is("~other/x", "/h", "~other/x", 0);   /* another user's home is not this one's */
    is("$HOMEWORK/x", "/h", "$HOMEWORK/x", 0);
    is("/abs/~/x", "/h", "/abs/~/x", 0);   /* only at the start */
    is("~/x", NULL, "~/x", 0);
    is("~/x", "", "~/x", 0);
    is("", "/h", "", 0);
    char tiny[8];
    CHECK_EQ_INT(path_expand("~/abcdefghij", "/home/user", tiny, sizeof tiny), 1);   /* cut, never overflowing */
    CHECK_EQ_INT((int)strlen(tiny), 7);
    return TEST_REPORT();
}
