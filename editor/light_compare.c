/* The Track light window's comparison pictures (light_compare.h). */
#include "light_compare.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void light_compare_init(LightCompare *c)
{
    memset(c, 0, sizeof *c);
    c->running = -1;
}

void light_compare_free(LightCompare *c)
{
    for (int i = 0; i < LIGHT_COMPARE_SLOTS; ++i) light_photo_free(&c->job[i]);
    light_compare_init(c);
}

int light_compare_count(int mode)
{
    return mode == LIGHT_COMPARE_BEFORE ? 1 : mode == LIGHT_COMPARE_FOUR ? LIGHT_COMPARE_SLOTS : 0;
}

char light_compare_letter(int slot)
{
    return "ABCN"[slot < 0 ? 0 : slot > 3 ? 3 : slot];
}

static uint64_t mix(uint64_t h, const void *bytes, size_t n)
{
    const unsigned char *p = bytes;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

uint64_t light_compare_key(const char *track, char letter, unsigned node, int view, const char *file, size_t file_size, const char *places, size_t places_size)
{
    uint64_t h = 1469598103934665603ull;
    h = mix(h, track, strlen(track));
    h = mix(h, &letter, 1);
    h = mix(h, &node, sizeof node);
    h = mix(h, &view, sizeof view);
    h = mix(h, &file_size, sizeof file_size);
    if (file != NULL) h = mix(h, file, file_size);
    h = mix(h, &places_size, sizeof places_size);
    if (places != NULL) h = mix(h, places, places_size);
    return h != 0 ? h : 1;
}

static char *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *text = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n > 0 && fseek(f, 0, SEEK_SET) == 0 && (text = malloc((size_t)n + 1)) != NULL) {
            if (fread(text, 1, (size_t)n, f) == (size_t)n) text[n] = '\0', *size = (size_t)n;
            else free(text), text = NULL;
        }
    }
    fclose(f);
    return text;
}

/* the light file a slot photographs: BEFORE, the session's as read or last saved; FOUR, the working copy for the session's time and the folder's file for another (NULL
 * when the folder has none). Malloc'd. */
static char *slot_text(const LightCompare *c, const LightSession *s, int slot, char *letter, size_t *size)
{
    if (c->mode == LIGHT_COMPARE_BEFORE) {
        *letter = s->letter;
        *size = s->loaded_size;
        char *copy = malloc(s->loaded_size + 1);
        if (copy != NULL) memcpy(copy, s->loaded, s->loaded_size + 1);
        return copy;
    }
    *letter = light_compare_letter(slot);
    if (*letter == s->letter) return light_session_text(s, size);
    char path[LIGHT_SESSION_PATH + 64];
    snprintf(path, sizeof path, "%s/track_light/%s%c.txt", s->dir, s->track, *letter);
    return read_all(path, size);
}

/* the places file a slot is photographed with: the working copy, except BEFORE's, which is as read or last saved. Malloc'd or NULL. */
static char *slot_places(const LightCompare *c, const LightSession *s, size_t *size)
{
    if (!s->has_places) return NULL;
    if (c->mode == LIGHT_COMPARE_BEFORE) {
        *size = s->places_loaded_size;
        char *copy = malloc(s->places_loaded_size + 1);
        if (copy != NULL) memcpy(copy, s->places_loaded, s->places_loaded_size + 1);
        return copy;
    }
    return light_session_places_text(s, size);
}

static void say(LightCompare *c, const char *why)
{
    snprintf(c->why, sizeof c->why, "%s", why);
}

/* starts slot's picture if its key is out of date; 1 when a game was started, 0 when the picture is current, -1 when it cannot be made (said in c->why, and not tried again
 * for the same key) */
static int start_slot(LightCompare *c, const LightSession *s, int slot, int node, int view, const char *game, const char *cd)
{
    char letter = 0;
    size_t size = 0, places_size = 0;
    char *text = slot_text(c, s, slot, &letter, &size);
    char *places = slot_places(c, s, &places_size);
    const uint64_t key = light_compare_key(s->track, letter, (unsigned)node, view, text, text != NULL ? size : 0, places, places != NULL ? places_size : 0);
    int result = 0;
    if (c->shown[slot] != key) {
        char why[LIGHT_PHOTO_WHY];
        if (text == NULL || size == 0) {
            snprintf(why, sizeof why, "there is no light file for %s at %c", s->track, letter);
            say(c, why);
            c->shown[slot] = key;
            c->missing[slot] = 1;
            result = -1;
        } else if (!light_photo_start(&c->job[slot], game, cd, s->track, letter, (unsigned)node, view, light_photo_view_ticks(view), text, size, places, places_size, why, sizeof why)) {
            say(c, why);
            c->shown[slot] = key;   /* tried: a refusal is not tried again every frame */
            result = -1;
        } else {
            c->started[slot] = key;
            c->missing[slot] = 0;
            c->running = slot;
            c->why[0] = '\0';
            result = 1;
        }
    }
    free(text);
    free(places);
    return result;
}

int light_compare_poll(LightCompare *c, const LightSession *s, int node, int view, const char *game, const char *cd, int main_busy)
{
    if (c->running >= 0) {
        LightPhotoJob *job = &c->job[c->running];
        if (light_photo_poll(job)) return 1;
        if (job->state == LIGHT_PHOTO_FAILED && strcmp(job->why, "stopped") != 0) say(c, job->why);
        c->shown[c->running] = c->started[c->running];   /* made, or failed: the same picture is not asked for again */
        c->running = -1;
    }
    if (c->mode == LIGHT_COMPARE_OFF || !s->has_file) return 0;
    if (main_busy) return 1;
    for (int slot = 0; slot < light_compare_count(c->mode); ++slot)
        if (start_slot(c, s, slot, node, view, game, cd) == 1) return 1;
    return 0;
}
