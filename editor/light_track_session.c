/* The Track light window's session on the new light file (light_track_session.h). */
#include "light_track_session.h"

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
int light_track_session_check_folder(const char *dir, char *why, size_t cap)
{
    char sub[LIGHT_TRACK_SESSION_PATH + 16];
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

void light_track_session_path(const LightTrackSession *s, char *out, size_t cap)
{
    if (cap == 0) return;
    if (!s->has_file || snprintf(out, cap, "%s/track_light/%s.txt", s->dir, s->track) >= (int)cap) out[0] = '\0';
}

int light_track_session_open(LightTrackSession *s, const char *dir, const char *track, unsigned nodes, char *why, size_t cap)
{
    memset(s, 0, sizeof *s);
    if (!light_track_session_check_folder(dir, why, cap)) return 0;
    if (!stem_ok(track)) return refuse(why, cap, "%s is not a CD track's stem such as CY1 or TR7", track != NULL ? track : "(none)");
    if (nodes == 0) return refuse(why, cap, "the track's node count is not known (is the game's folder set?)");
    if (snprintf(s->dir, sizeof s->dir, "%s", dir) >= (int)sizeof s->dir) return refuse(why, cap, "the light folder's path is too long");
    snprintf(s->track, sizeof s->track, "%s", track);
    s->nodes = nodes;
    char path[LIGHT_TRACK_SESSION_PATH + 64];
    snprintf(path, sizeof path, "%s/track_light/%s.txt", s->dir, s->track);
    s->loaded = read_all(path, &s->loaded_size);
    if (s->loaded == NULL) {
        if (errno == ENOENT) {
            refuse(why, cap, "%s has no light file for %s: this track's light is made in code, so there is nothing to edit", s->dir, s->track);
            return 1;
        }
        return refuse(why, cap, "cannot read %s: %s", path, strerror(errno));
    }
    if (!light_track_parse(s->loaded, s->loaded_size, nodes, &s->file, why, cap)) {
        free(s->loaded);
        s->loaded = NULL;
        return 0;
    }
    if (strcmp(s->file.track, s->track) != 0) {
        refuse(why, cap, "%s is the light of %s, not of %s", path, s->file.track, s->track);
        light_track_free(&s->file);
        free(s->loaded);
        s->loaded = NULL;
        return 0;
    }
    s->has_file = 1;
    return 1;
}

void light_track_session_close(LightTrackSession *s)
{
    if (s->has_file) light_track_free(&s->file);
    free(s->loaded);
    memset(s, 0, sizeof *s);
}

char *light_track_session_text(const LightTrackSession *s, size_t *size)
{
    return s->has_file ? light_track_format(&s->file, size) : NULL;
}

void light_track_session_changed(LightTrackSession *s)
{
    s->dirty_known = 0;
}

int light_track_session_dirty(const LightTrackSession *s)
{
    if (!s->has_file) return 0;
    if (s->dirty_known) return s->dirty_value;
    LightTrackSession *keep = (LightTrackSession *)s;   // the kept answer is a cache, not state
    size_t n = 0;
    char *text = light_track_format(&s->file, &n);
    const int same = text != NULL && n == s->loaded_size && memcmp(text, s->loaded, n) == 0;
    free(text);
    keep->dirty_known = 1, keep->dirty_value = !same;
    return !same;
}

void light_track_session_revert(LightTrackSession *s)
{
    if (!s->has_file) return;
    LightTrack fresh;
    char why[LT_WHY];
    if (!light_track_parse(s->loaded, s->loaded_size, s->nodes, &fresh, why, sizeof why)) return;   /* it parsed when it was loaded: keep the copy if it somehow does not */
    light_track_free(&s->file);
    s->file = fresh;
    s->dirty_known = 0;
}

unsigned light_track_session_clamp_node(const LightTrackSession *s, long node)
{
    if (!s->has_file || s->nodes == 0 || node <= 0) return 0;
    return node >= (long)s->nodes ? s->nodes - 1 : (unsigned)node;
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

int light_track_session_save(LightTrackSession *s, char *why, size_t cap)
{
    if (!s->has_file) return refuse(why, cap, "there is no file to save: this track's light is made in code");
    if (!light_track_session_check_folder(s->dir, why, cap)) return 0;
    size_t size = 0;
    char *text = light_track_format(&s->file, &size);
    if (text == NULL) return refuse(why, cap, "out of memory");
    char path[LIGHT_TRACK_SESSION_PATH + 64], tmp[LIGHT_TRACK_SESSION_PATH + 72];
    light_track_session_path(s, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    int ok = write_temp(tmp, text, size, why, cap);
    if (ok && rename(tmp, path) != 0) ok = refuse(why, cap, "cannot write %s: %s", path, strerror(errno));
    if (!ok) {
        unlink(tmp);
        free(text);
        return 0;
    }
    free(s->loaded);
    s->loaded = text;
    s->loaded_size = size;
    s->dirty_known = 1, s->dirty_value = 0;
    ++s->saves;
    return 1;
}
