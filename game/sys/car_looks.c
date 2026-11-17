/* See car_looks.h. */
#include "game/sys/car_looks.h"

#include "game/platform/platform.h"
#include "game/race/car_registry.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/sys/track_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { KEY_MAX = 64, VALUE_MAX = CAR_LOOKS_SKIN_NAME_MAX + 1 + MOD_INI_PAINT_NAME_MAX + 1 };
static const char LOOK[] = "look.";

static int restomod(void) { return port_settings_profile() == PROFILE_RESTOMOD; }

/* Restomod's file as a NUL-terminated copy ("" when there is none); free it */
static char *file_text(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(port_profile_file(PROFILE_RESTOMOD), &size);
    char *text = malloc(size + 1);
    if (text == NULL) {
        if (data != NULL) platform_free_file(data);
        return NULL;
    }
    if (data != NULL) memcpy(text, data, size), platform_free_file(data);
    text[data != NULL ? size : 0] = '\0';
    return text;
}

/* the line from `p` (its end, without CR LF, at *end); the position after it */
static const char *line_at(const char *p, const char **end)
{
    const char *nl = strchr(p, '\n');
    const char *e = nl != NULL ? nl : p + strlen(p);
    *end = e > p && e[-1] == '\r' ? e - 1 : e;
    return nl != NULL ? nl + 1 : e;
}

/* 1 when the line `s`..`e` is `key`=...; its value then at *value */
static int line_is(const char *s, const char *e, const char *key, const char **value)
{
    const size_t n = strlen(key);
    if ((size_t)(e - s) <= n || memcmp(s, key, n) != 0 || s[n] != '=') return 0;
    *value = s + n + 1;
    return 1;
}

/* `key`'s value in Restomod's file into `out`; 1 when there is one */
static int value_of(const char *key, char *out, size_t cap)
{
    char *text = file_text();
    int found = 0;
    for (const char *p = text, *next; p != NULL && *p != '\0' && !found; p = next) {
        const char *e, *v;
        next = line_at(p, &e);
        if (line_is(p, e, key, &v) && (size_t)(e - v) < cap) memcpy(out, v, (size_t)(e - v)), out[e - v] = '\0', found = 1;
    }
    free(text);
    return found;
}

/* Restomod's file with `key`'s line as `key`=`value` (in its place, or last), or taken out when `value` is NULL */
static bool put(const char *key, const char *value)
{
    char *text = file_text();
    if (text == NULL) return false;
    const size_t room = strlen(text) + strlen(key) + (value != NULL ? strlen(value) : 0) + 4;
    char *out = malloc(room);
    if (out == NULL) return free(text), false;
    size_t n = 0;
    int done = value == NULL;
    for (const char *p = text, *next; *p != '\0'; p = next) {
        const char *e, *v;
        next = line_at(p, &e);
        if (line_is(p, e, key, &v)) {
            if (!done) n += (size_t)snprintf(out + n, room - n, "%s=%s\n", key, value), done = 1;
            continue;
        }
        if (e > p) memcpy(out + n, p, (size_t)(e - p)), n += (size_t)(e - p), out[n++] = '\n';
    }
    if (!done) n += (size_t)snprintf(out + n, room - n, "%s=%s\n", key, value);
    if (n == 0) out[n++] = '\n';   /* nothing left: the mode's values, as port_mode_write leaves it */
    const bool written = platform_write_user_file(port_profile_file(PROFILE_RESTOMOD), (const uint8_t *)out, n);
    free(out), free(text);
    port_settings_forget();   /* the mode's file changed: nothing read from it before is kept */
    if (!written) port_log("car looks: %s could not be written\n", port_profile_file(PROFILE_RESTOMOD));
    return written;
}

/* a skin's name: 1..16 of a-z 0-9 '-' */
static int skin_ok(const char *s)
{
    const size_t n = strlen(s);
    if (n == 0 || n > CAR_LOOKS_SKIN_NAME_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-')) return 0;
    return 1;
}

/* a paint's name as a card may give it (mod_ini.h): 1..24 bytes of text without ':' or ',' */
static int paint_ok(const char *s)
{
    const size_t n = strlen(s);
    if (n == 0 || n > MOD_INI_PAINT_NAME_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || s[i] == 0x7f || s[i] == ':' || s[i] == ',') return 0;
    return 1;
}

int car_looks_saved_car(int opponent)
{
    char id[MOD_INI_ID_MAX + 2];
    if (!restomod() || !value_of(opponent ? "opponent" : "car", id, sizeof id)) return -1;
    const int car = car_registry_find(id);
    return car >= 0 ? car : CAR_LOOKS_NOT_LOADED;
}

bool car_looks_save_car(int opponent, int car)
{
    if (!restomod()) return false;
    const CarPiece *p = car_registry_piece(car);
    return put(opponent ? "opponent" : "car", p != NULL ? p->id : NULL);
}

int car_looks_saved_lunar(void)
{
    char value[8];
    return restomod() && value_of("track", value, sizeof value) && strcmp(value, "lunar") == 0;
}

int car_looks_saved_track(void)
{
    char id[MOD_INI_ID_MAX + 2];
    if (!restomod() || !value_of("track", id, sizeof id) || strcmp(id, "lunar") == 0) return -1;
    const int track = track_registry_find(id);
    return track >= 0 ? track : CAR_LOOKS_NOT_LOADED;
}

bool car_looks_save_track(int track)
{
    const TrackPiece *p = track_registry_piece(track);
    if (p == NULL && car_looks_saved_lunar()) return restomod();   /* no added track to write: Lunar's line stays */
    return restomod() && put("track", p != NULL ? p->id : NULL);
}

bool car_looks_save_lunar(int lunar)
{
    if (!lunar && car_looks_saved_track() != -1) return restomod();   /* an added track's line is not Lunar's to take out */
    return restomod() && put("track", lunar ? "lunar" : NULL);
}

CarLook car_looks_get(const char *id)
{
    CarLook l = { { 0 }, { 0 } };
    char key[KEY_MAX], value[VALUE_MAX + 1];
    if (!restomod() || id == NULL || !mod_ini_id_ok(id)) return l;
    snprintf(key, sizeof key, "%s%s", LOOK, id);
    if (!value_of(key, value, sizeof value)) return l;
    char *comma = strchr(value, ',');
    const char *paint = comma != NULL ? comma + 1 : "-";
    if (comma != NULL) *comma = '\0';
    if (strcmp(value, "-") != 0 && skin_ok(value)) snprintf(l.skin, sizeof l.skin, "%s", value);   /* a bad name: as it comes */
    if (strcmp(paint, "-") != 0 && paint_ok(paint)) snprintf(l.paint, sizeof l.paint, "%s", paint);
    return l;
}

bool car_looks_set(const char *id, const CarLook *look)
{
    if (!restomod() || id == NULL || look == NULL || !mod_ini_id_ok(id)) return false;
    if ((look->skin[0] != '\0' && !skin_ok(look->skin)) || (look->paint[0] != '\0' && !paint_ok(look->paint))) return false;
    char key[KEY_MAX], value[VALUE_MAX + 2];
    snprintf(key, sizeof key, "%s%s", LOOK, id);
    if (look->skin[0] == '\0' && look->paint[0] == '\0') return put(key, NULL);
    snprintf(value, sizeof value, "%s,%s", look->skin[0] != '\0' ? look->skin : "-", look->paint[0] != '\0' ? look->paint : "-");
    return put(key, value);
}

int car_looks_key(const char *key)
{
    if (strcmp(key, "car") == 0 || strcmp(key, "opponent") == 0 || strcmp(key, "track") == 0) return 1;
    return strncmp(key, LOOK, sizeof LOOK - 1) == 0 && mod_ini_id_ok(key + sizeof LOOK - 1);
}
