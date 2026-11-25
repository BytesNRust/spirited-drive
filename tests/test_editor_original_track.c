#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
/* Original tracks as workspace projects (editor/original_track.h): adding all sixteen from a fake data/ and art folder, never overwriting, skipping PSD backups; the publish plan and
 * the copy back into data/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "original_track.h"

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; fprintf(stderr, "FAIL line %d: ", __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static char g_root[256];

static void path_of(char *out, size_t cap, const char *rel) { snprintf(out, cap, "%s/%s", g_root, rel); }

static void make(const char *rel)
{
    char path[512];
    path_of(path, sizeof path, rel);
    mkdir(path, 0777);
}

static void put(const char *rel, const char *text)
{
    char path[512];
    path_of(path, sizeof path, rel);
    FILE *f = fopen(path, "w");
    if (f != NULL) fputs(text, f), fclose(f);
}

static int get(const char *rel, char *out, size_t cap)
{
    char path[512];
    path_of(path, sizeof path, rel);
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    const size_t n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    return 1;
}

static int exists(const char *rel)
{
    char path[512];
    path_of(path, sizeof path, rel);
    struct stat st;
    return stat(path, &st) == 0;
}

/* a data/ with the light file of every track, and an art folder with PSDs for AL1 (and a backup and a stray file) */
static void fixtures(void)
{
    make("data"), make("data/track_light"), make("art"), make("art/AL1"), make("ws");
    for (int t = 0; t < OT_STEMS; ++t) {
        char rel[128], body[64];
        snprintf(rel, sizeof rel, "data/track_light/%s.txt", original_track_stem(t));
        snprintf(body, sizeof body, "# tnfs track light 2\ntrack %s\n", original_track_stem(t));
        put(rel, body);
    }
    put("art/AL1/scenery_00.psd", "psd0"), put("art/AL1/slot_03.psd", "psd3"), put("art/AL1/slot_03.before-paint.psd", "bak"), put("art/AL1/.DS_Store", "x");
}

int main(void)
{
    snprintf(g_root, sizeof g_root, "%s/tnfs_original_track_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(g_root) == NULL) return perror("mkdtemp"), 2;
    fixtures();
    char ws[300], data[300], art[300], why[OT_WHY] = "", dir[400], stem[16], text[512];
    path_of(ws, sizeof ws, "ws"), path_of(data, sizeof data, "data"), path_of(art, sizeof art, "art");
    make("ws");

    {   /* a mask PNG that changed size says so: a count of texels would read "+0 -0" for a mask that is all different (retouch QA) */
        OriginalPublish p;
        memset(&p, 0, sizeof p);
        snprintf(p.item[0].file, sizeof p.item[0].file, "track_masks/AL1/scenery_01.png");
        p.item[0].state = OT_CHANGED, p.item[0].lines_added = p.item[0].lines_removed = -1, p.count = 1;
        char said[300];
        original_track_publish_text(&p, "AL1", said, sizeof said);
        CHECK(strstr(said, "size differs") != NULL && strstr(said, "+-1") == NULL && strstr(said, "+0 -0") == NULL, "a mask of another size: %s", said);
    }
    CHECK(strcmp(original_track_stem(0), "AL1") == 0 && strcmp(original_track_stem(15), "TR7") == 0 && original_track_stem(16) == NULL, "stems");
    CHECK(!original_track_dir(ws, "XX9", dir, sizeof dir), "a stem that is not one of the sixteen has no folder");
    CHECK(original_track_dir(ws, "AL1", dir, sizeof dir) && strstr(dir, "/ws/tweaks/al1-tweak/studio/project") != NULL, "the folder: %s", dir);
    CHECK(!original_track_present(dir), "nothing there yet");

    OriginalAdd add = { 0 };
    CHECK(original_track_add_all(ws, data, art, &add, why, sizeof why), "add all: %s", why);
    CHECK(add.tracks == 16 && add.light_files == 16 && add.psds == 2 && add.skipped == 2 && add.kept == 0, "counts: %d tracks, %d light, %d psds, %d skipped, %d kept", add.tracks, add.light_files, add.psds, add.skipped, add.kept);
    CHECK(original_track_present(dir) && original_track_stem_of(dir, stem, sizeof stem) && strcmp(stem, "AL1") == 0, "AL1 is an original track");
    CHECK(exists("ws/tweaks/tr7-tweak/studio/project/light/track_light/TR7.txt"), "the light file is in");
    CHECK(exists("ws/tweaks/al1-tweak/studio/project/art-source/masks/scenery_00.psd") && exists("ws/tweaks/al1-tweak/studio/project/art-source/masks/slot_03.psd"), "the PSDs are copied");
    CHECK(!exists("ws/tweaks/al1-tweak/studio/project/art-source/masks/slot_03.before-paint.psd") && !exists("ws/tweaks/al1-tweak/studio/project/art-source/masks/.DS_Store"), "backups and strays stay out");
    CHECK(exists("art/AL1/scenery_00.psd"), "the originals stay where they are (copied, not moved)");
    CHECK(original_track_light_dir(dir, text, sizeof text) && strstr(text, "/al1-tweak/studio/project/light") != NULL, "light dir");

    /* a second run touches nothing: the files that are there are kept, even edited ones */
    put("ws/tweaks/al1-tweak/studio/project/light/track_light/AL1.txt", "edited\n");
    OriginalAdd again = { 0 };
    CHECK(original_track_add_all(ws, data, art, &again, why, sizeof why) && again.light_files == 0 && again.psds == 0 && again.kept == 16 + 2, "again: %d copied, %d kept", again.light_files + again.psds, again.kept);
    get("ws/tweaks/al1-tweak/studio/project/light/track_light/AL1.txt", text, sizeof text);
    CHECK(strcmp(text, "edited\n") == 0, "the edited file is kept");
    /* the PSD he painted, the mask PNG made from it and the card he changed are the owner's: Add originals again (the art folder's PSD differs) does not touch them */
    char keep_psd[256], keep_mask[256], keep_card[256];
    get("ws/tweaks/al1-tweak/studio/project/art-source/masks/scenery_00.psd", keep_psd, sizeof keep_psd);
    get("ws/tweaks/al1-tweak/studio/project/art-generated/track_masks/scenery_00.png", keep_mask, sizeof keep_mask);
    get("ws/tweaks/al1-tweak/studio/project/original.ini", keep_card, sizeof keep_card);
    put("ws/tweaks/al1-tweak/studio/project/art-source/masks/scenery_00.psd", "my own psd\n");
    put("ws/tweaks/al1-tweak/studio/project/art-generated/track_masks/scenery_00.png", "my own mask\n");
    put("ws/tweaks/al1-tweak/studio/project/original.ini", "my own card\n");
    CHECK(original_track_add_all(ws, data, art, &again, why, sizeof why) && again.light_files == 0 && again.psds == 0, "again, with edits");
    get("ws/tweaks/al1-tweak/studio/project/art-source/masks/scenery_00.psd", text, sizeof text);
    CHECK(strcmp(text, "my own psd\n") == 0, "the edited PSD is kept: %s", text);
    get("ws/tweaks/al1-tweak/studio/project/art-generated/track_masks/scenery_00.png", text, sizeof text);
    CHECK(strcmp(text, "my own mask\n") == 0, "the edited mask is kept: %s", text);
    get("ws/tweaks/al1-tweak/studio/project/original.ini", text, sizeof text);
    CHECK(strcmp(text, "my own card\n") == 0, "the edited card is kept: %s", text);
    put("ws/tweaks/al1-tweak/studio/project/art-source/masks/scenery_00.psd", keep_psd), put("ws/tweaks/al1-tweak/studio/project/art-generated/track_masks/scenery_00.png", keep_mask), put("ws/tweaks/al1-tweak/studio/project/original.ini", keep_card);   // as it was, for the rest

    /* a workspace of the old light format: Add originals takes the old names away and keeps the one file */
    put("ws/tweaks/al1-tweak/studio/project/light/track/AL1.txt", "old places\n");
    put("ws/tweaks/al1-tweak/studio/project/light/track_light/AL1B.txt", "old time B\n");
    OriginalAdd old = { 0 };
    CHECK(original_track_add_all(ws, data, art, &old, why, sizeof why), "add over old files: %s", why);
    CHECK(!exists("ws/tweaks/al1-tweak/studio/project/light/track/AL1.txt") && !exists("ws/tweaks/al1-tweak/studio/project/light/track_light/AL1B.txt"), "the old light files are gone");
    get("ws/tweaks/al1-tweak/studio/project/light/track_light/AL1.txt", text, sizeof text);
    CHECK(strcmp(text, "edited\n") == 0, "the one file is kept");

    /* a missing light file stops the add with a reason */
    OriginalAdd none = { 0 };
    char gone[300];
    path_of(gone, sizeof gone, "data/track_light/CL2.txt");
    unlink(gone);
    make("ws2"), make("ws2/tracks");
    char ws2[300];
    path_of(ws2, sizeof ws2, "ws2");
    CHECK(!original_track_add(ws2, "CL2", data, NULL, &none, why, sizeof why) && strstr(why, "CL2") != NULL, "a missing light file is named: %s", why);

    /* publish: the plan and the copy */
    OriginalPublish plan;
    CHECK(original_track_publish_plan(dir, data, &plan, why, sizeof why), "plan: %s", why);
    CHECK(plan.count == 1 && plan.changed == 1 && plan.same == 0 && plan.added == 0, "plan: %d files, %d changed, %d same, %d new", plan.count, plan.changed, plan.same, plan.added);
    int changed_at = -1;
    for (int i = 0; i < plan.count; ++i) if (plan.item[i].state == OT_CHANGED) changed_at = i;
    CHECK(changed_at >= 0 && strcmp(plan.item[changed_at].file, "track_light/AL1.txt") == 0 && plan.item[changed_at].lines_added == 1 && plan.item[changed_at].lines_removed == 2,
          "the edited file: +%d -%d", changed_at >= 0 ? plan.item[changed_at].lines_added : -1, changed_at >= 0 ? plan.item[changed_at].lines_removed : -1);
    original_track_publish_text(&plan, "AL1", text, sizeof text);
    CHECK(strstr(text, "track_light/AL1.txt") != NULL && strstr(text, "+1") != NULL, "text: %s", text);
    int written = -1;
    CHECK(original_track_publish(dir, data, &written, why, sizeof why) && written == 1, "published %d: %s", written, why);
    get("data/track_light/AL1.txt", text, sizeof text);
    CHECK(strcmp(text, "edited\n") == 0, "data/ has the edited file");
    CHECK(original_track_publish(dir, data, &written, why, sizeof why) && written == 0, "a second publish writes nothing");

    /* a track whose data/ file is gone is new */
    path_of(gone, sizeof gone, "data/track_light/AL1.txt");
    unlink(gone);
    CHECK(original_track_publish_plan(dir, data, &plan, why, sizeof why) && plan.added == 1, "new when data/ has none: %d", plan.added);

    /* the acceptance check: a track made by hand through the same code path differs from the migrated one only in the mask folders */
    {
        char migrated[400], report[2048] = "";
        original_track_dir(ws, "CL1", migrated, sizeof migrated);
        int n = original_track_acceptance(migrated, "CL1", data, report, sizeof report, why, sizeof why);
        CHECK(n == 0 && strstr(report, "files compared") != NULL, "CL1 is as made by hand: %d %s %s", n, report, why);
        put("data/track_light/AL1.txt", "edited\n");   // the file the test removed above
        original_track_dir(ws, "AL1", migrated, sizeof migrated);
        n = original_track_acceptance(migrated, "AL1", data, report, sizeof report, why, sizeof why);
        CHECK(n == 0, "AL1 has PSDs and its masks folders are not compared: %d %s", n, report);   // its light file was published back equal above
        put("ws/tweaks/al1-tweak/studio/project/light/track_light/AL1.txt", "retouched\n");
        put("ws/tweaks/al1-tweak/studio/project/notes.txt", "extra\n");
        put("ws/tweaks/al1-tweak/studio/project/art-source/masks/more.psd", "psd");   // in the mask folder: allowed
        n = original_track_acceptance(migrated, "AL1", data, report, sizeof report, why, sizeof why);
        CHECK(n == 2 && strstr(report, "light/track_light/AL1.txt: differs") != NULL && strstr(report, "notes.txt: missing in the hand-made folder") != NULL, "two differences named: %d %s", n, report);
        CHECK(original_track_acceptance(migrated, "ZZ9", data, report, sizeof report, why, sizeof why) == -1 && why[0] != '\0', "a stem that is not a track: %s", why);
    }

    /* an old workspace keeps its working copy in tracks/<STEM>: opening the tweak copies it into the tweak's studio/project once, with the PSD he edited, and leaves the old folder alone */
    {
        char old[400], home[400], two[400];
        make("ws3"), make("ws3/tracks"), make("ws3/tracks/AL1"), make("ws3/tracks/AL1/art-source"), make("ws3/tracks/AL1/art-source/masks"), make("ws3/tracks/AL1/light"), make("ws3/tracks/AL1/light/track_light");
        put("ws3/tracks/AL1/original.ini", "kind=original-track\nstem=AL1\n");
        put("ws3/tracks/AL1/light/track_light/AL1.txt", "my light\n");
        put("ws3/tracks/AL1/art-source/masks/slot_03.psd", "my edited psd\n");
        char ws3[300];
        path_of(ws3, sizeof ws3, "ws3");
        CHECK(original_track_old_dir(ws3, "AL1", old, sizeof old) && strstr(old, "/ws3/tracks/AL1") != NULL && !original_track_old_dir(ws3, "XX9", old, sizeof old), "the old folder: %s", old);
        original_track_old_dir(ws3, "AL1", old, sizeof old);
        original_track_dir(ws3, "AL1", home, sizeof home);
        int files = -1;
        CHECK(original_convert(old, home, &files, why, sizeof why) == 1 && files == 3, "converted %d files: %s", files, why);
        CHECK(original_track_present(home) && original_track_stem_of(home, stem, sizeof stem) && strcmp(stem, "AL1") == 0, "the copy is an original track");
        get("ws3/tweaks/al1-tweak/studio/project/art-source/masks/slot_03.psd", text, sizeof text);
        CHECK(strcmp(text, "my edited psd\n") == 0, "the edited PSD came across: %s", text);
        get("ws3/tweaks/al1-tweak/studio/project/light/track_light/AL1.txt", text, sizeof text);
        CHECK(strcmp(text, "my light\n") == 0, "the light came across: %s", text);
        CHECK(exists("ws3/tracks/AL1/art-source/masks/slot_03.psd") && exists("ws3/tracks/AL1/original.ini"), "the old folder is left alone");
        /* once: what he edits in the copy is never overwritten by a second opening, and an edit in the old folder is not copied again */
        put("ws3/tweaks/al1-tweak/studio/project/art-source/masks/slot_03.psd", "edited in the tweak\n");
        put("ws3/tracks/AL1/art-source/masks/slot_03.psd", "edited in the old\n");
        files = -1;
        CHECK(original_convert(old, home, &files, why, sizeof why) == 0 && files == 0, "a second opening converts nothing: %d", files);
        get("ws3/tweaks/al1-tweak/studio/project/art-source/masks/slot_03.psd", text, sizeof text);
        CHECK(strcmp(text, "edited in the tweak\n") == 0, "the copy is kept: %s", text);
        /* nothing old, nothing to do */
        original_track_old_dir(ws3, "CL1", two, sizeof two);
        original_track_dir(ws3, "CL1", home, sizeof home);
        CHECK(original_convert(two, home, &files, why, sizeof why) == 0 && !original_track_present(home), "no old folder: nothing made");
    }

    /* a copy that stops midway leaves no half working copy: the next opening copies everything */
    {
        char old[400], home[400], ws4[300], file[400];
        make("ws4"), make("ws4/tracks"), make("ws4/tracks/AL1"), make("ws4/tracks/AL1/light"), make("ws4/tracks/AL1/art-source");
        put("ws4/tracks/AL1/original.ini", "kind=original-track\nstem=AL1\n");
        put("ws4/tracks/AL1/light/AL1.txt", "my light\n");
        put("ws4/tracks/AL1/art-source/zzz.psd", "my psd\n");   /* read last or first: one of them cannot be read */
        path_of(ws4, sizeof ws4, "ws4");
        original_track_old_dir(ws4, "AL1", old, sizeof old);
        original_track_dir(ws4, "AL1", home, sizeof home);
        path_of(file, sizeof file, "ws4/tracks/AL1/art-source/zzz.psd");
        chmod(file, 0);
        int files = -1;
        CHECK(original_convert(old, home, &files, why, sizeof why) == -1 && why[0] != '\0', "an unreadable file stops the copy: %s", why);
        char part[420];
        snprintf(part, sizeof part, "%s.partial", home);
        CHECK(!exists("ws4/tweaks/al1-tweak/studio/project") && access(part, F_OK) != 0, "no half working copy is left (nor its temporary folder)");
        chmod(file, 0644);
        CHECK(original_convert(old, home, &files, why, sizeof why) == 1 && files == 3, "the next opening copies all: %d %s", files, why);
        CHECK(exists("ws4/tweaks/al1-tweak/studio/project/art-source/zzz.psd") && original_is_home(home) && original_home_state(home) == OH_ORIGINAL, "whole, and marked as an original's");
        /* a link in the old folder is not followed, and the status line is told */
        make("ws4/tracks/CL1"), put("ws4/tracks/CL1/original.ini", "kind=original-track\nstem=CL1\n");
        char target[400], link[400], home2[400], old2[400];
        path_of(target, sizeof target, "ws4/tracks/AL1/original.ini"), path_of(link, sizeof link, "ws4/tracks/CL1/link.ini");
        CHECK(symlink(target, link) == 0, "link made");
        original_track_old_dir(ws4, "CL1", old2, sizeof old2), original_track_dir(ws4, "CL1", home2, sizeof home2);
        CHECK(original_convert(old2, home2, &files, why, sizeof why) == 1 && files == 1 && strstr(why, "1 links") != NULL, "a skipped link is said: %d %s", files, why);
    }

    /* an own tweak with the id of an original is not the original's working copy: nothing is converted into it, filled into it, or refused from Open */
    {
        char old[400], home[400], ws5[300];
        make("ws5"), make("ws5/tracks"), make("ws5/tracks/AL1"), put("ws5/tracks/AL1/original.ini", "kind=original-track\nstem=AL1\n");
        make("ws5/tweaks"), make("ws5/tweaks/al1-tweak"), make("ws5/tweaks/al1-tweak/studio"), make("ws5/tweaks/al1-tweak/studio/project");
        put("ws5/tweaks/al1-tweak/studio/project/mine.txt", "mine\n");
        path_of(ws5, sizeof ws5, "ws5");
        original_track_old_dir(ws5, "AL1", old, sizeof old), original_track_dir(ws5, "AL1", home, sizeof home);
        CHECK(!original_is_home(home) && original_home_state(home) == OH_FOREIGN, "no mark: not an original's");
        CHECK(original_convert(old, home, NULL, why, sizeof why) == -1 && strstr(why, "not a game original") != NULL, "not converted into it: %s", why);
        CHECK(!exists("ws5/tweaks/al1-tweak/studio/project/original.ini"), "nothing of the old folder came in");
        {
            OriginalAdd add = { 0 };
            char data5[300];
            path_of(data5, sizeof data5, "data");
            CHECK(!original_track_add(ws5, "AL1", data5, NULL, &add, why, sizeof why) && strstr(why, "not a game original") != NULL && !exists("ws5/tweaks/al1-tweak/studio/project/light"), "Add originals does not fill an own tweak: %s", why);
        }
        CHECK(original_mark_home(home) && original_is_home(home), "marked: now it is");
        original_drop_home(home);
        CHECK(!exists("ws5/tweaks/al1-tweak/studio/project") && !original_is_home(home) && original_home_state(home) == OH_ABSENT, "dropped with its mark");
    }

    {   // the words of the copy dialog: a changed mask counts pixels (not "texels"), a changed light file lines
        OriginalPublish words = { 0 };
        snprintf(words.item[0].file, sizeof words.item[0].file, "track_masks/AL1/slot_03.png");
        words.item[0].state = OT_CHANGED, words.item[0].lines_added = 8192, words.item[0].lines_removed = 0;
        snprintf(words.item[1].file, sizeof words.item[1].file, "track_light/AL1.txt");
        words.item[1].state = OT_CHANGED, words.item[1].lines_added = 2, words.item[1].lines_removed = 1;
        words.count = 2;
        char said[400];
        original_track_publish_text(&words, "AL1", said, sizeof said);
        CHECK(strstr(said, "slot_03.png changed (+8192 -0 pixels)") != NULL && strstr(said, "AL1.txt changed (+2 -1 lines)") != NULL && strstr(said, "texel") == NULL, "the changes are counted in pixels and lines: %s", said);
    }

    printf("test_editor_original_track: %s\n", g_fail == 0 ? "ok" : "FAILED");
    return g_fail != 0;
}
