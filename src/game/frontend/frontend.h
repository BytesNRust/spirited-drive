/* Front end: boot screens and the menu loop (NFS_WIN.EXE 0043xxxx-0046xxxx). */
#ifndef TNFS_FRONTEND_H
#define TNFS_FRONTEND_H

/* show_screen modes, the second argument of 00466f68. */
enum {
    SCREEN_WAIT = 0,     /* fade in, wait 5 s (3 s with music) or a key, fade out */
    SCREEN_HOLD = 1,     /* fade in and return; the caller decides when to leave */
    SCREEN_INSTANT = 3   /* draw without clearing or fading */
};

/* 00466f68: shows an art QFS ("!pal" + "bgnd"). Returns 1 if a key skipped it. */
int show_screen(const char *name, int mode, int music);

void title_screen(int state);  /* 004372e4 */
int intro_sequence(void);      /* 00441abc */
void frontend_run(void);       /* 004373c4 */

#endif
