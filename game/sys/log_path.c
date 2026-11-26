/* port: see log_path.h. */
#include "game/sys/log_path.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int is_separator(char c) { return c == '/' || c == '\\'; }

/* Windows paths differ in case alone ("c:\users" and "C:\Users" are one folder) */
static int same_prefix(const char *a, const char *b, size_t n)
{
#ifdef _WIN32
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
#else
    return strncmp(a, b, n) == 0;
#endif
}

/* the length of `folder` without trailing separators, when `path` starts with it as whole names; else 0 */
static size_t prefix_of(const char *path, const char *folder)
{
    if (folder == NULL) return 0;
    size_t n = strlen(folder);
    while (n > 0 && is_separator(folder[n - 1])) n--;
    if (n == 0 || strlen(path) < n || !same_prefix(path, folder, n)) return 0;
    return path[n] == '\0' || is_separator(path[n]) ? n : 0;
}

void log_path_for_report(const char *path, const char *home, const char *user_dir, char *out, size_t cap)
{
    if (cap == 0) return;
    if (path == NULL) path = "";
    size_t n = prefix_of(path, user_dir);
    if (n != 0) {
        snprintf(out, cap, "<user>%s", path + n);
        return;
    }
    n = prefix_of(path, home);
    if (n != 0) snprintf(out, cap, "~%s", path + n);
    else snprintf(out, cap, "%s", path);
}

/* a character a folder's name cannot be preceded by if the folder is to be taken as the whole of a path's start: one of a longer path or word */
static int is_name_char(char c) { return isalnum((unsigned char)c) || c == '/' || c == '\\' || c == '.' || c == '_' || c == '-' || c == '~'; }

static size_t folder_length(const char *folder)
{
    size_t n = folder != NULL ? strlen(folder) : 0;
    while (n > 0 && is_separator(folder[n - 1])) n--;
    return n;
}

size_t log_text_for_report(const char *text, const char *home, const char *user_dir, char *out, size_t cap)
{
    if (cap == 0) return 0;
    if (text == NULL) text = "";
    const size_t user_n = folder_length(user_dir), home_n = folder_length(home);
    size_t at = 0;
    for (size_t i = 0; text[i] != '\0';) {
        const char *shown = NULL;
        size_t used = 0;
        const int starts = i == 0 || !is_name_char(text[i - 1]);
        if (starts && user_n != 0 && same_prefix(text + i, user_dir, user_n) && strlen(text + i) >= user_n && (text[i + user_n] == '\0' || !isalnum((unsigned char)text[i + user_n]))) shown = "<user>", used = user_n;
        else if (starts && home_n != 0 && strlen(text + i) >= home_n && same_prefix(text + i, home, home_n) && (text[i + home_n] == '\0' || !isalnum((unsigned char)text[i + home_n]))) shown = "~", used = home_n;
        if (shown != NULL) {
            for (const char *c = shown; *c != '\0' && at + 1 < cap; c++) out[at++] = *c;
            i += used;
        } else if (at + 1 < cap) {
            out[at++] = text[i++];
        } else {
            break;
        }
    }
    out[at] = '\0';
    return at;
}
