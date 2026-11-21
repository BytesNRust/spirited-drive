/* The textures of a CD track (editor/track_tex.h): needs the game's files. usage: test_editor_track_tex <cd dir> [<art root: <TRACK>/<name>.psd>]
 * AL1 has road slots and scenery boards in the order the masks name them; every texture reads as RGBA of its own size with colour 0xFF transparent; a missing track says so in words.
 * With the art root every PSD of the owner's must be the size of the texture it is named for (the mask is painted at native size). */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/chunk.h"
#include "formats/psd_read.h"
#include "game/race/track_art.h"
#include "track_tex.h"

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; fprintf(stderr, "FAIL line %d: ", __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    rewind(f);
    uint8_t *b = malloc((size_t)n + 1);
    *len = b != NULL && fread(b, 1, (size_t)n, f) == (size_t)n ? (size_t)n : 0;
    fclose(f);
    return b;
}

/* the Studio's reader (on the core alone) against the game's own loader, on all sixteen tracks: the same textures, sizes and pixels */
static void against_the_game(const char *cd)
{
    static const char *const T[] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7" };
    static TrackArt art;
    int compared = 0;
    for (size_t t = 0; t < sizeof T / sizeof T[0]; ++t) {
        char path[1024], why[200] = "";
        snprintf(path, sizeof path, "%s/SIMDATA/ETRACKFM/%s_001.FAM", cd, T[t]);
        size_t len = 0;
        uint8_t *fam = slurp(path, &len);
        Chunk root;
        CHECK(fam != NULL && chunk_open(&root, fam, len) == CHUNK_OK && track_art_load(&art, &root) && art.has_palette, "%s: the game's loader reads it", T[t]);
        TrackTexSet *set = track_tex_open(cd, T[t], why, sizeof why);
        CHECK(set != NULL, "%s: %s", T[t], why);
        int expect = 0;
        for (int i = 0; fam != NULL && set != NULL && i < TRACK_TEXTURE_SLOTS + TRACK_SCENERY_TEXTURES; ++i) {
            const int slot = i < TRACK_TEXTURE_SLOTS;
            const TrackTexture *tx = slot ? &art.slots[i][0] : &art.scenery[i - TRACK_TEXTURE_SLOTS];
            if (!tx->present) continue;
            char name[32];
            snprintf(name, sizeof name, slot ? "slot_%02d" : "scenery_%02d", slot ? i : i - TRACK_TEXTURE_SLOTS);
            int w = 0, h = 0;
            uint8_t *rgba = NULL;
            CHECK(track_tex_rgba(set, name, &w, &h, &rgba) && w == tx->bitmap.width && h == tx->bitmap.height, "%s/%s: the Studio's reader has it, the same size", T[t], name);
            for (int k = 0; rgba != NULL && k < w * h; ++k) {
                const uint8_t v = tx->bitmap.payload[k];
                if (memcmp(rgba + 4 * k, art.palette + 3 * v, 3) != 0 || rgba[4 * k + 3] != (v == 255 ? 0 : 255)) { CHECK(0, "%s/%s: texel %d differs", T[t], name, k); break; }
            }
            free(rgba);
            ++expect;
            ++compared;
        }
        CHECK(set != NULL && track_tex_count(set) == expect, "%s: %d textures, the game's loader %d", T[t], track_tex_count(set), expect);
        track_tex_close(set);
        free(fam);
    }
    printf("track_tex: %d textures equal to the game's loader\n", compared);
}

static void sizes_of_psds(const char *cd, const char *art)
{
    static const char *const T[] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR3", "TR4", "TR6" };
    int checked = 0;
    for (size_t t = 0; t < sizeof T / sizeof T[0]; ++t) {
        char why[200] = "", dir[1024];
        TrackTexSet *set = track_tex_open(cd, T[t], why, sizeof why);
        CHECK(set != NULL, "%s: %s", T[t], why);
        snprintf(dir, sizeof dir, "%s/%s", art, T[t]);
        DIR *d = opendir(dir);
        for (struct dirent *e = d != NULL && set != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) {
            const size_t n = strlen(e->d_name);
            if (n < 5 || strcmp(e->d_name + n - 4, ".psd") != 0) continue;
            char path[1536], name[32];
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            snprintf(name, sizeof name, "%.*s", (int)(n - 4), e->d_name);
            size_t len = 0;
            uint8_t *bytes = slurp(path, &len);
            PsdFile psd;
            const char *want[1] = { "zzz" };
            CHECK(bytes != NULL && psd_parse(bytes, len, want, 1, &psd, why, sizeof why), "%s/%s: %s", T[t], e->d_name, why);
            int found = 0;
            for (int i = 0; i < track_tex_count(set); ++i) {
                const TrackTexInfo *info = track_tex_info(set, i);
                if (strcmp(info->name, name) != 0) continue;
                found = 1;
                CHECK((uint32_t)info->width == psd.width && (uint32_t)info->height == psd.height, "%s/%s is %ux%u, the texture %dx%d", T[t], e->d_name, psd.width, psd.height, info->width, info->height);
            }
            CHECK(found, "%s/%s: the track has no texture of that name", T[t], e->d_name);
            psd_free(&psd);
            free(bytes);
            ++checked;
        }
        if (d != NULL) closedir(d);
        track_tex_close(set);
    }
    printf("track_tex: %d PSDs checked against their textures\n", checked);
}

int main(int argc, char **argv)
{
    if (argc < 2) return fprintf(stderr, "usage: test_editor_track_tex <cd dir> [<art root>]\n"), 2;
    char why[200] = "";
    TrackTexSet *set = track_tex_open(argv[1], "AL1", why, sizeof why);
    CHECK(set != NULL, "AL1 opens: %s", why);
    if (set != NULL) {
        const int n = track_tex_count(set);
        CHECK(n > 20, "AL1 has %d textures", n);
        int slots = 0, boards = 0, transparent = 0;
        for (int i = 0; i < n; ++i) {
            const TrackTexInfo *info = track_tex_info(set, i);
            slots += strncmp(info->name, "slot_", 5) == 0, boards += strncmp(info->name, "scenery_", 8) == 0;
            CHECK(i == 0 || strcmp(track_tex_info(set, i - 1)->name, info->name) != 0, "%s twice", info->name);
            int w = 0, h = 0;
            uint8_t *rgba = NULL;
            CHECK(track_tex_rgba(set, info->name, &w, &h, &rgba) && w == info->width && h == info->height && rgba != NULL, "%s reads", info->name);
            for (int k = 0; rgba != NULL && k < w * h; ++k) transparent += rgba[4 * k + 3] == 0;
            free(rgba);
        }
        CHECK(slots > 0 && boards > 0 && slots + boards == n, "%d slots, %d boards of %d", slots, boards, n);
        CHECK(transparent > 0, "a board has transparent texels (colour 0xFF)");
        int w, h;
        uint8_t *none = NULL;
        CHECK(!track_tex_rgba(set, "slot_999", &w, &h, &none), "no such texture");
        track_tex_close(set);
    }
    CHECK(track_tex_open(argv[1], "ZZ9", why, sizeof why) == NULL && strstr(why, "ZZ9") != NULL, "a track with no file says so: %s", why);
    against_the_game(argv[1]);
    if (argc > 2) sizes_of_psds(argv[1], argv[2]);
    printf("test_editor_track_tex: %s\n", g_fail == 0 ? "ok" : "FAILED");
    return g_fail != 0;
}
