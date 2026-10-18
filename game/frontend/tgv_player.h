/* A TGV movie played in the menus' window (004423d4's place): the file's chunks in order ('kVGT' / 'fVGT' through
 * tgv_decode, 0048e8b4's; '1SNh' / '1SNd' the sound, an EACS sample), the pictures at 15 a second (00441f18 counts
 * the 100 Hz clock times 15 / 100) on a 320 x 200 screen, each centred at ((320 - w) / 2 + 3) & ~3,
 * ((200 - h) / 2 + 3) & ~3 as 004423d4 places it, shown twice the size in the 640 x 480 window. Glue: the original's
 * stream reader (0048a674 .. 0048bde8) and its sound channel are replaced by reading the whole file. */
#ifndef TNFS_TGV_PLAYER_H
#define TNFS_TGV_PLAYER_H

/* 004423d4 (eax the path): 1 when a key (Space, Enter, Escape) or a button stopped it, 0 when it ran to its end or
 * the file could not be read */
int tgv_player_play(const char *path);

#endif
