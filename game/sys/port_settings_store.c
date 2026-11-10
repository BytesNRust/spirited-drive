/* The port's settings files: the machine's port.cfg and the modes' files read, kept, written, chosen and reset (port_settings.h; split from port_settings.c by responsibility). */
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"
#include "game/race/session_rules.h"
#include "game/race/weather.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/viewer/screen_layout.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/sys/port_settings_private.h"

/* port.cfg: the mode's name and the machine's folder */
/* port (frame budget): the settings getters (port_car_tunnel_light ...) run per row of a drawn frame and each read port.cfg and the mode's file, parsed them, and made the user folder.
 * The two reads are kept until a writer here changes a file (settings_written): the values are the ones the files had when last written by the game. */
static struct {
    bool machine_have;
    PortSettings machine;
    bool mode_have[PROFILE_COUNT];
    PortSettings mode[PROFILE_COUNT];
} kept;

/* a file of settings was written or reset (or a test changed what the platform reads): the next read goes to the files */
void port_settings_forget(void)
{
    memset(&kept.machine_have, 0, sizeof kept.machine_have);
    memset(kept.mode_have, 0, sizeof kept.mode_have);
}

static PortSettings machine_read(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (data == NULL) return port_settings_defaults();
    const PortSettings s = port_settings_parse((const char *)data, size);
    platform_free_file(data);
    return s;
}

static PortSettings machine_load(void)
{
    if (!kept.machine_have) kept.machine = machine_read(), kept.machine_have = true;
    return kept.machine;
}

bool port_machine_save(int profile, const char *psx_dir)
{
    char text[PSX_DIR_MAX + 64];
    if (strpbrk(psx_dir, "\r\n") != NULL) {   /* a line end would write another key */
        port_log("settings: the PS1 folder has a line end, not saved\n");
        return false;
    }
    const int n = snprintf(text, sizeof text, "profile=%s\npsx_dir=%s\n", port_profile_key((Profile)profile), psx_dir);
    const bool written = n > 0 && (size_t)n < sizeof text && platform_write_user_file(PORT_CFG_NAME, (const uint8_t *)text, (size_t)n);
    port_settings_forget();
    return written;
}

/* the values of mode `p`'s table, with the machine's own */
PortSettings port_mode_values(int p, const char *psx_dir)
{
    PortSettings s = port_profile_settings((Profile)p, port_settings_defaults(), port_monitor_format(SCREEN_16_9));
    s.profile = p;
    snprintf(s.psx_dir, sizeof s.psx_dir, "%s", psx_dir);
    return s;
}

static PortSettings mode_read(int profile)
{
    const PortSettings machine = machine_load();
    PortSettings s = port_mode_values(profile, machine.psx_dir);
    size_t size = 0;
    uint8_t *data = platform_read_user_file(port_profile_file((Profile)profile), &size);
    if (data == NULL) return s;
    port_profile_read_free((Profile)profile, (const char *)data, size, port_settings_take, &s);
    platform_free_file(data);
    return s;
}

PortSettings port_settings_of_profile(int profile)
{
    if (profile < 0 || profile >= PROFILE_COUNT) return mode_read(profile);
    if (!kept.mode_have[profile]) kept.mode[profile] = mode_read(profile), kept.mode_have[profile] = true;
    return kept.mode[profile];
}

/* whether `text` has a line starting with `key=` */
static bool names(const char *text, size_t size, const char *key)
{
    const size_t len = strlen(key);
    for (size_t pos = 0; pos < size; pos++) {
        const size_t start = pos;
        while (pos < size && text[pos] != '\n')
            pos++;
        const size_t line = pos - start;
        if (line > len && strncmp(text + start, key, len) == 0 && text[start + len] == '=')
            return true;
    }
    return false;
}

PortSettings port_settings_load(void) { return port_settings_of_profile(machine_load().profile); }

int port_settings_profile(void) { return machine_load().profile; }

bool port_settings_profile_chosen(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (data == NULL) return false;
    const bool chosen = names((const char *)data, size, "profile");
    platform_free_file(data);
    return chosen;
}

bool port_mode_write(const PortSettings *s, const PortExtras *e, const char *keep, int *dropped)
{
    const Profile p = (Profile)s->profile;
    const PortSettings base = port_mode_values(s->profile, s->psx_dir);
    const PortExtras extras_base = port_profile_extras(p, port_extras_defaults());
    char now[PORT_CFG_TEXT_MAX], was[PORT_CFG_TEXT_MAX], extras_now[PORT_EXTRAS_TEXT_MAX], extras_was[PORT_EXTRAS_TEXT_MAX];
    char out[PORT_CFG_TEXT_MAX + PORT_EXTRAS_TEXT_MAX];
    if (port_settings_format(s, now, sizeof now) == 0 || port_settings_format(&base, was, sizeof was) == 0 ||
        port_extras_format(e, extras_now, sizeof extras_now) == 0 || port_extras_format(&extras_base, extras_was, sizeof extras_was) == 0) return false;
    const long settings = port_profile_differences(p, was, now, keep, out, sizeof out, dropped);
    if (settings < 0) return false;
    const long extras = port_profile_differences(p, extras_was, extras_now, keep, out + settings, sizeof out - (size_t)settings, dropped);
    if (extras < 0) return false;
    size_t n = (size_t)settings + (size_t)extras;
    if (n == 0) out[n++] = '\n';   /* nothing changed: an empty file, the mode's values */
    const bool written = platform_write_user_file(port_profile_file(p), (const uint8_t *)out, n);
    port_settings_forget();
    return written;
}

bool port_mode_save(const PortSettings *s, const PortExtras *e)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(port_profile_file((Profile)s->profile), &size);
    char *keep = data != NULL ? malloc(size + 1) : NULL;
    if (keep != NULL) {
        memcpy(keep, data, size);
        keep[size] = '\0';
    }
    if (data != NULL) platform_free_file(data);
    int dropped = 0;
    const bool written = port_mode_write(s, e, keep, &dropped);
    free(keep);
    return written;   /* what was not free in the mode is in the log (port_profile_differences) */
}

bool port_settings_save(const PortSettings *s)
{
    const PortSettings machine = machine_load();
    if (s->profile != machine.profile) {   /* a change of mode is port_profile_select's */
        port_log("settings: not saved: these are the settings of %s, the mode is %s\n", port_profile_key((Profile)s->profile), port_profile_key((Profile)machine.profile));
        return false;
    }
    if ((!port_settings_profile_chosen() || strcmp(s->psx_dir, machine.psx_dir) != 0) && !port_machine_save(s->profile, s->psx_dir)) return false;
    const PortExtras e = port_extras_load();
    return port_mode_save(s, &e);
}

bool port_profile_select(int p)
{
    if (p < 0 || p >= PROFILE_COUNT) return false;
    return port_machine_save(p, machine_load().psx_dir);
}

bool port_profile_reset(int p)
{
    if (p < 0 || p >= PROFILE_COUNT) return false;
    static const uint8_t EMPTY[] = "\n";
    const bool written = platform_write_user_file(port_profile_file((Profile)p), EMPTY, 1);
    port_settings_forget();
    return written;
}
