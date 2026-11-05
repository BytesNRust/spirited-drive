/* port (one time): an old port.cfg moves to the mode's file (port_old_files.h). */
#include "game/sys/port_old_files.h"
#include "game/platform/platform.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OLD_CFG_NAME "port.cfg"
#define OLD_CFG_COPY "port.cfg.bak"
#define OLD_EXTRAS_NAME "extras.cfg"

static void mark(void *to, const char *key, const char *value)
{
    (void)key, (void)value;
    *(int *)to = 1;
}

int port_old_files_carried(const char *text, size_t size)
{
    int carried = 0;
    port_profile_read_free(PROFILE_RESTOMOD, text, size, mark, &carried);   /* in Restomod everything but the machine's two is free */
    return carried;
}

/* the value of `key` in `text`, cut to `room`; 1 when the key is there */
static int value_of(const char *text, size_t size, const char *key, char *out, size_t room)
{
    const size_t key_len = strlen(key);
    for (size_t pos = 0; pos < size;) {
        const char *end = memchr(text + pos, '\n', size - pos);
        const size_t len = end != NULL ? (size_t)(end - (text + pos)) : size - pos;
        if (len > key_len && text[pos + key_len] == '=' && memcmp(text + pos, key, key_len) == 0) {
            size_t n = len - key_len - 1;
            if (n > 0 && text[pos + len - 1] == '\r') n--;
            if (n >= room) n = room - 1;
            memcpy(out, text + pos + key_len + 1, n);
            out[n] = '\0';
            return 1;
        }
        pos += len + 1;
    }
    return 0;
}

/* a named mode: its table over the file, as the game loaded it, but for full screen, which was the player's */
static void from_named(Profile p, const char *text, size_t size, const char *extras, size_t extras_size, PortSettings *s, PortExtras *e)
{
    const PortSettings own = port_settings_parse(text, size);
    char value[8];
    *s = port_profile_settings(p, own, port_monitor_format(SCREEN_16_9));
    if (value_of(text, size, "fullscreen", value, sizeof value)) s->fullscreen = own.fullscreen;
    *e = port_profile_extras(p, port_extras_parse(extras != NULL ? extras : "", extras_size));
}

/* custom: the file's values over the mode's table, so what the file does not carry takes the table's */
static void from_custom(Profile p, const char *text, size_t size, const char *extras, size_t extras_size, PortSettings *s, PortExtras *e)
{
    *s = port_mode_values(p, port_settings_parse(text, size).psx_dir);
    *e = port_profile_extras(p, port_extras_defaults());
    port_profile_read_free(p, text, size, port_settings_take, s);
    if (extras != NULL) port_profile_read_free(p, extras, extras_size, port_extras_take, e);
}

bool port_old_files_move(const char *text, size_t size)
{
    static bool failed;
    if (failed) return false;
    char name[16];
    const int named = value_of(text, size, "profile", name, sizeof name) ? port_profile_named(name) : -1;
    const Profile p = named >= 0 ? (Profile)named : PROFILE_RESTOMOD;

    size_t extras_size = 0, mode_size = 0;
    uint8_t *extras = platform_read_user_file(OLD_EXTRAS_NAME, &extras_size);
    uint8_t *mode_file = platform_read_user_file(port_profile_file(p), &mode_size);
    if (extras == NULL) extras_size = 0;
    if (mode_file == NULL) mode_size = 0;
    PortSettings s;
    PortExtras e;
    if (named >= 0) from_named(p, text, size, (const char *)extras, extras_size, &s, &e);
    else from_custom(p, text, size, (const char *)extras, extras_size, &s, &e);
    s.profile = p;
    if (mode_file != NULL) {   /* newer than port.cfg: its keys win */
        port_profile_read_free(p, (const char *)mode_file, mode_size, port_settings_take, &s);
        port_profile_read_free(p, (const char *)mode_file, mode_size, port_extras_take, &e);
    }
    /* custom was the player's own, key by key: all it set stays set, even at the table's value. A named mode's lines at its table's value were a snapshot the game ignored. */
    char *keep = malloc(size + extras_size + mode_size + 3);
    if (keep != NULL) {
        char *to = keep;
        if (named < 0) {
            memcpy(to, text, size), to += size, *to++ = '\n';
            if (extras != NULL) memcpy(to, extras, extras_size), to += extras_size, *to++ = '\n';
        }
        if (mode_file != NULL) memcpy(to, mode_file, mode_size), to += mode_size;
        *to = '\0';
    }
    if (extras != NULL) platform_free_file(extras);
    if (mode_file != NULL) platform_free_file(mode_file);

    /* in this order: stopped halfway, port.cfg still carries it all and the next run does it again. What is not free in the mode stays only in the copy, and is logged. */
    int dropped = 0;
    const bool ok = keep != NULL && platform_write_user_file(OLD_CFG_COPY, (const uint8_t *)text, size) && port_mode_write(&s, &e, keep, &dropped) && port_machine_save(p, s.psx_dir);
    free(keep);
    if (!ok) {
        failed = true;
        port_log("settings: could not move the old port.cfg to %s\n", port_profile_file(p));
        return false;
    }
    port_log("settings: the old port.cfg moved to %s, copy in %s\n", port_profile_file(p), OLD_CFG_COPY);
    return true;
}
