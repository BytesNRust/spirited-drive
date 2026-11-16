/* See mode_choices.h. */
#include "game/frontend/mode_choices.h"
#include "game/sys/edition_file.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <string.h>

static int editions(char ids[][EDITION_ID_MAX + 1])
{
    return edition_list(ids, MODE_CHOICES_EDITIONS);
}

int mode_choices_count(void)
{
    char ids[MODE_CHOICES_EDITIONS][EDITION_ID_MAX + 1];
    return MODE_CHOICES_MODES + editions(ids);
}

int mode_choices_name_fits_width(const char *name, int width_px)
{
    const size_t n = strlen(name);
    return n >= 1 && n <= EDITION_NAME_MAX && width_px < MODE_CHOICES_TITLE_MAX_PX;
}

static int (*measure)(const char *text, int num, int den);

void mode_choices_measure(int (*width)(const char *text, int num, int den)) { measure = width; }

int mode_choices_name_fits(const char *name)
{
    return mode_choices_name_fits_width(name, measure != NULL ? measure(name, MODE_CHOICES_TITLE_NUM, MODE_CHOICES_TITLE_DEN) : 0);
}

static void plural(char *out, size_t cap, int n, const char *one)
{
    snprintf(out, cap, "%d %s%s", n, one, n == 1 ? "" : "s");
}

/* "A Restomod '26 edition - N cars - M tracks" (the owner's separator, as the records' "NORMAL - CHEATS"), the pieces that are there counted */
static void edition_line(const Edition *e, char *out, size_t cap)
{
    int cars = 0, tracks = 0;
    for (int i = 0; i < e->cars; i++) cars += edition_car_present(e->car[i]);
    for (int i = 0; i < e->tracks; i++) tracks += edition_track_present(e->track[i]);
    char c[24], t[24];
    plural(c, sizeof c, cars, "car"), plural(t, sizeof t, tracks, "track");
    snprintf(out, cap, "A Restomod '26 edition - %s - %s", c, t);
}

static void edition_choice(const char *id, ModeChoice *out)
{
    snprintf(out->id, sizeof out->id, "%s", id);
    snprintf(out->title, sizeof out->title, "%s", id);
    out->profile = PROFILE_RESTOMOD;
    Edition e;
    char why[MODE_CHOICES_LINE_CAP] = "";
    if (!edition_load(id, &e, why, sizeof why)) {
        snprintf(out->line1, sizeof out->line1, "%s", why);
        return;
    }
    if (!mode_choices_name_fits(e.name)) {
        snprintf(out->line1, sizeof out->line1, "Its name is too wide for this screen");
        return;
    }
    snprintf(out->title, sizeof out->title, "%s", e.name);
    if (!edition_usable(&e, why, sizeof why)) {
        snprintf(out->line1, sizeof out->line1, "%s", why);
        return;
    }
    edition_line(&e, out->line1, sizeof out->line1);
    out->usable = 1;
}

int mode_choices_get(int index, ModeChoice *out)
{
    memset(out, 0, sizeof *out);
    if (index < 0) return 0;
    if (index < MODE_CHOICES_MODES) {
        snprintf(out->title, sizeof out->title, "%s", port_profile_title((Profile)index));
        snprintf(out->line1, sizeof out->line1, "%s", port_profile_line1((Profile)index));
        snprintf(out->line2, sizeof out->line2, "%s", port_profile_line2((Profile)index));
        out->profile = index, out->usable = 1;
        return 1;
    }
    char ids[MODE_CHOICES_EDITIONS][EDITION_ID_MAX + 1];
    const int n = editions(ids);
    if (index - MODE_CHOICES_MODES >= n) return 0;
    edition_choice(ids[index - MODE_CHOICES_MODES], out);
    return 1;
}

int mode_choices_in_use(void)
{
    const PortSettings s = port_settings_load();
    if (s.profile != PROFILE_RESTOMOD) return s.profile;
    Edition e;
    if (s.edition[0] == 0 || edition_active(&e) != 1) return PROFILE_RESTOMOD;
    char ids[MODE_CHOICES_EDITIONS][EDITION_ID_MAX + 1];
    const int n = editions(ids);
    for (int i = 0; i < n; i++)
        if (strcmp(ids[i], s.edition) == 0) return MODE_CHOICES_MODES + i;
    return PROFILE_RESTOMOD;
}

/* Restomod's edition= set to `id` ("" the standard edition), its file written only when it changes */
static int edition_put(const char *id)
{
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    if (strcmp(s.edition, id) == 0) return 1;
    snprintf(s.edition, sizeof s.edition, "%s", id);
    const PortExtras x = port_extras_of_profile(PROFILE_RESTOMOD);
    if (port_mode_save(&s, &x)) return 1;
    port_log("port (dep59): edition: restomod.cfg could not be saved\n");
    return 0;
}

int mode_choices_apply(int index)
{
    ModeChoice c;
    if (!mode_choices_get(index, &c) || !c.usable) return 0;
    if (c.profile == PROFILE_RESTOMOD && !edition_put(c.id)) return 0;
    if (port_settings_load().profile != c.profile && !port_profile_select(c.profile)) {
        port_log("profile: the file could not be saved\n");
        return 0;
    }
    return 1;
}
