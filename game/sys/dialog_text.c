/* port: see dialog_text.h. */
#include "game/sys/dialog_text.h"

#include <string.h>

static int is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

int dialog_chars(const char *text)
{
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++)
        if (!is_continuation(*p)) n++;
    return n;
}

/* the bytes of the next word (no space, no newline) and its characters */
static size_t word_at(const char *p, int *chars)
{
    size_t n = 0;
    *chars = 0;
    while (p[n] != '\0' && p[n] != ' ' && p[n] != '\n') {
        if (!is_continuation((unsigned char)p[n])) (*chars)++;
        n++;
    }
    return n;
}

int dialog_wrap(const char *text, int width, char *out, size_t size)
{
    if (size == 0) return 0;
    if (text == NULL) text = "";
    if (width < 1) width = 1;
    size_t at = 0;
    int col = 0;
    const char *p = text;
#define PUT(c) ((void)(at + 1 < size && (out[at++] = (c), 1)))   /* an expression: the cut text still ends */
    while (*p != '\0') {
        if (*p == '\n') {
            PUT('\n'), col = 0, p++;
            continue;
        }
        if (*p == ' ') {   /* a space is kept inside a line; at the end of one it is dropped for the newline */
            int next_chars;
            (void)word_at(p + 1, &next_chars);
            if (col > 0 && col + 1 + next_chars > width && next_chars <= width) {
                PUT('\n'), col = 0, p++;
                continue;
            }
            if (col > 0) PUT(' '), col++;
            p++;
            continue;
        }
        int chars;
        const size_t bytes = word_at(p, &chars);
        if (col > 0 && col + chars > width && chars <= width) PUT('\n'), col = 0;
        for (size_t i = 0; i < bytes;) {   /* a word longer than a line (a path) is cut inside, never inside a UTF-8 sequence */
            if (col >= width) PUT('\n'), col = 0;
            PUT(p[i]);
            i++;
            while (i < bytes && is_continuation((unsigned char)p[i])) PUT(p[i]), i++;
            col++;
        }
        p += bytes;
    }
#undef PUT
    out[at] = '\0';
    return (int)at;
}
