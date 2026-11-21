/* The Track light window's session (light_session.h). */
#include "light_session.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list args;
        va_start(args, format);
        vsnprintf(why, cap, format, args);
        va_end(args);
    }
    return 0;
}

/* 1 when a component of the path is `original` (any case): the CD's own data, only ever read */
static int under_original(const char *path)
{
    for (const char *p = path; *p != '\0';) {
        const char *end = strchr(p, '/');
        const size_t n = end != NULL ? (size_t)(end - p) : strlen(p);
        if (n == 8 && strncasecmp(p, "original", 8) == 0) return 1;
        if (end == NULL) break;
        p = end + 1;
    }
    return 0;
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* the light folder's rules, at open and again at Save */
static int check_folder(const char *dir, char *why, size_t cap)
{
    char sub[LIGHT_SESSION_PATH + 16];
    if (dir == NULL || dir[0] == '\0') return refuse(why, cap, "there is no light folder");
    if (under_original(dir)) return refuse(why, cap, "%s is under an original folder: the CD's files are only read", dir);
    if (snprintf(sub, sizeof sub, "%s/track_light", dir) >= (int)sizeof sub || !is_dir(sub)) return refuse(why, cap, "%s has no track_light folder: it is not a light folder", dir);
    return 1;
}

static int stem_ok(const char *track)
{
    return track != NULL && track[0] >= 'A' && track[0] <= 'Z' && track[1] >= 'A' && track[1] <= 'Z' && track[2] >= '0' && track[2] <= '9' && track[3] == '\0';
}

static char *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *text = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n >= 0 && fseek(f, 0, SEEK_SET) == 0 && (text = malloc((size_t)n + 1)) != NULL) {
            if (fread(text, 1, (size_t)n, f) == (size_t)n) text[n] = '\0', *size = (size_t)n;
            else free(text), text = NULL;
        }
    }
    fclose(f);
    return text;
}

void light_session_path(const LightSession *s, char *out, size_t cap)
{
    if (cap == 0) return;
    if (!s->has_file || snprintf(out, cap, "%s/track_light/%s%c.txt", s->dir, s->track, s->letter) >= (int)cap) out[0] = '\0';
}

/* the track's places file, when it is there and is the places of this track; anything else leaves the session without one (the time's file is the window's business) */
static void open_places(LightSession *s)
{
    char path[LIGHT_SESSION_PATH + 64], why[LIGHT_WHY];
    snprintf(path, sizeof path, "%s/track/%s.txt", s->dir, s->track);
    s->places_loaded = read_all(path, &s->places_loaded_size);
    if (s->places_loaded == NULL) return;
    if (memchr(s->places_loaded, '\0', s->places_loaded_size) == NULL && light_file_parse(s->places_loaded, s->places_loaded_size, &s->places, why, sizeof why) && s->places.letter == 0 && strcmp(s->places.track, s->track) == 0) {
        s->has_places = 1;
        return;
    }
    free(s->places_loaded);
    s->places_loaded = NULL;
    memset(&s->places, 0, sizeof s->places);
}

int light_session_open(LightSession *s, const char *dir, const char *track, char letter, char *why, size_t cap)
{
    memset(s, 0, sizeof *s);
    if (!check_folder(dir, why, cap)) return 0;
    if (!stem_ok(track)) return refuse(why, cap, "%s is not a CD track's stem such as CY1 or TR7", track != NULL ? track : "(none)");
    if (letter != 'A' && letter != 'B' && letter != 'C' && letter != 'N') return refuse(why, cap, "the time of day is A, B, C or N");
    if (snprintf(s->dir, sizeof s->dir, "%s", dir) >= (int)sizeof s->dir) return refuse(why, cap, "the light folder's path is too long");
    snprintf(s->track, sizeof s->track, "%s", track);
    s->letter = letter;
    char path[LIGHT_SESSION_PATH + 64];
    snprintf(path, sizeof path, "%s/track_light/%s%c.txt", s->dir, s->track, letter);
    s->loaded = read_all(path, &s->loaded_size);
    if (s->loaded == NULL) {
        if (errno == ENOENT) {
            refuse(why, cap, "%s has no light file for %s: this track's light is made in code, so there is nothing to edit", s->dir, s->track);
            return 1;
        }
        return refuse(why, cap, "cannot read %s: %s", path, strerror(errno));
    }
    if (!light_file_parse(s->loaded, s->loaded_size, &s->file, why, cap)) {
        free(s->loaded);
        s->loaded = NULL;
        return 0;
    }
    if (strcmp(s->file.track, s->track) != 0 || s->file.letter != letter) {
        refuse(why, cap, "%s is the light of %s at %c, not of %s at %c", path, s->file.track, s->file.letter != 0 ? s->file.letter : '-', s->track, letter);
        light_file_free(&s->file);
        free(s->loaded);
        s->loaded = NULL;
        return 0;
    }
    s->has_file = 1;
    open_places(s);
    return 1;
}

void light_session_close(LightSession *s)
{
    if (s->has_file) light_file_free(&s->file);
    if (s->has_places) light_file_free(&s->places);
    free(s->places_loaded);
    free(s->loaded);
    memset(s, 0, sizeof *s);
}

char *light_session_text(const LightSession *s, size_t *size)
{
    return s->has_file ? light_file_format(&s->file, size) : NULL;
}

static int differs(const LightFile *f, const char *loaded, size_t loaded_size)
{
    size_t n = 0;
    char *text = light_file_format(f, &n);
    const int same = text != NULL && n == loaded_size && memcmp(text, loaded, n) == 0;
    free(text);
    return !same;
}

char *light_session_places_text(const LightSession *s, size_t *size)
{
    return s->has_places ? light_file_format(&s->places, size) : NULL;
}

int light_session_dirty(const LightSession *s)
{
    if (!s->has_file) return 0;
    return differs(&s->file, s->loaded, s->loaded_size) || (s->has_places && differs(&s->places, s->places_loaded, s->places_loaded_size));
}

void light_session_revert(LightSession *s)
{
    if (!s->has_file) return;
    LightFile fresh;
    char why[LIGHT_WHY];
    if (!light_file_parse(s->loaded, s->loaded_size, &fresh, why, sizeof why)) return;   /* it parsed when it was loaded: keep the copy if it somehow does not */
    light_file_free(&s->file);
    s->file = fresh;
    if (!s->has_places) return;
    LightFile places;
    if (!light_file_parse(s->places_loaded, s->places_loaded_size, &places, why, sizeof why)) return;
    light_file_free(&s->places);
    s->places = places;
}

unsigned light_session_clamp_node(const LightSession *s, long node)
{
    if (!s->has_file || s->file.nodes == 0 || node <= 0) return 0;
    return node >= (long)s->file.nodes ? s->file.nodes - 1 : (unsigned)node;
}

/* `text` into `tmp` (a file next to `path`); 1, or 0 with the reason */
static int write_temp(const char *tmp, const char *text, size_t size, char *why, size_t cap)
{
    FILE *f = fopen(tmp, "wb");
    int ok = f != NULL && fwrite(text, 1, size, f) == size;
    if (f != NULL && fclose(f) != 0) ok = 0;
    if (!ok) {
        refuse(why, cap, "cannot write %s: %s", tmp, strerror(errno));
        unlink(tmp);
    }
    return ok;
}

int light_session_save(LightSession *s, char *why, size_t cap)
{
    if (!s->has_file) return refuse(why, cap, "there is no file to save: this track's light is made in code");
    if (!check_folder(s->dir, why, cap)) return 0;
    const int places_dirty = s->has_places && differs(&s->places, s->places_loaded, s->places_loaded_size);
    char places_path[LIGHT_SESSION_PATH + 64], places_tmp[LIGHT_SESSION_PATH + 72], folder[LIGHT_SESSION_PATH + 16];
    snprintf(folder, sizeof folder, "%s/track", s->dir);
    if (places_dirty && !is_dir(folder)) return refuse(why, cap, "%s has no track folder: the places cannot be saved, and nothing was written", s->dir);
    size_t size = 0, places_size = 0;
    char *text = light_file_format(&s->file, &size);
    char *places = places_dirty ? light_file_format(&s->places, &places_size) : NULL;
    if (text == NULL || (places_dirty && places == NULL)) {
        free(text);
        free(places);
        return refuse(why, cap, "out of memory");
    }
    char path[LIGHT_SESSION_PATH + 64], tmp[LIGHT_SESSION_PATH + 72];
    light_session_path(s, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    snprintf(places_path, sizeof places_path, "%s/track/%s.txt", s->dir, s->track);
    snprintf(places_tmp, sizeof places_tmp, "%s.tmp", places_path);
    int ok = write_temp(tmp, text, size, why, cap) && (!places_dirty || write_temp(places_tmp, places, places_size, why, cap));
    if (ok && rename(tmp, path) != 0) ok = refuse(why, cap, "cannot write %s: %s", path, strerror(errno));
    if (ok && places_dirty && rename(places_tmp, places_path) != 0) ok = refuse(why, cap, "cannot write %s: %s", places_path, strerror(errno));
    if (!ok) {
        unlink(tmp);
        unlink(places_tmp);
        free(text);
        free(places);
        return 0;
    }
    free(s->loaded);
    s->loaded = text;
    s->loaded_size = size;
    if (places_dirty) {
        free(s->places_loaded);
        s->places_loaded = places;
        s->places_loaded_size = places_size;
    }
    return 1;
}
