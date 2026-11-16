/* dep59 step 3: an added car's description in lines (game/frontend/text_wrap.h): broken at spaces, a word wider than a line cut by
 * characters (Review Focus 6), no more lines than asked, a UTF-8 character never split. */
#include "game/frontend/text_wrap.h"
#include "test.h"

#include <string.h>

/* 10 px a character (a UTF-8 character counts once) */
static int ten(void *ctx, const char *s, int len)
{
    (void)ctx;
    int n = 0;
    for (int i = 0; i < len; i++) n += ((unsigned char)s[i] & 0xc0) != 0x80;
    return 10 * n;
}

static int is(const char *text, const TextLine *l, const char *want)
{
    return l->len == (int)strlen(want) && strncmp(text + l->start, want, (size_t)l->len) == 0;
}

int main(void)
{
    TextLine lines[64];
    const char *t = "The last car Enzo signed";
    CHECK_EQ_INT(text_wrap(t, 100, ten, NULL, lines, 64), 3);
    CHECK(is(t, &lines[0], "The last"));
    CHECK(is(t, &lines[1], "car Enzo"));
    CHECK(is(t, &lines[2], "signed"));
    CHECK_EQ_INT(text_wrap(t, 100, ten, NULL, lines, 2), 2);   /* the rest dropped */
    CHECK_EQ_INT(text_wrap(t, 1000, ten, NULL, lines, 64), 1);
    CHECK(is(t, &lines[0], t));
    char word[401];   /* Review Focus 6: 400 characters with no space, cut every 10 */
    memset(word, 'x', 400), word[400] = '\0';
    CHECK_EQ_INT(text_wrap(word, 100, ten, NULL, lines, 64), 40);
    for (int i = 0; i < 40; i++) CHECK(lines[i].len == 10 && lines[i].start == 10 * i);
    CHECK_EQ_INT(text_wrap("ab  cd", 30, ten, NULL, lines, 64), 2);   /* spaces at a break are not on either line */
    CHECK(is("ab  cd", &lines[0], "ab") && is("ab  cd", &lines[1], "cd"));
    const char *u = "ÑÑÑÑÑ";   /* two bytes each: cut between characters, never inside one */
    CHECK_EQ_INT(text_wrap(u, 20, ten, NULL, lines, 64), 3);
    CHECK(lines[0].len == 4 && lines[1].len == 4 && lines[2].len == 2);
    CHECK_EQ_INT(text_wrap("", 100, ten, NULL, lines, 64), 0);
    CHECK_EQ_INT(text_wrap("   ", 100, ten, NULL, lines, 64), 0);
    CHECK_EQ_INT(text_wrap("abc", 5, ten, NULL, lines, 64), 3);   /* a line narrower than a character: one a line, never none */
    char w[64];   /* the words the small face draws */
    CHECK_EQ_INT(text_small_words("Fast, light; red.", w, sizeof w), 16);
    CHECK(strcmp(w, "Fast light; red.") == 0);
    CHECK_EQ_INT(text_small_words("Ni\xc3\xb1o \xe2\x82\xac" "5", w, sizeof w), 5);   /* a UTF-8 character dropped whole */
    CHECK(strcmp(w, "Nio 5") == 0);
    CHECK_EQ_INT(text_small_words("a\tb\nc", w, sizeof w), 5);
    CHECK(strcmp(w, "a b c") == 0);
    CHECK_EQ_INT(text_small_words("abcdef", w, 4), 3);   /* cut to the room */
    CHECK(strcmp(w, "abc") == 0);
    CHECK_EQ_INT(text_small_words("\xff\xfe", w, sizeof w), 0);
    CHECK(strcmp(w, "") == 0);
    /* the main face (GRAVER18.FFN): its ',' is a comma, kept; the rest as the small face's */
    CHECK_EQ_INT(text_words("Fast, light; red.", w, sizeof w), 17);
    CHECK(strcmp(w, "Fast, light; red.") == 0);
    CHECK_EQ_INT(text_words("Ni\xc3\xb1o,\t\xe2\x82\xac" "5\x7f", w, sizeof w), 7);
    CHECK(strcmp(w, "Nio, 5 ") == 0);
    CHECK_EQ_INT(text_words("abcdef", w, 4), 3);
    CHECK(strcmp(w, "abc") == 0);
    CHECK_EQ_INT(text_words("\xc3", w, sizeof w), 0);   /* a character cut short at the end: dropped, nothing read past it */
    CHECK(strcmp(w, "") == 0);
    return TEST_REPORT();
}
