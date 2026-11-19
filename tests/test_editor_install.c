/* The editor's install by hand (editor/build.c): the shell command it names where it does not run the game itself, with and without a
 * player folder; and the install refusing, without running anything, a game that is not there. */
#include "build.h"
#include "test.h"

#include <string.h>

static void check_command_text(void)
{
    char out[256];
    install_command_text("/g/tnfs", "/p/a.zip", "/u", out, sizeof out);
    CHECK(strcmp(out, "/g/tnfs --user /u --install /p/a.zip") == 0);
    install_command_text("/g/tnfs", "/p/a.zip", "", out, sizeof out);   /* no player folder: the game's own, so no --user at all */
    CHECK(strcmp(out, "/g/tnfs --install /p/a.zip") == 0);
    char tiny[12];
    install_command_text("/g/tnfs", "/p/a.zip", "/u", tiny, sizeof tiny);   /* cut, never overrun */
    CHECK(strlen(tiny) == sizeof tiny - 1);
}

static void check_missing_game(void)
{
    char why[256];
    CHECK(!install_zip("/no/such/dir/tnfs", "/no/such.zip", "/u", why, sizeof why));
    CHECK(why[0] != '\0');
}

int main(void)
{
    check_command_text();
    check_missing_game();
    return TEST_REPORT();
}
