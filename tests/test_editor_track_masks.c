#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
/* The masks of an original track's folder (editor/track_masks.h): the states, generating the PNGs from the PSDs, never overwriting a hand-edited PNG or writing from an empty
 * mask layer, lamps as an RGBA PNG. PSDs written by the test fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "formats/png.h"
#include "formats/psd_mask.h"
#include "psd_fixture.h"
#include "track_masks.h"

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; fprintf(stderr, "FAIL line %d: ", __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static char g_dir[300];

static void path_of(char *out, size_t cap, const char *rel) { snprintf(out, cap, "%s/%s", g_dir, rel); }

static void put_bytes(const char *rel, const uint8_t *b, size_t n)
{
    char path[512];
    path_of(path, sizeof path, rel);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(b, 1, n, f), fclose(f);
}

static void age(const char *rel, int seconds_ago)
{
    char path[512];
    path_of(path, sizeof path, rel);
    struct timeval tv[2];
    gettimeofday(&tv[0], NULL);
    tv[0].tv_sec -= seconds_ago;
    tv[1] = tv[0];
    utimes(path, tv);
}

/* a 4 x 1 PSD with the mask layer `mask` (RGBA texels) and a painted hidden draft */
static void psd(const char *name, const uint8_t mask[16])
{
    static const uint8_t draft[16] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    FxLayer l[2] = { { "mask", NULL, 0, 0, 0, 4, 1, mask, 0, 1, 0 }, { "draft", NULL, 0, 0, 0, 4, 1, draft, 1, 1, 0 } };
    FxBuf f = fx_psd(4, 1, l, 2, 8, 3, 1);
    char rel[128];
    snprintf(rel, sizeof rel, "art-source/masks/%s.psd", name);
    put_bytes(rel, f.p, f.n);
    free(f.p);
}

static const TrackMaskRow *row(const TrackMasks *m, const char *name)
{
    for (int i = 0; i < m->count; ++i) if (strcmp(m->row[i].name, name) == 0) return &m->row[i];
    return NULL;
}

/* the marks of a generated PNG: 0 none, 1 reflective, 2 lamp (the game's reading); *rgba 1 when the file is RGBA */
static int read_png(const char *rel, uint8_t marks[4], int *rgba)
{
    char path[512];
    path_of(path, sizeof path, rel);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    static uint8_t buf[1 << 16];
    const size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    *rgba = n > 25 && buf[25] == 6;
    PngImage img;
    if (png_read(buf, n, &img) != PNG_OK || img.width != 4 || img.height != 1) return 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t px[4];
        if (img.indexed) { px[0] = img.palette[img.pixels[i] * 3], px[1] = img.palette[img.pixels[i] * 3 + 1], px[2] = img.palette[img.pixels[i] * 3 + 2], px[3] = 255; }
        else memcpy(px, img.pixels + i * 4, 4);
        marks[i] = px[3] != 0 && px[0] >= 128 ? ((px[1] >= 128 && px[2] < 128) ? 2 : 1) : 0;
    }
    png_free(img.pixels);
    return 1;
}

int main(void)
{
    snprintf(g_dir, sizeof g_dir, "%s/tnfs_track_masks_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(g_dir) == NULL) return perror("mkdtemp"), 2;
    char p[400], why[200] = "";
    for (const char *s = "art-source art-source/masks art-generated art-generated/track_masks"; *s != '\0';) {
        char d[128];
        int n = 0;
        while (*s != '\0' && *s != ' ') d[n++] = *s++;
        d[n] = '\0';
        if (*s == ' ') ++s;
        path_of(p, sizeof p, d);
        mkdir(p, 0777);
    }
    static const uint8_t PAINT[16] = { 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0 };         /* texels 0 and 2 reflective */
    static const uint8_t LAMPS[16] = { 255, 255, 255, 255, 255, 255, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0 };           /* texel 0 reflective, 1 a lamp */
    static const uint8_t NONE[16] = { 0 };
    psd("slot_01", PAINT), psd("slot_02", LAMPS), psd("scenery_00", NONE), psd("scenery_01", PAINT), psd("slot_03", PAINT);
    put_bytes("art-source/masks/slot_01.before-x.psd", PAINT, 16);   /* a backup: not a mask PSD */
    put_bytes("art-source/masks/notes.txt", PAINT, 4);
    static const uint8_t KEEP_PNG[1] = { 0 };
    (void)KEEP_PNG;

    TrackMasks m;
    CHECK(track_masks_scan(g_dir, NULL, 0, &m, why, sizeof why), "scan: %s", why);
    CHECK(m.count == 5, "5 PSDs, no backup or stray: %d", m.count);
    CHECK(row(&m, "slot_01") != NULL && row(&m, "slot_01")->state == TM_NEW && row(&m, "slot_01")->reflective == 2 && row(&m, "slot_01")->width == 4, "slot_01 is new with 2 texels");
    CHECK(row(&m, "slot_02")->lamps == 1 && row(&m, "slot_02")->reflective == 1, "slot_02 has a lamp");
    CHECK(row(&m, "scenery_00")->state == TM_EMPTY, "an empty mask layer is not a source (the hidden draft is never read)");
    CHECK(m.to_make == 4 && m.edited == 0, "4 to make: %d", m.to_make);

    int written = -1;
    CHECK(track_masks_generate(g_dir, &m, &written, why, sizeof why) && written == 4, "generate: %d %s", written, why);
    uint8_t marks[4];
    int rgba = -1;
    CHECK(read_png("art-generated/track_masks/slot_01.png", marks, &rgba) && rgba == 0 && marks[0] == 1 && marks[1] == 0 && marks[2] == 1, "slot_01.png is grey, texels 0 and 2");
    CHECK(read_png("art-generated/track_masks/slot_02.png", marks, &rgba) && rgba == 1 && marks[0] == 1 && marks[1] == 2 && marks[2] == 0, "slot_02.png is RGBA with a lamp");
    {
        char png[400];
        path_of(png, sizeof png, "art-generated/track_masks/scenery_00.png");
        CHECK(access(png, F_OK) != 0, "nothing is generated from an empty mask layer");
    }
    CHECK(track_masks_scan(g_dir, NULL, 0, &m, why, sizeof why) && m.to_make == 0 && row(&m, "slot_01")->state == TM_OK && row(&m, "slot_02")->state == TM_OK, "all ok after generate");
    CHECK(track_masks_generate(g_dir, &m, &written, why, sizeof why) && written == 0, "a second generate writes nothing");

    /* the PSD is repainted: newer than its PNG -> stale -> generated again */
    psd("slot_01", LAMPS);
    age("art-generated/track_masks/slot_01.png", 100);
    CHECK(track_masks_scan(g_dir, NULL, 0, &m, why, sizeof why) && row(&m, "slot_01")->state == TM_STALE && m.to_make == 1, "slot_01 is stale");
    CHECK(track_masks_generate(g_dir, &m, &written, why, sizeof why) && written == 1, "regenerated");
    CHECK(read_png("art-generated/track_masks/slot_01.png", marks, &rgba) && rgba == 1 && marks[1] == 2, "the new PNG has the lamp");

    /* a PNG changed by hand (newer than its PSD, different): edited, kept by generate, Regenerate puts it back */
    age("art-source/masks/slot_03.psd", 200);
    static const uint8_t HAND[4] = { 0 };
    (void)HAND;
    {   /* write slot_03.png: all four texels reflective */
        uint8_t grey[4] = { 255, 255, 255, 255 }, *png = NULL;
        size_t len = 0;
        CHECK(png_write_grey(grey, 4, 1, &png, &len) == PNG_OK, "png");
        put_bytes("art-generated/track_masks/slot_03.png", png, len);
        png_free(png);
    }
    CHECK(track_masks_scan(g_dir, NULL, 0, &m, why, sizeof why) && row(&m, "slot_03")->state == TM_EDITED && m.edited == 1 && m.to_make == 0, "slot_03 was edited by hand");
    CHECK(row(&m, "slot_03")->reflective == 4, "an edited row counts the PNG's texels (the game's): %d", row(&m, "slot_03")->reflective);
    CHECK(track_masks_generate(g_dir, &m, &written, why, sizeof why) && written == 0, "generate keeps the edited PNG");
    read_png("art-generated/track_masks/slot_03.png", marks, &rgba);
    CHECK(marks[1] == 1, "still the hand-made one");
    CHECK(track_masks_regenerate(g_dir, "slot_03", why, sizeof why), "regenerate: %s", why);
    read_png("art-generated/track_masks/slot_03.png", marks, &rgba);
    CHECK(marks[0] == 1 && marks[1] == 0 && marks[2] == 1 && marks[3] == 0, "the PSD's mask is back");
    CHECK(!track_masks_regenerate(g_dir, "scenery_00", why, sizeof why) && strstr(why, "nothing") != NULL, "regenerate refuses an empty mask layer: %s", why);
    CHECK(!track_masks_regenerate(g_dir, "slot_99", why, sizeof why), "regenerate of a name with no PSD");

    /* a PNG kept next to an empty PSD; a PNG with no PSD is its own source; both survive generate */
    {
        uint8_t grey[4] = { 255, 0, 0, 255 }, *png = NULL;
        size_t len = 0;
        png_write_grey(grey, 4, 1, &png, &len);
        put_bytes("art-generated/track_masks/scenery_00.png", png, len);
        put_bytes("art-generated/track_masks/slot_10.png", png, len);
        png_free(png);
    }
    CHECK(track_masks_scan(g_dir, NULL, 0, &m, why, sizeof why) && row(&m, "scenery_00")->state == TM_EMPTY && row(&m, "scenery_00")->has_png && row(&m, "slot_10")->state == TM_PNG_SOURCE && row(&m, "slot_10")->reflective == 2, "empty PSD with a PNG, PNG alone");
    CHECK(track_masks_generate(g_dir, &m, &written, why, sizeof why) && written == 0, "generate touches neither");
    read_png("art-generated/track_masks/scenery_00.png", marks, &rgba);
    CHECK(marks[0] == 1 && marks[3] == 1, "the kept PNG is as it was");

    /* the texture sizes come from the list when given */
    const TrackTexInfo tex[1] = { { "slot_10", 64, 32 } };
    CHECK(track_masks_scan(g_dir, tex, 1, &m, why, sizeof why) && row(&m, "slot_10")->width == 64 && row(&m, "slot_10")->height == 32, "sizes from the texture list");

    /* every texture of the track is a row: the ones with no PSD and no PNG are TM_NONE, with their size, in order; they are not work to do and change nothing in generate */
    {
        const TrackTexInfo all[4] = { { "slot_10", 64, 32 }, { "slot_00", 128, 256 }, { "scenery_05", 32, 32 }, { "slot_01", 4, 1 } };
        CHECK(track_masks_scan(g_dir, all, 4, &m, why, sizeof why), "scan with the textures: %s", why);
        CHECK(row(&m, "slot_00") != NULL && row(&m, "slot_00")->state == TM_NONE && row(&m, "slot_00")->width == 128 && row(&m, "slot_00")->height == 256 && !row(&m, "slot_00")->has_psd && !row(&m, "slot_00")->has_png, "slot_00 has no mask");
        CHECK(row(&m, "scenery_05")->state == TM_NONE, "scenery_05 has no mask");
        CHECK(row(&m, "slot_01")->state != TM_NONE && row(&m, "slot_10")->state == TM_PNG_SOURCE, "a texture with a PSD or a PNG is not a 'no mask' row");
        int none = 0, last_rank_ok = 1;
        for (int i = 0; i < m.count; ++i) {
            none += m.row[i].state == TM_NONE;
            if (i > 0 && strncmp(m.row[i - 1].name, "scenery", 7) == 0 && strncmp(m.row[i].name, "slot", 4) == 0) last_rank_ok = 0;   /* the slots first, then the boards */
        }
        CHECK(none == 2 && last_rank_ok, "two rows with no mask, slots before boards: %d", none);
        CHECK(m.to_make == 0 && m.edited == 0, "no mask is not work: to_make %d edited %d", m.to_make, m.edited);
        CHECK(strcmp(track_mask_state_text(TM_NONE), "no mask") == 0, "words");
        CHECK(track_masks_generate(g_dir, &m, &written, why, sizeof why) && written == 0, "generate writes nothing for them");
        const TrackTexInfo some[1] = { { "slot_77", 8, 8 } };
        char gone[300];
        snprintf(gone, sizeof gone, "%s/nothing", g_dir);
        CHECK(track_masks_scan(gone, some, 1, &m, why, sizeof why) && m.count == 1 && m.row[0].state == TM_NONE, "a track folder with no masks still lists its textures");
        CHECK(!track_masks_scan(gone, NULL, 0, &m, why, sizeof why), "no folder and no textures is still refused");
    }

    /* a PSD that does not read */
    put_bytes("art-source/masks/slot_20.psd", (const uint8_t *)"not a psd", 9);
    CHECK(track_masks_scan(g_dir, NULL, 0, &m, why, sizeof why) && row(&m, "slot_20")->state == TM_BAD && row(&m, "slot_20")->note[0] != '\0', "a bad PSD says why");
    CHECK(strcmp(track_mask_state_text(TM_EDITED), "edited by hand") == 0, "words");

    /* publish: every PNG against data/track_masks/<STEM>, compared as marks */
    {
        char data[300], plan_text[2048];
        snprintf(data, sizeof data, "%s/data", g_dir);
        mkdir(data, 0777);
        OriginalPublish plan;
        memset(&plan, 0, sizeof plan);
        CHECK(track_masks_publish_plan(g_dir, data, "CL1", &plan, why, sizeof why), "publish plan: %s", why);
        CHECK(plan.count == 6 && plan.added == 6 && plan.changed == 0, "the 6 PNGs are new: %d items, %d new", plan.count, plan.added);
        int n = 0;
        CHECK(track_masks_publish(g_dir, data, &plan, &n, why, sizeof why) && n == 6, "published %d: %s", n, why);
        char copy[400];
        snprintf(copy, sizeof copy, "%s/track_masks/CL1/slot_02.png", data);
        CHECK(access(copy, F_OK) == 0, "the PNG is in data/track_masks/CL1");
        memset(&plan, 0, sizeof plan);
        CHECK(track_masks_publish_plan(g_dir, data, "CL1", &plan, why, sizeof why) && plan.same == 6 && plan.added == 0, "a second plan: all the same (%d)", plan.same);
        /* data/ has another mask for slot_01: the game's is texel 1 only; ours has 0 and 2 -> +2 -1 */
        {
            uint8_t grey[4] = { 0, 255, 0, 0 }, *png = NULL;
            size_t len = 0;
            png_write_grey(grey, 4, 1, &png, &len);
            snprintf(copy, sizeof copy, "%s/track_masks/CL1/slot_01.png", data);
            FILE *f = fopen(copy, "wb");
            if (f != NULL) fwrite(png, 1, len, f), fclose(f);
            png_free(png);
        }
        memset(&plan, 0, sizeof plan);
        CHECK(track_masks_publish_plan(g_dir, data, "CL1", &plan, why, sizeof why) && plan.changed == 1 && plan.same == 5, "one changed: %d", plan.changed);
        int at = -1;
        for (int i = 0; i < plan.count; ++i) if (plan.item[i].state == OT_CHANGED) at = i;
        CHECK(at >= 0 && strcmp(plan.item[at].file, "track_masks/CL1/slot_01.png") == 0 && plan.item[at].lines_added == 2 && plan.item[at].lines_removed == 1, "+%d -%d texels", at >= 0 ? plan.item[at].lines_added : -1, at >= 0 ? plan.item[at].lines_removed : -1);
        original_track_publish_text(&plan, "CL1", plan_text, sizeof plan_text);
        CHECK(strstr(plan_text, "slot_01.png changed (+2 -1 pixels)") != NULL && strstr(plan_text, "5 unchanged") != NULL, "text: %s", plan_text);
        n = 0;
        CHECK(track_masks_publish(g_dir, data, &plan, &n, why, sizeof why) && n == 1, "only the changed one is written: %d", n);
    }

    /* the newest PSD's time: the poll of the panel */
    {
        char first[400];
        path_of(first, sizeof first, "art-source/masks/slot_03.psd");
        struct stat a, b;
        CHECK(stat(first, &a) == 0 && track_masks_newest_psd(g_dir) > 0, "a PSD has a time");
        age("art-source/masks/slot_01.psd", 5000);
        const double before = track_masks_newest_psd(g_dir);
        psd("slot_01", PAINT);   // saved again: newer than all the others
        CHECK(track_masks_newest_psd(g_dir) >= before && stat(first, &b) == 0, "saved again, not older");
        CHECK(track_masks_newest_psd("/nonexistent/folder") == 0, "no folder, no time");
    }

    /* a folder with neither */
    char other[300];
    snprintf(other, sizeof other, "%s/art-source", g_dir);
    CHECK(!track_masks_scan("/nonexistent/folder", NULL, 0, &m, why, sizeof why), "a folder with no masks");

    printf("test_editor_track_masks: %s\n", g_fail == 0 ? "ok" : "FAILED");
    return g_fail != 0;
}
