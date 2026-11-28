/* The first start's message boxes on Windows (game/sys/dialog_text.h): its box does not wrap (one long line makes it as wide as the screen) and gives the buttons a fixed
 * width (a long label is cut): the wrapping keeps every line within 60 characters and every button label within 14, nothing of the words lost. */
#include "game/sys/dialog_text.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int longest_line(const char *text)
{
    int longest = 0;
    for (const char *line = text; *line != '\0';) {
        const char *end = strchr(line, '\n');
        const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        char one[400];
        snprintf(one, sizeof one, "%.*s", (int)len, line);
        if (dialog_chars(one) > longest) longest = dialog_chars(one);
        line += len + (end != NULL);
    }
    return longest;
}

static void flatten(const char *text, char *out, size_t size)
{
    size_t n = 0;
    for (const char *p = text; *p != '\0' && n + 1 < size; p++) out[n++] = *p == '\n' ? ' ' : *p;
    out[n] = '\0';
}

int main(void)
{
    char out[2400];
    /* a sentence is cut at spaces, the words are kept */
    const char *sentence = "The same game on a modern screen: widescreen and smooth motion up to 144 fps. Nothing else is changed at all.";
    dialog_wrap(sentence, 30, out, sizeof out);
    CHECK(longest_line(out) <= 30);
    char flat[400];
    flatten(out, flat, sizeof flat);
    CHECK(strcmp(flat, sentence) == 0);
    /* the newlines of the text stay, a short text is not changed */
    CHECK_EQ_INT(dialog_wrap("one\n\ntwo", 60, out, sizeof out), 8);
    CHECK(strcmp(out, "one\n\ntwo") == 0);
    /* a path with no space (a long one) is cut inside, 60 a line, UTF-8 whole */
    char path[400] = "C:\\Users\\";
    for (int i = 0; i < 12; i++) strcat(path, "n\xc3\xa1me-of-a-folder\\");
    dialog_wrap(path, DIALOG_LINE_MAX, out, sizeof out);
    CHECK(longest_line(out) <= DIALOG_LINE_MAX);
    for (const unsigned char *p = (const unsigned char *)out; *p != '\0'; p++)
        if (*p == '\n') CHECK((p[1] & 0xC0) != 0x80);   /* never between the bytes of a character */
    /* a small buffer cuts the text and still ends it */
    char small[10];
    dialog_wrap(sentence, 30, small, sizeof small);
    CHECK_EQ_INT(small[sizeof small - 1], 0);
    CHECK_EQ_INT(dialog_wrap("x", 30, out, 0), 0);

    /* the start window: its text wrapped, and every button label of the first start fits */
    dialog_wrap(FIRST_RUN_START_TEXT, DIALOG_LINE_MAX, out, sizeof out);
    CHECK(longest_line(out) <= DIALOG_LINE_MAX);
    CHECK(strstr(out, "Special Edition (PC, 1996)") != NULL || strstr(out, "Special\nEdition") != NULL);
    dialog_wrap(FIRST_RUN_NO_CHOOSER_TEXT, DIALOG_LINE_MAX, out, sizeof out);
    CHECK(longest_line(out) <= DIALOG_LINE_MAX);
    CHECK(strstr(out, "--data") != NULL);
    const char *const labels[] = { FIRST_RUN_BUTTON_CD, FIRST_RUN_BUTTON_ISO, FIRST_RUN_BUTTON_QUIT, FIRST_RUN_BUTTON_AGAIN, FIRST_RUN_BUTTON_OTHER_FOLDER,
                                   FIRST_RUN_BUTTON_OTHER_IMAGE, FIRST_RUN_BUTTON_ANYWAY };
#ifndef __linux__   /* the width is Windows' button; Linux's boxes size their buttons to the label */
    for (size_t i = 0; i < sizeof labels / sizeof labels[0]; i++) CHECK(dialog_chars(labels[i]) <= DIALOG_BUTTON_MAX);
#else
    (void)labels;
#endif
    CHECK_EQ_INT(dialog_chars("CD or folder\xe2\x80\xa6"), 13);   /* the ellipsis is one character */

    /* Linux, pinned on every system. Its notice names both install commands, a line each, and its labels are ASCII: SDL's own X11 box has no ellipsis in its font */
    dialog_wrap(FIRST_RUN_NO_CHOOSER_TEXT_LINUX, DIALOG_LINE_MAX, out, sizeof out);
    CHECK(longest_line(out) <= DIALOG_LINE_MAX);
    CHECK(strstr(out, "\nsudo apt install zenity") != NULL);
    CHECK(strstr(out, "\nsudo dnf install zenity") != NULL);
    CHECK(strstr(out, "package manager") != NULL);
    CHECK(strstr(out, "--data") != NULL);
    const char *const linux_texts[] = { FIRST_RUN_BUTTON_CD_LINUX, FIRST_RUN_BUTTON_ISO_LINUX, FIRST_RUN_NO_CHOOSER_TEXT_LINUX };
    for (size_t i = 0; i < sizeof linux_texts / sizeof linux_texts[0]; i++)
        for (const unsigned char *p = (const unsigned char *)linux_texts[i]; *p != '\0'; p++) CHECK(*p < 0x80);
    CHECK(strcmp(FIRST_RUN_BUTTON_CD_LINUX, "CD or folder...") == 0);
    CHECK(strcmp(FIRST_RUN_BUTTON_ISO_LINUX, "ISO image...") == 0);
    return TEST_REPORT();
}
