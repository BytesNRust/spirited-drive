/* One time, at startup: a port.cfg that still holds settings moves to the mode's file, and copies stay as .bak (port_old_files.h). */
#include "game/sys/port_old_files.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[32];
    uint8_t bytes[PORT_CFG_TEXT_MAX];
    size_t size;
    int present;
} Stored;

enum { FILES = 8 };
static Stored stored[FILES];
static int writes_fail, writes;


static Stored *slot_for(const char *name)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].name[0] != '\0' && strcmp(stored[i].name, name) == 0) return &stored[i];
    for (int i = 0; i < FILES; i++)
        if (stored[i].name[0] == '\0') {
            strcpy(stored[i].name, name);
            return &stored[i];
        }
    return &stored[0];
}

static void put_file(const char *name, const char *text)
{
    Stored *s = slot_for(name);
    snprintf((char *)s->bytes, sizeof s->bytes, "%s", text);
    s->size = strlen(text);
    s->present = 1;
}

/* the file's text, or NULL when it is not there */
static const char *file_text(const char *name)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            stored[i].bytes[stored[i].size < sizeof stored[i].bytes ? stored[i].size : sizeof stored[i].bytes - 1] = 0;
            return (const char *)stored[i].bytes;
        }
    return NULL;
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    for (int i = 0; i < FILES; i++)
        if (stored[i].present && strcmp(name, stored[i].name) == 0) {
            uint8_t *copy = malloc(stored[i].size + 1);
            memcpy(copy, stored[i].bytes, stored[i].size);
            *size = stored[i].size;
            return copy;
        }
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    writes++;
    if (writes_fail) return false;
    Stored *s = slot_for(name);
    if (size >= sizeof s->bytes) return false;
    memcpy(s->bytes, data, size);
    s->size = size;
    s->present = 1;
    return true;
}

bool platform_remove_user_file(const char *name)
{
    Stored *s = slot_for(name);
    s->present = 0;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static bool probe(int *w, int *h)
{
    *w = 3440, *h = 1440;
    return true;
}

static int has_line(const char *name, const char *line)
{
    const char *text = file_text(name);
    if (text == NULL) return 0;
    const size_t len = strlen(line);
    for (const char *at = strstr(text, line); at != NULL; at = strstr(at + 1, line))
        if ((at == text || at[-1] == '\n') && (at[len] == '\n' || at[len] == '\0')) return 1;
    return 0;
}

static void test_custom_goes_to_restomod(void)
{
    memset(stored, 0, sizeof stored);
    static const char OLD[] = "format=21:9\nprofile=custom\npsx_dir=/ps1\ncamera_kind=3\nlaps=5\nfog=0\nfullscreen=0\n";
    put_file("port.cfg", OLD);
    put_file("extras.cfg", "nitro=1\nunits=kmh\nfollow_camera=0\n");
    port_old_files_migrate();
    const PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.profile, PROFILE_RESTOMOD);   /* the only mode where all of it stays free */
    CHECK_EQ_INT(s.camera_kind, 3);
    CHECK_EQ_INT(s.laps, 5);
    CHECK_EQ_INT(s.fog, 0);
    CHECK_EQ_INT(s.fullscreen, 0);
    CHECK(strcmp(s.psx_dir, "/ps1") == 0);
    CHECK_EQ_INT(s.city_lift, 3);   /* what the old file did not carry takes the mode's value */
    const PortExtras e = port_extras_load();
    CHECK_EQ_INT(e.nitro, 1);
    CHECK_EQ_INT(e.follow_camera, 0);
    /* port.cfg is left with the machine's two, and the copy is the old file byte for byte */
    CHECK(strcmp(file_text("port.cfg"), "profile=restomod\npsx_dir=/ps1\n") == 0);
    CHECK(strcmp(file_text("port.cfg.bak"), OLD) == 0);
    CHECK(has_line("restomod.cfg", "camera_kind=3"));
    CHECK(has_line("restomod.cfg", "nitro=1"));
    CHECK(!has_line("restomod.cfg", "city_lift=3"));   /* the file did not set it: a later table reaches it */
    CHECK(file_text("stock.cfg") == NULL);
    CHECK(port_settings_profile_chosen());
    /* extras.cfg goes, with its copy */
    CHECK(file_text("extras.cfg") == NULL);
    CHECK(strcmp(file_text("extras.cfg.bak"), "nitro=1\nunits=kmh\nfollow_camera=0\n") == 0);
}

/* a key custom set at the table's value is the player's choice: it stays when the table changes, and through a later save */
static void test_custom_pins_what_it_set(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=custom\nfog=1\ncamera_kind=3\n");
    put_file("extras.cfg", "no_movies=0\n");
    port_old_files_migrate();
    PortSettings s = port_settings_load();
    CHECK(has_line("restomod.cfg", "fog=1"));
    CHECK(has_line("restomod.cfg", "no_movies=0"));
    s.laps = 4;
    CHECK(port_settings_save(&s));
    CHECK(has_line("restomod.cfg", "fog=1"));
    CHECK(has_line("restomod.cfg", "laps=4"));
}

static void test_it_runs_once(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=custom\ncamera_kind=3\n");
    port_old_files_migrate();
    /* the player's later change is not undone by a later start */
    put_file("restomod.cfg", "camera_kind=1\n");
    const int before = writes;
    port_old_files_migrate();
    CHECK_EQ_INT(port_settings_load().camera_kind, 1);
    CHECK_EQ_INT(writes, before);
}

static void test_no_profile_line_was_custom(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "camera_kind=2\n");
    CHECK(!file_text("port.cfg.bak"));
    port_old_files_migrate();
    CHECK_EQ_INT(port_settings_load().camera_kind, 2);
    CHECK_EQ_INT(port_settings_profile(), PROFILE_RESTOMOD);
    CHECK(file_text("port.cfg.bak") != NULL);
}

/* a copy already there is the player's original: it stays */
static void test_copies_are_kept(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=custom\ncamera_kind=2\n");
    put_file("extras.cfg", "nitro=1\n");
    put_file("port.cfg.bak", "first\n");
    put_file("extras.cfg.bak", "first too\n");
    port_old_files_migrate();
    CHECK(strcmp(file_text("port.cfg.bak"), "first\n") == 0);
    CHECK(strcmp(file_text("extras.cfg.bak"), "first too\n") == 0);
    CHECK(file_text("extras.cfg") == NULL);
    CHECK_EQ_INT(port_settings_load().camera_kind, 2);
}

/* a named mode took its table's values whatever the file said; what it left alone moves when it is free in that mode */
static void test_named_mode_keeps_its_table(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=restomod\ncity_lift=1\nrefresh_hz=60\ncamera_kind=3\nfullscreen=0\n");
    port_old_files_migrate();
    const PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.profile, PROFILE_RESTOMOD);
    CHECK_EQ_INT(s.city_lift, 3);   /* a snapshot of an older table: never the player's choice */
    CHECK_EQ_INT(s.refresh_hz, 144);
    CHECK_EQ_INT(s.camera_kind, 3);
    CHECK_EQ_INT(s.fullscreen, 0);
    CHECK(!has_line("restomod.cfg", "city_lift=1"));

    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=restored\ncamera_kind=3\nfullscreen=0\n");
    put_file("extras.cfg", "nitro=1\nno_movies=1\n");
    port_old_files_migrate();
    const PortSettings r = port_settings_load();
    CHECK_EQ_INT(r.profile, PROFILE_RESTORED);
    CHECK_EQ_INT(r.camera_kind, -1);   /* not free in Restored; the copy in port.cfg.bak still has it */
    CHECK_EQ_INT(r.fullscreen, 0);
    CHECK_EQ_INT(port_extras_load().nitro, 1);
    CHECK(has_line("port.cfg.bak", "camera_kind=3"));
    CHECK(!has_line("restored.cfg", "camera_kind=3"));
    CHECK(!has_line("restored.cfg", "no_movies=1"));
}

/* a mode's file already there is newer than the old port.cfg: its keys win, the others are added */
static void test_mode_file_wins(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=restomod\nlaps=5\ncamera_kind=3\n");
    put_file("restomod.cfg", "camera_kind=1\nfog=0\n");
    port_old_files_migrate();
    const PortSettings s = port_settings_load();
    CHECK_EQ_INT(s.camera_kind, 1);
    CHECK_EQ_INT(s.fog, 0);
    CHECK_EQ_INT(s.laps, 5);
}

static void test_clean_port_cfg_stays(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=stock\npsx_dir=/ps1\n");
    put_file("extras.cfg", "nitro=1\n");
    const int before = writes;
    port_old_files_migrate();
    CHECK_EQ_INT(port_settings_load().profile, PROFILE_STOCK);
    CHECK_EQ_INT(writes, before);
    CHECK(file_text("port.cfg.bak") == NULL);
    CHECK(file_text("stock.cfg") == NULL);
    CHECK(file_text("extras.cfg") != NULL);   /* nothing moved: nothing goes */
}

/* a folder that cannot be written: port.cfg and extras.cfg are as they were, and the next start tries again */
static void test_a_failed_move(void)
{
    memset(stored, 0, sizeof stored);
    put_file("port.cfg", "profile=custom\ncamera_kind=3\n");
    put_file("extras.cfg", "nitro=1\n");
    writes_fail = 1;
    port_old_files_migrate();
    writes_fail = 0;
    CHECK(has_line("port.cfg", "camera_kind=3"));
    CHECK(has_line("extras.cfg", "nitro=1"));
    port_old_files_migrate();
    CHECK_EQ_INT(port_settings_load().camera_kind, 3);
    CHECK(file_text("extras.cfg") == NULL);
}

int main(void)
{
    port_settings_set_display_probe(probe);
    test_custom_goes_to_restomod();
    test_custom_pins_what_it_set();
    test_it_runs_once();
    test_no_profile_line_was_custom();
    test_copies_are_kept();
    test_named_mode_keeps_its_table();
    test_mode_file_wins();
    test_clean_port_cfg_stays();
    test_a_failed_move();
    return TEST_REPORT();
}
