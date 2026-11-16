/* See text_wrap.h. */
#include "game/frontend/text_wrap.h"

#include <string.h>

static int is_space(char c) { return c == ' '; }

/* the byte after the UTF-8 character at `i` */
static int next_char(const char *t, int i, int n)
{
    i++;
    while (i < n && ((unsigned char)t[i] & 0xc0) == 0x80) i++;
    return i;
}

int text_wrap(const char *text, int width, int (*measure)(void *ctx, const char *s, int len), void *ctx, TextLine *lines, int max)
{
    const int n = (int)strlen(text);
    int count = 0, pos = 0;
    while (pos < n && is_space(text[pos])) pos++;
    while (pos < n && count < max) {
        const int start = pos;
        int end = start;
        for (int at = start; at < n;) {   /* whole words while they fit */
            int w = at;
            while (w < n && !is_space(text[w])) w++;
            if (measure(ctx, text + start, w - start) > width) break;
            end = w;
            while (w < n && is_space(text[w])) w++;
            at = w;
        }
        if (end == start) {   /* a word wider than the line: as many characters as fit, one at least */
            end = next_char(text, start, n);
            for (int c = end; c < n && !is_space(text[c]); c = next_char(text, c, n)) {
                const int after = next_char(text, c, n);
                if (measure(ctx, text + start, after - start) > width) break;
                end = after;
            }
        }
        lines[count++] = (TextLine){ start, end - start };
        pos = end;
        while (pos < n && is_space(text[pos])) pos++;
    }
    return count;
}

int text_small_words(const char *text, char *out, int size)
{
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0' && n < size - 1; p++) {
        if (*p >= 0x80) {
            while ((p[1] & 0xc0) == 0x80) p++;   /* the rest of the character */
            continue;
        }
        if (*p == ',') continue;
        out[n++] = *p < 0x20 || *p == 0x7f ? ' ' : (char)*p;
    }
    if (size > 0) out[n] = '\0';
    return n;
}
