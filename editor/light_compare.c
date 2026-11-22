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

uint64_t light_compare_key(const char *track, char letter, unsigned node, int view, int hud, const char *file, size_t file_size)
{
    uint64_t h = 1469598103934665603ull;
    h = mix(h, track, strlen(track));
    h = mix(h, &letter, 1);
    h = mix(h, &node, sizeof node);
    h = mix(h, &view, sizeof view);
    h = mix(h, &hud, sizeof hud);
    h = mix(h, &file_size, sizeof file_size);
    if (file != NULL) h = mix(h, file, file_size);
    return h != 0 ? h : 1;
}

/* the light file a slot photographs: BEFORE, the session's as read or last saved, at the window's time; FOUR, the working copy at the slot's time (the file holds every time).
 * Malloc'd. */
static char *slot_text(const LightCompare *c, const LightTrackSession *s, char time, int slot, char *letter, size_t *size)
{
    if (c->mode == LIGHT_COMPARE_BEFORE) {
        *letter = time;
        *size = s->loaded_size;
        char *copy = malloc(s->loaded_size + 1);
        if (copy != NULL) memcpy(copy, s->loaded, s->loaded_size + 1);
        return copy;
    }
    *letter = light_compare_letter(slot);
    return light_track_session_text(s, size);
}

static void say(LightCompare *c, const char *why)
{
    snprintf(c->why, sizeof c->why, "%s", why);
}

/* starts slot's picture if its key is out of date; 1 when a game was started, 0 when the picture is current, -1 when it cannot be made (said in c->why, and not tried again
 * for the same key) */
static int start_slot(LightCompare *c, const LightTrackSession *s, char time, int slot, int node, int view, int hud, const char *game, const char *cd)
{
    char letter = 0;
    size_t size = 0;
    char *text = slot_text(c, s, time, slot, &letter, &size);
    const uint64_t key = light_compare_key(s->track, letter, (unsigned)node, view, hud, text, text != NULL ? size : 0);
    int result = 0;
    if (c->shown[slot] != key) {
        char why[LIGHT_PHOTO_WHY];
        if (text == NULL || size == 0) {
            snprintf(why, sizeof why, "there is no light file for %s at %c", s->track, letter);
            say(c, why);
            c->shown[slot] = key;
            c->missing[slot] = 1;
            result = -1;
        } else if ((snprintf(c->job[slot].masks, sizeof c->job[slot].masks, "%s", c->masks), 0) || !light_photo_start(&c->job[slot], game, cd, s->track, letter, (unsigned)node, view, hud, light_photo_view_ticks(view), text, size, why, sizeof why)) {
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
    return result;
}

int light_compare_poll(LightCompare *c, const LightTrackSession *s, char time, int node, int view, int hud, const char *game, const char *cd, int main_busy)
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
        if (start_slot(c, s, time, slot, node, view, hud, game, cd) == 1) return 1;
    return 0;
}
