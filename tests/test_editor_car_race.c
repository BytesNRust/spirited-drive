/* Test drive and In-game photo (editor/car_race.c, proc_run.c): the command line and environment built for each kind of car and each preset, the scratch player folder's
 * files, the log's lines, the slot a car has, and the whole job run against a fake game (a shell script that writes the lines the real one does), so no game data is
 * needed. */
#include "car_race.h"
#include "formats/car_paint.h"
#include "test.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512];

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static int index_of(const char *folder)
{
    for (int i = 0; car_export_car(i) != NULL; ++i)
        if (strcmp(car_export_car(i)->folder, folder) == 0) return i;
    return -1;
}

static int has_arg(const RaceCommand *c, const char *word)
{
    for (int i = 0; i < c->argc; ++i)
        if (strcmp(c->argv[i], word) == 0) return 1;
    return 0;
}

static int has_env(const RaceCommand *c, const char *entry)
{
    for (int i = 0; i < c->envc; ++i)
        if (strcmp(c->envp[i], entry) == 0) return 1;
    return 0;
}

static int has_env_key(const RaceCommand *c, const char *key)
{
    const size_t n = strlen(key);
    for (int i = 0; i < c->envc; ++i)
        if (strncmp(c->envp[i], key, n) == 0) return 1;
    return 0;
}

/* 1 when `flag` is followed by `value` in the arguments */
static int arg_pair(const RaceCommand *c, const char *flag, const char *value)
{
    for (int i = 0; i + 1 < c->argc; ++i)
        if (strcmp(c->argv[i], flag) == 0 && strcmp(c->argv[i + 1], value) == 0) return 1;
    return 0;
}

static RaceSetup setup_of(const char *folder, const char *track, int night, int ticks)
{
    RaceSetup s = { 0 };
    s.game = "/g/tnfs", s.cd = "/cd", s.project = "/p/proj", s.user = "/tmp/u", s.car = car_export_car(index_of(folder)), s.track = track, s.night = night, s.photo_ticks = ticks, s.shot = "/tmp/f.ppm";
    return s;
}

static void test_command(void)
{
    char *base_env[] = { "HOME=/h", "TNFS_CAR_MASKS=/elsewhere", "TNFS_VIEW=1", "SDL_VIDEO_DRIVER=cocoa", "PATH=/bin", NULL };
    RaceCommand c;
    char why[300];
    /* a car you drive, a photo: the car's number, alone, the project's masks and dash masks, the headless drivers, the follow view */
    RaceSetup s = setup_of("P911", "TR5", 1, RACE_TICKS_DRIVEN);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(strcmp(c.argv[0], "/g/tnfs") == 0 && c.argv[c.argc] == NULL && c.envp[c.envc] == NULL);
    CHECK(arg_pair(&c, "--data", "/cd") && arg_pair(&c, "--user", "/tmp/u") && arg_pair(&c, "--drive", "TR5") && arg_pair(&c, "--car", "2") && arg_pair(&c, "--racers", "1"));
    CHECK(arg_pair(&c, "--node", "200") && arg_pair(&c, "--screenshot", "/tmp/f.ppm") && !has_arg(&c, "--type") && !has_arg(&c, "--mods"));
    CHECK(has_env(&c, "TNFS_CAR_MASKS=/p/proj/car_masks") && has_env(&c, "TNFS_DASH_MASKS=/p/proj/dash_masks"));
    CHECK(has_env(&c, "TNFS_VIEW=3") && has_env(&c, "SDL_VIDEO_DRIVER=dummy") && has_env(&c, "SDL_AUDIO_DRIVER=dummy") && has_env(&c, "SDL_RENDER_DRIVER=software"));
    CHECK(has_env(&c, "HOME=/h") && has_env(&c, "PATH=/bin"));
    CHECK(!has_env(&c, "TNFS_CAR_MASKS=/elsewhere") && !has_env(&c, "TNFS_VIEW=1") && !has_env(&c, "SDL_VIDEO_DRIVER=cocoa"));   /* the person's own do not change the test */
    CHECK(!has_env_key(&c, "TNFS_FOLLOW_CAR") && !has_env_key(&c, "TNFS_DRIVE_SEED"));
    car_race_command_free(&c);
    /* the cockpit photo (dep98 item 2): the dash view (view kind 0) and the dash's gauge light, for a car you drive; the default keeps the follow view and the
     * profile's own gauge light, and a traffic or police car has no cockpit */
    s = setup_of("P911", "TR5", 1, RACE_TICKS_DRIVEN);
    s.cockpit = 1, s.gauge_set = 1, s.gauge_light = 5;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(has_env(&c, "TNFS_VIEW=0") && !has_env(&c, "TNFS_VIEW=3") && has_env(&c, "TNFS_GAUGE_LIGHT=5") && has_env(&c, "TNFS_DASH_MASKS=/p/proj/dash_masks"));
    car_race_command_free(&c);
    s.gauge_set = 0;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(has_env(&c, "TNFS_VIEW=0") && !has_env_key(&c, "TNFS_GAUGE_LIGHT"));   /* none chosen: the profile's own */
    car_race_command_free(&c);
    s = setup_of("P911", "TR5", 1, RACE_TICKS_DRIVEN);
    s.gauge_set = 1, s.gauge_light = 0;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(has_env(&c, "TNFS_VIEW=3") && has_env(&c, "TNFS_GAUGE_LIGHT=0"));   /* 0 is a choice (the exe's white), not "none" */
    car_race_command_free(&c);
    s = setup_of("Wagon", "CL1", 1, RACE_TICKS_TRAFFIC);
    s.cockpit = 1, s.gauge_set = 1, s.gauge_light = 5;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(has_env(&c, "TNFS_VIEW=3") && !has_env_key(&c, "TNFS_GAUGE_LIGHT"));   /* no cockpit for a traffic car, and no dash masks to light */
    car_race_command_free(&c);
    /* the same in a window: no screenshot, no headless drivers (the person's SDL ones stay), no view kind; the race in a window is Control Central's, whose --car
     * is the menu's number (00426620 turns it into the model): the RX-7 is 0 there, model 7 */
    s = setup_of("MRX7", "TR2", 0, 0);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(arg_pair(&c, "--car", "0") && !has_arg(&c, "--node") && !has_arg(&c, "--screenshot"));
    CHECK(arg_pair(&c, "--type", "0") && !has_arg(&c, "--racers"));   /* T4: a time trial, the car alone (the window's race takes no --racers) */
    CHECK(has_env(&c, "SDL_VIDEO_DRIVER=cocoa") && !has_env_key(&c, "TNFS_VIEW") && has_env(&c, "TNFS_CAR_MASKS=/p/proj/car_masks"));
    car_race_command_free(&c);
    /* the 512TR (the owner's test, T1): menu number 6 in a window, model 4 for a photo */
    s = setup_of("F512TR", "TR5", 0, 0);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && arg_pair(&c, "--car", "6"));
    car_race_command_free(&c);
    s = setup_of("F512TR", "TR5", 0, RACE_TICKS_DRIVEN);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && arg_pair(&c, "--car", "4"));
    car_race_command_free(&c);
    /* every drivable car: the menu's number turns back into its model as the game's table does */
    static const int HEAD_TO_HEAD[8] = { 7, 6, 0, 2, 3, 5, 4, 1 };   /* 00426620 */
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        const ExportCar *car = car_export_car(i);
        if (car->kind != EXPORT_DRIVABLE) continue;
        const int pick = car_race_menu_pick(car->model);
        CHECK(pick >= 0 && pick < 8 && HEAD_TO_HEAD[pick] == car->model);
    }
    CHECK_EQ_INT(car_race_menu_pick(-1), -1);
    CHECK_EQ_INT(car_race_menu_pick(8), -1);
    /* a traffic car: a race with traffic (type 1), the first car for the player, the slot followed and the seed of the draw, no dash masks */
    s = setup_of("Wagon", "CL1", 1, RACE_TICKS_TRAFFIC);
    s.slot = 4, s.seed = RACE_SEED_FIRST + 3;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(arg_pair(&c, "--type", "1") && arg_pair(&c, "--car", "0") && !has_arg(&c, "--racers") && arg_pair(&c, "--node", "800"));
    CHECK(has_env(&c, "TNFS_FOLLOW_CAR=4") && has_env(&c, "TNFS_DRIVE_SEED=11115") && !has_env_key(&c, "TNFS_DASH_MASKS"));
    CHECK(has_env(&c, "TNFS_FOLLOW_SETTLE=64"));   /* T3: the frame waits until the car has been out on the road a while (it is parked off it before) */
    car_race_command_free(&c);
    s = setup_of("Wagon", "CL1", 0, 1);   /* a probe follows nothing and waits for nothing */
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && !has_env_key(&c, "TNFS_FOLLOW_SETTLE") && !has_env_key(&c, "TNFS_FOLLOW_CAR"));
    car_race_command_free(&c);
    /* in a window the player drives the same car as in a photo: its menu number (T2: the window says which car that is) */
    s = setup_of("JETTA", "CL1", 0, 0);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && arg_pair(&c, "--type", "1"));
    {
        char pick[8];
        snprintf(pick, sizeof pick, "%d", car_race_menu_pick(RACE_TRAFFIC_PLAYER_MODEL));
        CHECK(arg_pair(&c, "--car", pick));
    }
    car_race_command_free(&c);
    CHECK(car_race_player_label(car_export_car(index_of("JETTA"))) != NULL && strcmp(car_race_player_label(car_export_car(index_of("JETTA"))), "Toyota Supra Turbo") == 0);
    CHECK(strcmp(car_race_player_label(car_export_car(index_of("F512TR"))), "Ferrari 512 TR") == 0);
    /* what a race in a window is, said plainly (T2): who drives what, and where the tested car is */
    char note[400];
    car_race_note(car_export_car(index_of("JETTA")), "CL1", note, sizeof note);
    CHECK(strstr(note, "You drive the Toyota Supra Turbo on Coastal 1") != NULL && strstr(note, "Jetta") != NULL && strstr(note, "traffic") != NULL);
    CHECK(strstr(note, "at random") != NULL && strstr(note, "In-game photo") != NULL);   /* the window's draw is the game's own: not every race has it */
    car_race_note(car_export_car(index_of("COPMUST")), "AL2", note, sizeof note);
    CHECK(strstr(note, "You drive the Toyota Supra Turbo on Alpine 2") != NULL && strstr(note, "Police Mustang") != NULL);
    car_race_note(car_export_car(index_of("F512TR")), "TR5", note, sizeof note);
    CHECK(strstr(note, "You drive the Ferrari 512 TR on Rusty Springs") != NULL && strstr(note, "traffic") == NULL);
    car_race_note(NULL, "TR5", note, sizeof note);
    CHECK(note[0] == '\0');
    /* the police car the same */
    s = setup_of("COPMUST", "AL2", 0, RACE_TICKS_POLICE);
    s.slot = 2;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && arg_pair(&c, "--type", "1") && arg_pair(&c, "--node", "250"));
    CHECK(has_env(&c, "TNFS_POLICE_NEAR=15") && has_env(&c, "TNFS_FOLLOW_CAR=2"));   /* the police put behind the player, chasing: the camera on it has it at any tick */
    CHECK(!has_env_key(&c, "TNFS_FOLLOW_SETTLE"));   /* the police car is on the road from the start */
    car_race_command_free(&c);
    s = setup_of("Wagon", "CL1", 0, 0);   /* the police are put near only for a photo of the police car; a window waits for nothing */
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && !has_env_key(&c, "TNFS_POLICE_NEAR") && !has_env_key(&c, "TNFS_FOLLOW_SETTLE"));
    car_race_command_free(&c);
    /* a circuit has no traffic: refused, with the way out; a track that is not one; a setup that is not whole */
    s = setup_of("Wagon", "TR5", 0, 0);
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why) && strstr(why, "no traffic") != NULL && strstr(why, "Rusty Springs") != NULL);
    s = setup_of("P911", "NOPE", 0, 0);
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why) && strstr(why, "not a track") != NULL);
    s = setup_of("P911", "TR5", 0, RACE_TICKS_DRIVEN);
    s.shot = NULL;
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why));
    s.game = NULL;
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why));
    /* a long environment (a shell's has a hundred words and more) goes through */
    static char words[300][32];
    char *big[301];
    for (int i = 0; i < 300; ++i) snprintf(words[i], sizeof words[i], "VAR%d=value", i), big[i] = words[i];
    big[300] = NULL;
    s = setup_of("P911", "TR5", 0, 0);
    CHECK(car_race_command(&s, big, &c, why, sizeof why) && c.envc >= 300);
    car_race_command_free(&c);
}

/* an installed track (item 6): the game gets --mods of its folder and --drive its id; the window's race only, a car you drive only */
static void test_installed_track_command(void)
{
    char *base_env[] = { "HOME=/h", "PATH=/bin", NULL };
    RaceCommand c;
    char why[300];
    RaceSetup s = setup_of("P911", "my-track", 0, 0);
    s.track_mods = "/u/mods/my-track";
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(arg_pair(&c, "--mods", "/u/mods/my-track") && arg_pair(&c, "--drive", "my-track") && arg_pair(&c, "--type", "0") && arg_pair(&c, "--user", "/tmp/u"));
    CHECK(has_env(&c, "TNFS_CAR_MASKS=/p/proj/car_masks") && has_env(&c, "TNFS_DASH_MASKS=/p/proj/dash_masks"));
    car_race_command_free(&c);
    s = setup_of("Wagon", "my-track", 0, 0);   /* a traffic car: the installed track has no traffic here */
    s.track_mods = "/u/mods/my-track";
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why) && strstr(why, "no traffic") != NULL);
    s = setup_of("P911", "../x", 0, 0);   /* an id that is not one is never given to the game */
    s.track_mods = "/u/mods/x";
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why) && strstr(why, "not an installed track's id") != NULL);
    s = setup_of("P911", "TR5", 0, 0);   /* no folder: a track of the table, as ever, with no --mods */
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && !has_arg(&c, "--mods"));
    car_race_command_free(&c);
}

static void write_text_file(const char *path, const char *text, mode_t mode);

static void make_card(const char *mods, const char *folder, const char *text)
{
    char dir[600], path[700];
    snprintf(dir, sizeof dir, "%s/%s", mods, folder);
    mkdir(mods, 0755), mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    write_text_file(path, text, 0644);
}

/* the installed tracks of a player folder: tracks only, by name, a card that names another id than its folder left out */
static void test_installed_tracks_scan(void)
{
    char user[600], mods[700];
    snprintf(user, sizeof user, "%s/scan_user", base);
    mkdir(user, 0755);
    snprintf(mods, sizeof mods, "%s/mods", user);
    RaceModTrack t[RACE_MOD_TRACKS];
    CHECK(race_mods_scan(user, t, RACE_MOD_TRACKS) == 0);   /* no mods folder */
    make_card(mods, "zeta-ring", "id=zeta-ring\nname=Zeta Ring\nversion=1.0\ntype=track\nstem=ZR\n");
    make_card(mods, "alpha-pass", "id=alpha-pass\nname=Alpha Pass\nversion=1.0\ntype=track\nstem=AP\n");
    make_card(mods, "some-car", "id=some-car\nname=Some Car\nversion=1.0\ntype=car\nstem=SC\n");
    make_card(mods, "liar", "id=other-id\nname=Liar\nversion=1.0\ntype=track\nstem=LI\n");
    make_card(mods, "broken", "this is not a card\n");
    const int n = race_mods_scan(user, t, RACE_MOD_TRACKS);
    CHECK(n == 2);
    CHECK(n == 2 && strcmp(t[0].id, "alpha-pass") == 0 && strcmp(t[0].label, "Alpha Pass") == 0 && strcmp(t[1].id, "zeta-ring") == 0);
    CHECK(n == 2 && strstr(t[0].dir, "/mods/alpha-pass") != NULL);
    CHECK(race_mods_scan(user, t, 1) == 1);   /* the cap */
}

static void test_tracks(void)
{
    CHECK(car_race_track_count() == 16);
    CHECK(car_race_track(-1) == NULL && car_race_track(16) == NULL);
    int stages = 0;
    for (int i = 0; i < car_race_track_count(); ++i) stages += car_race_track(i)->stage;
    CHECK_EQ_INT(stages, 9);
    CHECK(strcmp(car_race_track_label("tr5"), "Rusty Springs") == 0 && strcmp(car_race_track_label("XX9"), "XX9") == 0 && strcmp(car_race_track_label(NULL), "") == 0);
    CHECK(car_race_track_find("tr5") == car_race_track_find("TR5") && car_race_track_find("TR5") >= 0 && car_race_track_find("XX9") < 0 && car_race_track_find(NULL) < 0);
    CHECK(strcmp(car_race_track(car_race_default_track(car_export_car(index_of("P911"))))->stem, "TR5") == 0);   /* Rusty Springs: the night reference */
    CHECK(strcmp(car_race_track(car_race_default_track(car_export_car(index_of("Wagon"))))->stem, "CL1") == 0);   /* a stage: the circuits have no traffic */
    CHECK(car_race_track(car_race_default_track(car_export_car(index_of("COPMUST"))))->stage);
    CHECK_EQ_INT(car_race_photo_ticks(car_export_car(index_of("P911"))), RACE_TICKS_DRIVEN);
    CHECK_EQ_INT(car_race_photo_ticks(car_export_car(index_of("Wagon"))), RACE_TICKS_TRAFFIC);
    CHECK_EQ_INT(car_race_photo_ticks(car_export_car(index_of("COPMUST"))), RACE_TICKS_POLICE);
}

static void read_all(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    out[0] = '\0';
    if (f != NULL) {
        const size_t n = fread(out, 1, cap - 1, f);
        out[n] = '\0';
        fclose(f);
    }
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL && fputs(text, f) >= 0);
    if (f != NULL) fclose(f);
}

/* the interactive runs show the game as the player set it up; the photos keep the fixed values; the player's files are not touched */
static void test_player_dir(void)
{
    char dir[64];
    CHECK(car_race_player_dir("/u/mine", "/lib/Spirited Drive/", dir, sizeof dir) && strcmp(dir, "/u/mine") == 0);   /* --user wins */
    CHECK(car_race_player_dir(NULL, "/lib/Spirited Drive/", dir, sizeof dir) && strcmp(dir, "/lib/Spirited Drive") == 0);   /* a normal start: the game's own folder, the slash dropped */
    CHECK(car_race_player_dir("", "/lib/Spirited Drive", dir, sizeof dir) && strcmp(dir, "/lib/Spirited Drive") == 0);
    CHECK(!car_race_player_dir(NULL, NULL, dir, sizeof dir) && dir[0] == '\0');
    CHECK(!car_race_player_dir(NULL, "/a/very/long/folder/name/that/does/not/fit/in/sixty/four/bytes/at/all/", dir, sizeof dir) && dir[0] == '\0');
}

static void test_player_options(void)
{
    char temp[1024], user[1100], mine[1100], why[300], text[600], path[1200], before[600];
    CHECK(car_race_temp(temp, sizeof temp, user, sizeof user, why, sizeof why));
    snprintf(mine, sizeof mine, "%s/player", temp);
    CHECK(mkdir(mine, 0777) == 0);
    snprintf(path, sizeof path, "%s/port.cfg", mine);
    write_text(path, "profile=stock\nfullscreen=1\npsx_dir=/somewhere\nformat=16_9\nrefresh_hz=144\n");
    snprintf(path, sizeof path, "%s/restomod.cfg", mine);
    write_text(path, "fullscreen=1\nfov_adjust=10\nscale=fill\nrender_scale=3\nlight_effects=1\nlight_time=night\nweather=rain\nshine_soft=0\nsome_other=7\n");
    read_all(path, before, sizeof before);
    RaceLook look = car_race_look_default();
    CHECK(car_race_user_files(user, 0, &look, why, sizeof why));   /* a photo: the fixed values only, car_race_player_options is not called */
    snprintf(path, sizeof path, "%s/restomod.cfg", user);
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "fov_adjust=10") == NULL && strstr(text, "render_scale=2\n") != NULL);
    car_race_set_player(mine);
    CHECK(car_race_player_options(user, why, sizeof why));   /* an interactive run */
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "fov_adjust=10\n") != NULL && strstr(text, "scale=fill\n") != NULL && strstr(text, "render_scale=3\n") != NULL && strstr(text, "render_scale=2\n") == NULL);   /* the player's value in place of the fixed one */
    CHECK(strstr(text, "light_time=day\n") != NULL && strstr(text, "weather=rain") == NULL && strstr(text, "some_other") == NULL && strstr(text, "fullscreen=1") == NULL);   /* the run's own keys stay */
    CHECK(strstr(text, "shine_soft=0\n") != NULL);   /* the player's soft edges choice goes to the test drive too */
    CHECK_EQ_INT(car_race_player_shine_soft(), 0);   /* and the Preview reads it from the same files */
    CHECK_EQ_INT(car_race_player_render_scale(), 3);   /* the Model window starts at the player's render scale */
    snprintf(path, sizeof path, "%s/port.cfg", user);
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "format=16_9\n") != NULL && strstr(text, "refresh_hz=144\n") != NULL && strstr(text, "profile=restomod\n") != NULL && strstr(text, "fullscreen=0\n") != NULL);
    CHECK(strstr(text, "profile=stock") == NULL && strstr(text, "psx_dir") == NULL && strstr(text, "fullscreen=1") == NULL);
    snprintf(path, sizeof path, "%s/restomod.cfg", mine);
    char after[600];
    read_all(path, after, sizeof after);
    CHECK(strcmp(before, after) == 0);   /* the player's file is as it was */
    car_race_set_player("");
    CHECK_EQ_INT(car_race_player_shine_soft(), 1);   /* no player files: Restomod's default, on */
    CHECK_EQ_INT(car_race_player_render_scale(), 2);   /* and Restomod's render scale */
    CHECK(car_race_player_options(user, why, sizeof why));   /* no player folder: nothing to do */
    car_race_set_player(NULL);
    CHECK(car_race_temp_remove(temp));
}

static void test_files_and_temp(void)
{
    char temp[1024], user[1100], why[300], text[400], path[1200];
    CHECK(car_race_temp(temp, sizeof temp, user, sizeof user, why, sizeof why));
    CHECK(strstr(temp, "studio-race-") != NULL && strcmp(user + strlen(temp), "/user") == 0);
    struct stat st;
    CHECK(stat(user, &st) == 0 && S_ISDIR(st.st_mode));
    /* T5: Restomod whole, whatever the player's own settings: the mode, and the look's keys written out (the table could change under them) */
    RaceLook look = car_race_look_default();
    CHECK(look.reflections && look.car_colours && look.soft_shadow && look.texture_filter && look.scale == 2);   /* T6: all on, full detail */
    CHECK(car_race_user_files(user, 1, &look, why, sizeof why));
    snprintf(path, sizeof path, "%s/port.cfg", user);
    read_all(path, text, sizeof text);
    CHECK(strcmp(text, "profile=restomod\nfullscreen=0\n") == 0);
    snprintf(path, sizeof path, "%s/restomod.cfg", user);
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "light_time=night\n") != NULL && strstr(text, "light_effects=1\n") != NULL && strstr(text, "render_scale=2\n") != NULL);
    CHECK(strstr(text, "body_shine=4\n") != NULL && strstr(text, "car_colours=1\n") != NULL && strstr(text, "car_soft_shadow=1\n") != NULL);
    CHECK(strstr(text, "texture_filter=1\n") != NULL && strstr(text, "extra_liveries=0\n") != NULL);   /* the project's livery, never another */
    CHECK(strstr(text, "fullscreen=0\n") != NULL);   /* the game the Studio starts (a test drive, a race, a photo) is in a window: its profile starts full screen */
    look.reflections = 0, look.texture_filter = 0, look.scale = 1;   /* T6: the switches */
    CHECK(car_race_user_files(user, 0, &look, why, sizeof why));
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "light_time=day\n") != NULL && strstr(text, "body_shine=0\n") != NULL && strstr(text, "texture_filter=0\n") != NULL);
    CHECK(strstr(text, "render_scale=1\n") != NULL && strstr(text, "car_colours=1\n") != NULL);
    char off[200];
    car_race_look_off(&look, off, sizeof off);   /* what the summary says was off */
    CHECK(strstr(off, "Reflections") != NULL && strstr(off, "Texture filter") != NULL && strstr(off, "640x480") != NULL && strstr(off, "Car colours") == NULL);
    look = car_race_look_default();
    car_race_look_off(&look, off, sizeof off);
    CHECK(off[0] == '\0');
    look.scale = 1;   /* the scale alone */
    car_race_look_off(&look, off, sizeof off);
    CHECK(strcmp(off, "640x480") == 0);
    look = (RaceLook){ 0, 0, 0, 0, 2, 0, 0, 0, 0, 0 };   /* every switch off: each key at 0, none read into another */
    CHECK(car_race_user_files(user, 0, &look, why, sizeof why));
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "body_shine=0\n") != NULL && strstr(text, "car_colours=0\n") != NULL && strstr(text, "car_soft_shadow=0\n") != NULL);
    CHECK(strstr(text, "texture_filter=0\n") != NULL && strstr(text, "render_scale=2\n") != NULL && strstr(text, "light_effects=1\n") != NULL);
    car_race_look_off(&look, off, sizeof off);
    CHECK(strcmp(off, "Reflections, Car colours, Soft shadow, Texture filter off") == 0);
    char small[12];   /* a short buffer: cut, still a string */
    car_race_look_off(&look, small, sizeof small);
    CHECK(strlen(small) < sizeof small);
    /* W5: Preview's colour: the catalogue paint of the car's model, the game's own digit (2 + k) at that model's place; none: the lines stay as they were */
    look = car_race_look_default();
    CHECK(look.paint == 0 && look.paint_model == 0);
    const ExportCar *drivable = car_export_car(0);   /* the colour a card has is the digit the game knows it by; any other colour, a traffic car or none: own */
    CHECK(drivable != NULL && drivable->model >= 0 && car_paint_count(drivable->model) > 1);
    CHECK(car_race_paint_digit(drivable, car_paint(drivable->model, 0)->rgb) == 2);
    const uint8_t odd[3] = { 1, 2, 3 };
    CHECK(car_race_paint_digit(drivable, odd) == 0 && car_race_paint_digit(drivable, NULL) == 0 && car_race_paint_digit(NULL, odd) == 0);
    CHECK(car_race_paint_digit(car_export_car(EXPORT_CARS - 1), car_paint(drivable->model, 0)->rgb) == 0);   /* the police car: no table */
    look.paint_model = 5, look.paint = 2 + 3;
    CHECK(car_race_user_files(user, 0, &look, why, sizeof why));
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "extra_liveries=1\n") != NULL && strstr(text, "car_paint=000005\n") != NULL);   /* digit 5 at index 5 (the file's digits stop there) */
    look.paint_model = 11, look.paint = 2 + 12;   /* the last car, a hex digit */
    CHECK(car_race_user_files(user, 0, &look, why, sizeof why));
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "car_paint=00000000000e\n") != NULL && strstr(text, "extra_liveries=1\n") != NULL && strstr(text, "extra_liveries=0\n") == NULL);
    look.paint_model = 12, look.paint = 3;   /* out of the table: refused, nothing is half written */
    CHECK(!car_race_user_files(user, 0, &look, why, sizeof why) && strstr(why, "paint") != NULL);
    look.paint_model = 2, look.paint = 0;   /* own colour: as before */
    CHECK(car_race_user_files(user, 0, &look, why, sizeof why));
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "extra_liveries=0\n") != NULL && strstr(text, "car_paint") == NULL);
    CHECK(car_race_user_files(user, 0, NULL, why, sizeof why));   /* none: the default */
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "render_scale=2\n") != NULL && strstr(text, "body_shine=4\n") != NULL);
    CHECK(strncmp(user, getenv("TMPDIR"), strlen(getenv("TMPDIR"))) == 0);   /* in the temp folder, never the player's own */
    CHECK(!car_race_user_files("/nonexistent-folder/user", 0, NULL, why, sizeof why) && strstr(why, "cannot write") != NULL);
    CHECK(car_race_temp_remove(temp) && stat(temp, &st) != 0);
    CHECK(car_race_temp_remove(temp));   /* already gone: fine */
    /* only what car_race_temp made is ever removed */
    char other[1100];
    snprintf(other, sizeof other, "%s/keep-me", base);
    mkdir(other, 0777);
    CHECK(!car_race_temp_remove(other) && stat(other, &st) == 0);
    CHECK(!car_race_temp_remove(NULL) && !car_race_temp_remove("/"));
    rmdir(other);
}

static void test_log(void)
{
    const char *log = "line\nrace car: slot 0 is TSupra\nrace car: slot 1 is LDiabl\nrace car: slot 3 is Wagon\nrace car: slot 4 is axxess\n";
    CHECK_EQ_INT(car_race_find_slot(log, "WAGON"), 3);
    CHECK_EQ_INT(car_race_find_slot(log, "axxess"), 4);
    CHECK_EQ_INT(car_race_find_slot(log, "AXXESS"), 4);
    CHECK_EQ_INT(car_race_find_slot(log, "probe"), -1);
    CHECK_EQ_INT(car_race_find_slot("", "Wagon"), -1);
    CHECK_EQ_INT(car_race_find_slot("race car: slot x is Wagon\n", "Wagon"), -1);
    CHECK_EQ_INT(car_race_find_slot("race car: slot 2 is CopMust", "COPMUST"), 2);
    CHECK_EQ_INT(car_race_line_kind("tick 60 (phase 1): node 20", 26), RACE_LINE_PLAIN);
    CHECK_EQ_INT(car_race_line_kind("car mask: /p/car_masks/Wagon/side_mask.png", 42), RACE_LINE_MASK);
    CHECK_EQ_INT(car_race_line_kind("platform: cannot read /p/TSupra/frnt_mask.png: Couldn't open /p", 64), RACE_LINE_NOTE);
    CHECK_EQ_INT(car_race_line_kind("platform: cannot read /p/x.cfm: Couldn't open /p", 47), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("FATAL: no game data", 19), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("car mask: cannot read it", 24), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("track not found", 15), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("", 0), RACE_LINE_PLAIN);
    const char *noisy = "a\nplatform: cannot read /p/TSupra/a_mask.png: Couldn't open /p\nb\nplatform: cannot read /p/TSupra/b_mask.png: Couldn't open /p\nplatform: cannot read /p/x.cfm: Couldn't open /p\n";
    CHECK_EQ_INT(car_race_count_notes(noisy, strlen(noisy)), 2);   /* the two masks; the car file is an error */
    CHECK_EQ_INT(car_race_count_notes("", 0), 0);
    { const char *last = "platform: cannot read /p/q_mask.png: Couldn't open /p"; CHECK_EQ_INT(car_race_count_notes(last, strlen(last)), 1); }   /* the last line without a newline */
}

static void write_text_file(const char *path, const char *text, mode_t mode)
{
    FILE *f = fopen(path, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
    chmod(path, mode);
}

static void test_ppm(void)
{
    char path[1100], why[200];
    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    snprintf(path, sizeof path, "%s/f.ppm", base);
    FILE *f = fopen(path, "wb");
    fputs("P6\n640 480\n255\n", f);
    for (int i = 0; i < 640 * 480; ++i) fputc(10, f), fputc(20, f), fputc(30, f);
    fclose(f);
    CHECK(car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && w == 640 && h == 480);
    CHECK(rgba != NULL && rgba[0] == 10 && rgba[1] == 20 && rgba[2] == 30 && rgba[3] == 255 && rgba[4 * (640 * 480 - 1) + 2] == 30);
    free(rgba);
    f = fopen(path, "wb");   /* render scale 2: twice each way */
    fputs("P6\n1280 960\n255\n", f);
    for (int i = 0; i < 1280 * 960; ++i) fputc(1, f), fputc(2, f), fputc(3, f);
    fclose(f);
    CHECK(car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && w == 1280 && h == 960 && rgba != NULL && rgba[4 * (1280 * 960 - 1)] == 1);
    free(rgba);
    write_text_file(path, "P6\n320 200\n255\nxx", 0644);
    CHECK(!car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && strstr(why, "640x480") != NULL && strstr(why, "1280x960") != NULL);
    write_text_file(path, "P6\n640 480\n255\nshort", 0644);
    CHECK(!car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && strstr(why, "cut short") != NULL);
    CHECK(!car_race_read_ppm("/nonexistent.ppm", &rgba, &w, &h, why, sizeof why));
    remove(path);
}

/* a fake game: it writes the lines the real one does into --user's log; FAKE_MODE says which traffic it draws */
static void make_fake_game(char *path, size_t cap)
{
    snprintf(path, cap, "%s/fake-tnfs", base);
    write_text_file(path,
        "#!/bin/sh\n"
        "user=; shot=\n"
        "zip=\n"
        "while [ $# -gt 0 ]; do case \"$1\" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; --install) zip=$2; shift;; esac; shift; done\n"
        "if [ -n \"$zip\" ]; then echo \"$zip\" > \"$user/../installed_from\"; sleep 1; case \"$zip\" in *broken*) exit 4;; esac; mkdir -p \"$user/mods/loop\"; echo 'installed loop 0.1'; exit 0; fi\n"
        "echo \"fake game out follow=$TNFS_FOLLOW_CAR seed=$TNFS_DRIVE_SEED masks=$TNFS_CAR_MASKS saw=$([ -n \"$TNFS_CAR_MASKS\" ] && ls \"$TNFS_CAR_MASKS\"/*/ 2>/dev/null | tr '\\n' ' ') dashsaw=$([ -n \"$TNFS_DASH_MASKS\" ] && ls \"$TNFS_DASH_MASKS\"/*/ 2>/dev/null | tr '\\n' ' ')\"\n"
        "case \"$FAKE_MODE\" in\n"
        "  drive) printf 'mods: 1\\ntrack piece: loop is track 18\\n' > \"$user/spirited-drive.log\"; echo \"$@\"; sleep 30; exit 0;;\n"
        "  half) printf 'track piece: loop is track 1' > \"$user/spirited-drive.log\"; sleep 1; printf '8\\n' >> \"$user/spirited-drive.log\"; sleep 30; exit 0;;\n"
        "  refused) printf 'track piece: loop refused: there are 8 added tracks already, the most there may be\\n' > \"$user/spirited-drive.log\"; exit 0;;\n"
        "  fail) echo \"error: no such track\" >&2; exit 3;;\n"
        "  hang) sleep 30; exit 0;;\n"
        "esac\n"
        "case \"$FAKE_MODE\" in\n"
        "  old) ;;\n"
        "  police) printf 'race car: slot 0 is TSupra\\nrace car: slot 2 is COPMUST\\n' > \"$user/spirited-drive.log\";;\n"
        "  never) printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is BMW\\n' > \"$user/spirited-drive.log\";;\n"
        "  later) if [ \"$TNFS_DRIVE_SEED\" = 11113 ]; then printf 'race car: slot 0 is TSupra\\nrace car: slot 5 is Wagon\\n' > \"$user/spirited-drive.log\";\n"
        "         else printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is BMW\\n' > \"$user/spirited-drive.log\"; fi;;\n"
        "  *) printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is Wagon\\n' > \"$user/spirited-drive.log\";;\n"
        "esac\n"
        "if [ -n \"$TNFS_FOLLOW_CAR\" ]; then case \"$FAKE_FOLLOW\" in\n"   /* the game's line on the followed car (a game from before it says none) */
        "  road) echo \"follow: slot $TNFS_FOLLOW_CAR on the road from tick 900, the frame at tick 964\";;\n"
        "  parked) echo \"follow: slot $TNFS_FOLLOW_CAR is parked off the road at the frame (tick 4800): it did not come out on the road in the wait\";;\n"
        "  first) if [ -z \"$TNFS_DRIVE_SEED\" ]; then echo \"follow: slot $TNFS_FOLLOW_CAR is parked off the road at the frame (tick 4800)\";\n"
        "         else echo \"follow: slot $TNFS_FOLLOW_CAR on the road from tick 1000, the frame at tick 1064\"; fi;;\n"
        "esac; fi\n"
        "if [ -n \"$shot\" ]; then printf 'P6\\n640 480\\n255\\n' > \"$shot\"; head -c 921600 /dev/zero >> \"$shot\"; fi\n"
        "exit 0\n", 0755);
}

static void run_job(RaceJob *job)
{
    for (int i = 0; i < 12000 && car_race_poll(job); ++i) usleep(5000);   /* a minute at most: every draw tried twice is 50 runs of the fake */
}

static int temps_left(void)
{
    char cmd[700];
    snprintf(cmd, sizeof cmd, "ls %s | grep -q studio-race-", base);
    return system(cmd) == 0;
}

/* a kit keeps its masks flat in art-generated/ (no stem level): the run is given a scratch folder with the stem level the game reads, linked to them, and the kit's files
 * are left as they were when it goes */
static void test_kit_masks(void)
{
    char game[600], why[300], kit[600], path[700];
    make_fake_game(game, sizeof game);
    snprintf(kit, sizeof kit, "%s/kitproj", base);
    snprintf(path, sizeof path, "%s/art-generated", kit);
    mkdir(kit, 0777), mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/art-generated/car_masks", kit);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/art-generated/car_masks/side_mask.png", kit);
    write_text_file(path, "not a png", 0644);
    snprintf(path, sizeof path, "%s/art-generated/dash_masks", kit);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/art-generated/dash_masks/dash_mask.png", kit);
    write_text_file(path, "not a png", 0644);
    RaceJob job;
    memset(&job, 0, sizeof job);
    job.run.fd = -1;
    const ExportCar *p911 = car_export_car(index_of("P911")), *wagon = car_export_car(index_of("Wagon"));
    setenv("FAKE_MODE", "ok", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", kit, p911, "TR5", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && strstr(job.log, "saw=side_mask.png") != NULL && strstr(job.log, "dashsaw=dash_mask.png") != NULL);
    CHECK(strstr(job.log, "masks=") != NULL && strstr(job.log, "/kitproj/") == NULL && strstr(job.log, "studio-race-") != NULL);   /* the scratch folder's, not the kit's */
    snprintf(path, sizeof path, "%s/art-generated/car_masks/side_mask.png", kit);
    struct stat st;
    CHECK(job.temp[0] == '\0' && !temps_left() && stat(path, &st) == 0 && st.st_size == 9);   /* the scratch folder went, the kit's file did not */
    CHECK(car_race_start_photo(&job, game, "/cd", kit, wagon, "CL1", 0, NULL, why, sizeof why));   /* a traffic car: no dash masks */
    run_job(&job);
    CHECK(job.ok && strstr(job.log, "saw=side_mask.png") != NULL && strstr(job.log, "dashsaw=\n") != NULL);   /* a traffic car has no dash masks: TNFS_DASH_MASKS is not set (an unset one in the fake's ls was the whole root, which filled the log) */
    CHECK(stat(path, &st) == 0 && !temps_left());
    /* Test drive (a race, not a photo) of the same kit: the game's own car with the same staged masks, the scratch folder gone at the end, the kit's files as they were */
    CHECK(car_race_start_race(&job, game, "/cd", kit, p911, "TR2", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && strstr(job.log, "saw=side_mask.png") != NULL && strstr(job.log, "dashsaw=dash_mask.png") != NULL);
    CHECK(strstr(job.log, "/kitproj/") == NULL && strstr(job.log, "studio-race-") != NULL);
    CHECK(job.temp[0] == '\0' && !temps_left() && stat(path, &st) == 0 && st.st_size == 9);
    car_race_free(&job);
}

static void test_job(void)
{
    char game[600], why[300], project[600];
    make_fake_game(game, sizeof game);
    snprintf(project, sizeof project, "%s/proj", base);
    mkdir(project, 0777);
    RaceJob job;
    memset(&job, 0, sizeof job);
    job.run.fd = -1;
    const ExportCar *p911 = car_export_car(index_of("P911")), *wagon = car_export_car(index_of("Wagon")), *cop = car_export_car(index_of("COPMUST"));
    setenv("FAKE_MODE", "ok", 1);
    /* an installed track (item 6): the window's race on its folder; a traffic car refused before anything is made; no folder refused */
    CHECK(!car_race_start_race_mod(&job, game, "/cd", project, wagon, "my-track", "/u/mods/my-track", 0, NULL, why, sizeof why) && strstr(why, "no traffic") != NULL);
    CHECK(!car_race_start_race_mod(&job, game, "/cd", project, p911, "my-track", "", 0, NULL, why, sizeof why) && strstr(why, "no folder") != NULL);
    CHECK(car_race_start_race_mod(&job, game, "/cd", project, p911, "my-track", "/u/mods/my-track", 1, NULL, why, sizeof why));
    run_job(&job);
    CHECK(!car_race_busy(&job) && job.finished && strstr(job.log, "Test drive on the installed track my-track, night") != NULL && job.temp[0] == '\0' && !temps_left());
    /* a car you drive: one run, a photo, the scratch folder gone after */
    CHECK(car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 1, NULL, why, sizeof why));
    CHECK(car_race_busy(&job) && !car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 1, NULL, why, sizeof why) && strstr(why, "already") != NULL);
    run_job(&job);
    CHECK(!car_race_busy(&job) && job.finished && job.ok && job.photo != NULL && job.photo_serial == 1 && job.photo_night == 1);
    CHECK(strstr(job.summary, "Porsche 911 Carrera on Rusty Springs, night") != NULL && strstr(job.summary, " off") == NULL && strstr(job.summary, "TR5") == NULL);
    CHECK(strstr(job.log, "In-game photo: Porsche 911 Carrera on Rusty Springs, night") != NULL);
    CHECK(job.photo_w == 640 && job.photo_h == 480);   /* the size the game wrote (the fake's) */
    {   /* T6: what was switched off is said in the summary and the log */
        RaceLook look = car_race_look_default();
        look.reflections = 0;
        CHECK(car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 0, &look, why, sizeof why));
        run_job(&job);
        CHECK(job.ok && strstr(job.summary, "Reflections off") != NULL && strstr(job.log, "Reflections off") != NULL);
    }
    CHECK(strstr(job.log, "fake game out") != NULL && strstr(job.log, "masks=") != NULL && strstr(job.log, "/proj/car_masks") != NULL);
    CHECK(job.temp[0] == '\0' && !temps_left());
    /* a traffic car found at once: the probe, then the shot following its slot */
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.photo_serial == 3 && job.photo_slot == 3 && strstr(job.summary, "slot 3") != NULL && strstr(job.summary, "800 ticks") != NULL && strstr(job.summary, "Coastal 1") != NULL && strstr(job.summary, "CL1") == NULL);
    CHECK(strstr(job.log, "follow=3") != NULL && strstr(job.log, "the car is in it") != NULL && !temps_left());
    /* found in the third draw: the seeds are tried in order */
    setenv("FAKE_MODE", "later", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 1, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.photo_slot == 5 && job.photo_seed == 11113 && strstr(job.log, "seed=11112") != NULL && strstr(job.log, "follow=5") != NULL);
    CHECK(strstr(job.log, "the car is not in it") != NULL && job.probes == 2);
    /* in none of the draws: no photo, said plainly, nothing left */
    setenv("FAKE_MODE", "never", 1);
    const int serial = job.photo_serial;
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.finished && !job.ok && strstr(job.summary, "none of") != NULL && strstr(job.summary, "no tick where it is on screen") != NULL);
    CHECK(job.probes == RACE_SEEDS + 1 && job.photo == NULL && job.photo_serial == serial && !temps_left());
    /* T3: the game says the car was on the road at the frame: the summary says so, and the tick */
    setenv("FAKE_MODE", "ok", 1);
    setenv("FAKE_FOLLOW", "road", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.photo != NULL && strstr(job.summary, "the Wagon on the road") != NULL && strstr(job.summary, "964 ticks") != NULL);
    CHECK(strstr(job.summary, "cannot see") == NULL);
    /* parked off the road in the game's own draw: the next draw is tried, and its photo has the car on the road */
    setenv("FAKE_FOLLOW", "first", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.photo_seed == RACE_SEED_FIRST && strstr(job.summary, "1064 ticks") != NULL && strstr(job.log, "stayed parked") != NULL);
    /* parked in every draw: no photo, said plainly */
    setenv("FAKE_FOLLOW", "parked", 1);
    const int before = job.photo_serial;
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.finished && !job.ok && strstr(job.summary, "parked off the road") != NULL && job.photo == NULL && job.photo_serial == before && !temps_left());
    CHECK(job.parked == RACE_PARKED_DRAWS && job.probes == RACE_PARKED_DRAWS);   /* a few parked draws are enough: each one is a long run */
    /* the police car is never parked: a line that says so is not a reason to try another draw (the old summary) */
    setenv("FAKE_MODE", "police", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, cop, "AL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.probes == 0 && job.parked == 0 && strstr(job.summary, "cannot see") != NULL);
    /* parked in the draws that have it, absent from the others (the last one tried among them): still said as parked */
    setenv("FAKE_MODE", "later", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.finished && !job.ok && strstr(job.summary, "parked off the road") != NULL && job.probes == RACE_SEEDS + 1 && job.parked == 1);
    unsetenv("FAKE_FOLLOW");
    /* a game that does not say who is in which slot (an older one) */
    setenv("FAKE_MODE", "old", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, cop, "AL1", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(!job.ok && strstr(job.summary, "older") != NULL && !temps_left());
    /* the police car is in slot 3 of the fake: any car is found by its name */
    setenv("FAKE_MODE", "ok", 1);
    /* a game that fails: its exit code and its output are in the log, no photo */
    setenv("FAKE_MODE", "fail", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 0, NULL, why, sizeof why));
    run_job(&job);
    CHECK(!job.ok && strstr(job.summary, "exit code 3") != NULL && strstr(job.log, "no such track") != NULL && job.photo == NULL && !temps_left());
    /* Test drive: the game in its window, run to its end */
    setenv("FAKE_MODE", "ok", 1);
    CHECK(car_race_start_race(&job, game, "/cd", project, p911, "TR2", 1, NULL, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && strstr(job.summary, "ended") != NULL && !temps_left());
    /* Stop ends a game that does not */
    setenv("FAKE_MODE", "hang", 1);
    CHECK(car_race_start_race(&job, game, "/cd", project, p911, "TR2", 0, NULL, why, sizeof why));
    usleep(100000);
    CHECK(car_race_busy(&job));
    car_race_stop(&job);
    run_job(&job);
    CHECK(!car_race_busy(&job) && strstr(job.summary, "Stopped") != NULL && !temps_left());
    /* refusals: no game, a car that is not an original, an unknown track */
    CHECK(!car_race_start_race(&job, "/nonexistent/tnfs", "/cd", project, p911, "TR2", 0, NULL, why, sizeof why) && strstr(why, "Locate game") != NULL);
    CHECK(!car_race_start_race(&job, game, "/cd", project, NULL, "TR2", 0, NULL, why, sizeof why) && strstr(why, "Export original car") != NULL);
    CHECK(!car_race_start_race(&job, game, "/cd", project, p911, "ZZ9", 0, NULL, why, sizeof why));
    CHECK(!car_race_start_race(&job, game, "/cd", project, wagon, "TR5", 0, NULL, why, sizeof why) && strstr(why, "no traffic") != NULL && !temps_left());   /* nothing made before the refusal */
    car_race_free(&job);
    rmdir(project);
    unlink(game);
}

static void test_original_of(void)
{
    CarProject c;
    memset(&c, 0, sizeof c);
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "P911");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("P911")));
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "TSUPRA");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("TSupra")));
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "C84");   /* a car of somebody's own */
    CHECK(car_race_original_of(&c) == NULL);
    c.masks_only = 1;
    snprintf(c.folder, sizeof c.folder, "%s", "axxess");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("axxess")));
    snprintf(c.folder, sizeof c.folder, "%s", "NOPE");
    CHECK(car_race_original_of(&c) == NULL);
    c.masks_only = 0;
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "WAGON");   /* a traffic car's kit (Export original car makes one now): its stem finds its original, of any kind */
    CHECK(car_race_original_of(&c) == car_export_car(index_of("Wagon")));
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "COPMUST");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("COPMUST")));
}

/* Test drive of a track piece (the track editor's Drive window): the menu's cars in its order, and the command, a time trial on the piece the run installed */
static void test_drive_command(void)
{
    const ExportCar *first = car_race_menu_car(0);
    CHECK(first != NULL && first->kind == EXPORT_DRIVABLE && car_race_menu_pick(first->model) == 0);   /* the menu's first */
    int all = 1;
    for (int pick = 0; pick < 8; ++pick) all &= car_race_menu_car(pick) != NULL && car_race_menu_pick(car_race_menu_car(pick)->model) == pick;
    CHECK(all && car_race_menu_car(8) == NULL && car_race_menu_car(-1) == NULL);

    char *env[] = { "HOME=/h", "TNFS_CAR_MASKS=/elsewhere", "PATH=/bin", NULL };
    RaceSetup s = { 0 };
    s.game = "/g/tnfs", s.cd = "/cd", s.project = "/p/loop", s.user = "/tmp/u", s.car = car_race_menu_car(3), s.piece = "loop";
    RaceCommand c;
    memset(&c, 0, sizeof c);
    char why[300];
    CHECK(car_race_command(&s, env, &c, why, sizeof why));
    CHECK(arg_pair(&c, "--mods", "/tmp/u/mods/loop") && arg_pair(&c, "--drive", "loop") && arg_pair(&c, "--car", "3") && arg_pair(&c, "--type", "0"));
    CHECK(arg_pair(&c, "--user", "/tmp/u") && arg_pair(&c, "--data", "/cd") && !has_arg(&c, "--racers") && !has_arg(&c, "--node"));
    CHECK(!has_env_key(&c, "TNFS_") && has_env(&c, "HOME=/h"));   /* no masks of a car's project, and none of the person's TNFS_ */
    s.car = car_export_car(index_of("Wagon"));
    CHECK(!car_race_command(&s, env, &c, why, sizeof why) && strstr(why, "drive") != NULL);   /* a traffic car is not driven */
    s.car = car_race_menu_car(0), s.photo_ticks = 200, s.shot = "/tmp/f.ppm";
    CHECK(!car_race_command(&s, env, &c, why, sizeof why));   /* no photo of a piece */
    s.photo_ticks = 0, s.piece = "../x";
    CHECK(!car_race_command(&s, env, &c, why, sizeof why));   /* an id that is no folder name */
    car_race_command_free(&c);
}

/* the whole Test drive against the fake game: installed into the scratch folder, the game's own log says whether it took the piece, Stop ends it */
static void test_drive_job(void)
{
    char game[600], why[300];
    make_fake_game(game, sizeof game);
    RaceJob job;
    memset(&job, 0, sizeof job);
    job.run.fd = -1;
    setenv("FAKE_MODE", "drive", 1);
    CHECK(car_race_start_drive(&job, game, "/cd", "/p/loop/build/loop.zip", "loop", car_race_menu_car(0), NULL, why, sizeof why));
    CHECK(car_race_busy(&job) && car_race_installing(&job));   /* the install runs in the job: the start does not wait for it (it takes a second) */
    for (int i = 0; i < 4000 && car_race_installing(&job) && car_race_poll(&job); ++i) usleep(5000);
    char from[700] = "";
    char path[700];
    snprintf(path, sizeof path, "%s/installed_from", job.temp);
    FILE *f = fopen(path, "r");
    if (f != NULL) (void)!fgets(from, sizeof from, f), fclose(f);
    CHECK(strncmp(from, "/p/loop/build/loop.zip", 22) == 0 && !car_race_installing(&job) && car_race_busy(&job));   /* the zip given, then the game */
    for (int i = 0; i < 4000 && job.piece_track == 0 && car_race_poll(&job); ++i) usleep(5000);
    CHECK_EQ_INT(job.piece_track, 18);   /* read from the game's log while it runs */
    CHECK(strstr(job.piece_line, "track piece: loop is track 18") != NULL);
    car_race_stop(&job);
    run_job(&job);
    CHECK(!car_race_busy(&job) && job.ok && strstr(job.summary, "loop") != NULL && strstr(job.summary, "track 18") != NULL);
    CHECK(strstr(job.log, "--drive loop") != NULL || strstr(job.log, "Test drive") != NULL);
    CHECK(!temps_left());

    setenv("FAKE_MODE", "half", 1);   /* a line the game is still writing ("track 1" of "track 18") is not taken */
    CHECK(car_race_start_drive(&job, game, "/cd", "/p/loop/build/loop.zip", "loop", car_race_menu_car(0), NULL, why, sizeof why));
    for (int i = 0; i < 4000 && job.piece_track == 0 && car_race_poll(&job); ++i) usleep(5000);
    CHECK_EQ_INT(job.piece_track, 18);
    car_race_stop(&job);
    run_job(&job);
    CHECK(!temps_left());

    setenv("FAKE_MODE", "refused", 1);   /* the game does not take it: said, not ok */
    CHECK(car_race_start_drive(&job, game, "/cd", "/p/loop/build/loop.zip", "loop", car_race_menu_car(0), NULL, why, sizeof why));
    run_job(&job);
    CHECK(!job.ok && job.piece_track < 0 && strstr(job.summary, "refused") != NULL && strstr(job.summary, "already") != NULL);
    CHECK(!temps_left());

    CHECK(car_race_start_drive(&job, game, "/cd", "/p/broken.zip", "loop", car_race_menu_car(0), NULL, why, sizeof why));
    run_job(&job);   /* an install that fails: the game does not run, the job says why, nothing left */
    CHECK(!job.ok && strstr(job.summary, "status 4") != NULL && strstr(job.summary, "install") != NULL && strstr(job.log, "--drive") == NULL && !temps_left());
    CHECK(car_race_start_drive(&job, game, "/cd", "/p/loop.zip", "loop", car_race_menu_car(0), NULL, why, sizeof why));
    car_race_stop(&job);   /* Stop while it installs: no game after it */
    run_job(&job);
    CHECK(!job.ok && strstr(job.summary, "Stopped") != NULL && strstr(job.log, "--drive") == NULL && !temps_left());
    CHECK(!car_race_start_drive(&job, game, "/cd", "/p/loop.zip", "loop", car_export_car(index_of("Wagon")), NULL, why, sizeof why));
    car_race_free(&job);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/car_race_test_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(base) == NULL) return 2;
    char real[512];
    if (realpath(base, real) != NULL) snprintf(base, sizeof base, "%s", real);
    setenv("TMPDIR", base, 1);   /* the scratch folders of the jobs are made here, and counted */
    test_command();
    test_installed_track_command();
    test_installed_tracks_scan();
    test_tracks();
    test_files_and_temp();
    test_player_dir();
    test_player_options();
    test_log();
    test_ppm();
    test_original_of();
    test_job();
    test_kit_masks();
    test_drive_command();
    test_drive_job();
    nftw(base, remove_entry, 16, FTW_DEPTH | FTW_PHYS);   /* what this test made, and only it */
    return TEST_REPORT();
}
