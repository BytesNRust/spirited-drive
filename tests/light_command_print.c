/* Prints the command and environment the Studio gives the game for a Track light photo or test drive, one word a line ("A word" for the program's arguments, "E word" for its
 * environment), so a script can run the real game with exactly what the Studio would (tests/editor_photo_drive_same_light.py). Nothing is run and no file is written.
 * usage: light_command_print photo|drive GAME CD USER MODS TRACK LETTER NODE SHOT */
#include "light_drive.h"
#include "light_photo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 10) return fprintf(stderr, "usage: %s photo|drive GAME CD USER MODS TRACK LETTER NODE SHOT\n", argv[0]), 2;
    const int photo = strcmp(argv[1], "photo") == 0;
    if (!photo && strcmp(argv[1], "drive") != 0) return fprintf(stderr, "photo or drive\n"), 2;
    const int view = LIGHT_VIEW_DEFAULT;
    const unsigned node = (unsigned)atoi(argv[8]);
    RaceCommand c;
    char why[LIGHT_DRIVE_WHY] = "";
    int ok;
    if (photo) {
        LightPhotoSetup s = { argv[2], argv[3], argv[4], argv[5], { 0 }, argv[7][0], node, view, 1, light_photo_view_ticks(view), argv[9], NULL };   /* hud 1: the drive's picture has one */
        snprintf(s.track, sizeof s.track, "%s", argv[6]);
        ok = light_photo_command(&s, NULL, &c, why, sizeof why);
    } else {
        LightDriveSetup s = { argv[2], argv[3], argv[4], argv[5], { 0 }, argv[7][0], node, 0, NULL, NULL, view };   /* weather 0: clear, as the photo's */
        snprintf(s.track, sizeof s.track, "%s", argv[6]);
        ok = light_drive_command(&s, NULL, &c, why, sizeof why);
    }
    if (!ok) return fprintf(stderr, "%s\n", why), 1;
    for (int i = 0; i < c.argc; ++i) printf("A %s\n", c.argv[i]);
    for (int i = 0; i < c.envc; ++i) printf("E %s\n", c.envp[i]);
    car_race_command_free(&c);
    return 0;
}
