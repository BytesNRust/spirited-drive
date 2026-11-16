/* port (dep59 step 3): text broken into lines of at most a width, for a page that shows a mod's own words (an added car's description
 * in its gallery). Lines break at spaces; a word wider than a line is cut by characters (a UTF-8 character is never split). The
 * caller's `measure` gives a run's width in pixels (text_width's, which skips a character the font does not have). */
#ifndef TNFS_TEXT_WRAP_H
#define TNFS_TEXT_WRAP_H

typedef struct {
    int start, len;   /* bytes of the text, without the spaces a line broke at */
} TextLine;

/* `text` in lines of at most `width` pixels by `measure`; at most `max` lines (the rest dropped); how many */
int text_wrap(const char *text, int width, int (*measure)(void *ctx, const char *s, int len), void *ctx, TextLine *lines, int max);

/* `text` as the bytes the small face (MAIN18.FFN) draws right, at most size - 1 of them: printable ASCII kept, a UTF-8 character
 * dropped whole (the face's codes above 0x7f are not Latin-1 or UTF-8) and ',' dropped (the face's 0x2c is a copy of its 0xfc,
 * a u with two dots), a control character made a space; `out` NUL-terminated; how many bytes */
int text_small_words(const char *text, char *out, int size);

#endif
