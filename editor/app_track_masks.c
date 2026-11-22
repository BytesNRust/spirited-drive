/* The Masks panel of an original track (app.h's app_track_mask*, dep103 phase B): the folder's masks scanned, the PNGs made from the PSDs that need them, one mask shown over its texture. */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/psd_mask.h"
#include "track_mask_make_psd.h"
#include "track_tex.h"

/* The art source of what is open (dep103's TrackArtSource): the folder whose light/, art-source/masks and art-generated/track_masks the panel works on, the track's stem, and where its textures
 * are read from. An original track (the Track light window) reads the CD's; an own track (the track editor) reads the project's own art FAM, art= of its project.ini. */
typedef struct {
    const char *root;
    char stem[16];
    char fam[APP_PATH];   /* "" for an original track: its textures are the CD's */
} ArtSource;

static int art_source(const App *a, ArtSource *out)
{
    memset(out, 0, sizeof *out);
    if (a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0' && original_track_stem_of(a->original_dir, out->stem, sizeof out->stem)) return out->root = a->original_dir, 1;
    if (a->kind != APP_KIND_TRACK || a->dir[0] == '\0') return 0;
    snprintf(out->stem, sizeof out->stem, "%s", a->project.stem);
    if (a->project.art[0] != '\0') {
        if (a->project.art[0] == '/') snprintf(out->fam, sizeof out->fam, "%s", a->project.art);
        else snprintf(out->fam, sizeof out->fam, "%s/%s", a->dir, a->project.art);
    }
    return out->root = a->dir, 1;
}

static int stem_of(const App *a, char *stem, size_t cap)
{
    ArtSource src;
    if (!art_source(a, &src)) return 0;
    snprintf(stem, cap, "%s", src.stem);
    return 1;
}

static double clock_of(const App *a)
{
    return a->kind == APP_KIND_LIGHT ? a->light.now : a->tmask_now;
}

static void drop_picture(App *a)
{
    free(a->tmask_rgba);
    a->tmask_rgba = NULL;
    a->tmask_w = a->tmask_h = 0;
    a->tmask_sel = -1;
    ++a->tmask_serial;
}

/* the textures of the track from the game's files, when they are there: for the sizes and the pictures */
static TrackTexSet *textures(const App *a, const char *stem)
{
    ArtSource src;
    if (art_source(a, &src) && src.fam[0] != '\0') return track_tex_open_fam(src.fam, NULL, 0);
    return a->export_src.cd[0] != '\0' ? track_tex_open(a->export_src.cd, stem, NULL, 0) : NULL;
}

static const char *root_of(const App *a)
{
    ArtSource src;
    return art_source(a, &src) ? src.root : "";
}

int app_track_masks_refresh(App *a)
{
    char stem[16], why[APP_MSG] = "";
    if (!stem_of(a, stem, sizeof stem)) return 0;
    TrackTexSet *set = textures(a, stem);
    TrackTexInfo info[TRACK_TEX_MAX];
    int n = 0;
    for (int i = 0; set != NULL && i < track_tex_count(set) && n < TRACK_TEX_MAX; ++i) info[n++] = *track_tex_info(set, i);
    track_tex_close(set);
    drop_picture(a);
    a->tmask_note[0] = '\0';
    if (!track_masks_scan(root_of(a), info, n, &a->tmasks, why, sizeof why)) {
        memset(&a->tmasks, 0, sizeof a->tmasks);
        snprintf(a->tmask_note, sizeof a->tmask_note, "%s", why);
        return a->tmasks_loaded = 1, 0;
    }
    a->tmask_psd_seen = track_masks_newest_psd(root_of(a));
    return a->tmasks_loaded = 1;
}

int app_track_masks_load(App *a)
{
    if (a->tmasks_loaded) return 1;
    if (!app_track_masks_refresh(a)) return 0;
    int written = 0;
    char why[APP_MSG] = "";
    if (a->tmasks.to_make > 0) {
        const int ok = track_masks_generate(root_of(a), &a->tmasks, &written, why, sizeof why);
        app_track_masks_refresh(a);
        if (!ok) snprintf(a->tmask_note, sizeof a->tmask_note, "%s", why);
        else snprintf(a->status, sizeof a->status, "masks: %d PNG%s made from the PSDs", written, written == 1 ? "" : "s");
    }
    if (a->tmasks.edited > 0) snprintf(a->status, sizeof a->status, "masks: %d PNG%s edited by hand, kept as they are (Regenerate from PSD makes them again)", a->tmasks.edited, a->tmasks.edited == 1 ? "" : "s");
    return 1;
}

int app_track_mask_select(App *a, int i)
{
    char stem[16];
    drop_picture(a);
    a->tmask_note[0] = '\0';
    if (!stem_of(a, stem, sizeof stem) || i < 0 || i >= a->tmasks.count) return 0;
    const TrackMaskRow *row = &a->tmasks.row[i];
    unsigned char *marks = NULL;
    unsigned mw = 0, mh = 0;
    if (row->state == TM_NONE) {   // a texture with no mask: the texture as it is
        TrackTexSet *set = textures(a, stem);
        int tw = 0, th = 0;
        uint8_t *tex = NULL;
        const int ok = set != NULL && track_tex_rgba(set, row->name, &tw, &th, &tex);
        track_tex_close(set);
        a->tmask_sel = i;
        if (!ok) return snprintf(a->tmask_note, sizeof a->tmask_note, "the game's files were not found: there is no texture to show"), 0;
        a->tmask_rgba = tex, a->tmask_w = tw, a->tmask_h = th;
        ++a->tmask_serial;
        snprintf(a->tmask_note, sizeof a->tmask_note, "this texture has no mask");
        return 1;
    }
    if (!row->has_png || !track_masks_load(root_of(a), row->name, &marks, &mw, &mh)) {
        snprintf(a->tmask_note, sizeof a->tmask_note, row->state == TM_EMPTY ? "nothing is painted in this PSD's mask layer, and there is no PNG" : "no PNG to show");
        a->tmask_sel = i;
        return 0;
    }
    TrackTexSet *set = textures(a, stem);
    int tw = 0, th = 0;
    uint8_t *tex = NULL;
    if (set == NULL || !track_tex_rgba(set, row->name, &tw, &th, &tex)) {
        tw = (int)mw, th = (int)mh;   // no texture (the game's files are not found): the mask alone, on dark grey
        tex = malloc((size_t)tw * (size_t)th * 4);
        for (size_t k = 0; tex != NULL && k < (size_t)tw * (size_t)th; ++k) memcpy(tex + k * 4, "\x30\x30\x30\xff", 4);
        snprintf(a->tmask_note, sizeof a->tmask_note, "the game's files were not found: the mask is shown without its texture");
    }
    track_tex_close(set);
    if (tex == NULL) return free(marks), 0;
    for (int y = 0; y < th; ++y)   // the mask scaled to the texture by nearest neighbour, as the game does
        for (int x = 0; x < tw; ++x) {
            const unsigned char m = marks[((size_t)y * mh / (size_t)th) * mw + ((size_t)x * mw / (size_t)tw)];
            uint8_t *p = tex + ((size_t)y * (size_t)tw + (size_t)x) * 4;
            if (m == TRACK_MASK_REFLECTIVE) p[0] = (uint8_t)((p[0] + 2 * 0) / 3), p[1] = (uint8_t)((p[1] + 2 * 230) / 3), p[2] = (uint8_t)((p[2] + 2 * 210) / 3), p[3] = 255;
            else if (m == TRACK_MASK_LAMP) p[0] = 255, p[1] = 230, p[2] = 0, p[3] = 255;
        }
    free(marks);
    a->tmask_rgba = tex, a->tmask_w = tw, a->tmask_h = th, a->tmask_sel = i;
    ++a->tmask_serial;
    return 1;
}

int app_track_mask_regenerate(App *a)
{
    char why[APP_MSG] = "", name[TRACK_TEX_NAME];
    const int sel = a->tmask_sel;
    if (sel < 0 || sel >= a->tmasks.count) return snprintf(a->tmask_note, sizeof a->tmask_note, "pick a mask first"), 0;
    snprintf(name, sizeof name, "%s", a->tmasks.row[sel].name);
    if (!track_masks_regenerate(root_of(a), name, why, sizeof why)) return snprintf(a->tmask_note, sizeof a->tmask_note, "%s", why), 0;
    app_track_masks_refresh(a);
    for (int i = 0; i < a->tmasks.count; ++i)
        if (strcmp(a->tmasks.row[i].name, name) == 0) app_track_mask_select(a, i);
    snprintf(a->status, sizeof a->status, "masks: %s made again from its PSD", name);
    return 1;
}

int app_track_mask_create_psd(App *a)
{
    char stem[16], name[TRACK_TEX_NAME], why[APP_MSG] = "";
    const int sel = a->tmask_sel;
    if (!stem_of(a, stem, sizeof stem)) return 0;
    if (sel < 0 || sel >= a->tmasks.count) return snprintf(a->tmask_note, sizeof a->tmask_note, "pick a texture first"), 0;
    if (a->tmasks.row[sel].has_psd) return snprintf(a->tmask_note, sizeof a->tmask_note, "this texture has a PSD already"), 0;
    snprintf(name, sizeof name, "%s", a->tmasks.row[sel].name);
    TrackTexSet *set = textures(a, stem);
    int tw = 0, th = 0;
    uint8_t *tex = NULL;
    const int have = set != NULL && track_tex_rgba(set, name, &tw, &th, &tex);
    track_tex_close(set);
    if (!have) return snprintf(a->tmask_note, sizeof a->tmask_note, "the game's files were not found: there is no texture to make a PSD from"), 0;
    const int ok = track_mask_make_psd(a->py.python, a->tools, root_of(a), stem, name, tex, tw, th, why, sizeof why);
    free(tex);
    if (!ok) return snprintf(a->tmask_note, sizeof a->tmask_note, "%s", why), 0;
    app_track_masks_refresh(a);
    for (int i = 0; i < a->tmasks.count; ++i)
        if (strcmp(a->tmasks.row[i].name, name) == 0) app_track_mask_select(a, i);
    snprintf(a->status, sizeof a->status, "masks: %s.psd made in art-source/masks: paint its mask layer", name);
    return 1;
}

/* Copy to game repo (app.h): the preview is the light files and the masks' PNGs of the open original track against data/; the copy writes the changed and new ones */
int app_original_copy_ask(App *a)
{
    OriginalPublish *plan = &a->publish_plan;
    char why[OT_WHY] = "", stem[16];
    if (!app_original_can_copy(a)) return snprintf(a->status, sizeof a->status, "copy to game repo: the game repo's data/ is not set (--port-data, or repo data in workspace.txt)"), 0;
    if (app_modified(a)) return snprintf(a->status, sizeof a->status, "copy to game repo: Save the light file first"), 0;
    if (!original_track_stem_of(a->original_dir, stem, sizeof stem)) return 0;
    app_track_masks_load(a);   // the PNGs are made from the PSDs before they are compared
    if (!original_track_publish_plan(a->original_dir, app_repo_data(a), plan, why, sizeof why) || !track_masks_publish_plan(a->original_dir, app_repo_data(a), stem, plan, why, sizeof why))
        return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    original_track_publish_text(plan, stem, a->publish_text, sizeof a->publish_text);
    a->publish_dialog = 1;
    return 1;
}

int app_original_copy(App *a)
{
    char why[OT_WHY] = "";
    int written = 0;
    a->publish_dialog = 0;
    if (!app_original_can_copy(a)) return 0;
    if (!original_track_publish(a->original_dir, app_repo_data(a), &written, why, sizeof why) || !track_masks_publish(a->original_dir, app_repo_data(a), &a->publish_plan, &written, why, sizeof why))
        return snprintf(a->status, sizeof a->status, "copy to game repo: %s", why), 0;
    snprintf(a->status, sizeof a->status, "copied to the game repo: %d file%s written into %s", written, written == 1 ? "" : "s", app_repo_data(a));
    return 1;
}

int app_original_check_all(App *a, char *out, size_t cap)
{
    int tracks = 0, masks = 0, equal = 0, differ = 0, added = 0, failed = 0, accept_bad = 0;
    char detail[1024] = "";
    char why[OT_WHY] = "";
    if (a->workspace[0] == '\0' || app_repo_data(a)[0] == '\0') return snprintf(out, cap, "originals: needs a workspace and the game's data folder"), 0;
    for (int i = 0; i < OT_STEMS; ++i) {
        char dir[APP_PATH];
        if (!original_track_dir(a->workspace, original_track_stem(i), dir, sizeof dir) || !original_track_present(dir)) continue;
        ++tracks;
        static TrackMasks m;   // big: not on the stack
        int written = 0;
        OriginalPublish plan;
        memset(&plan, 0, sizeof plan);
        if (!track_masks_scan(dir, NULL, 0, &m, why, sizeof why)) continue;   // no masks in that folder
        if (!track_masks_generate(dir, &m, &written, why, sizeof why) || !track_masks_publish_plan(dir, app_repo_data(a), original_track_stem(i), &plan, why, sizeof why)) ++failed;
        masks += plan.count, equal += plan.same, differ += plan.changed, added += plan.added;
        char report[1024] = "";   // the acceptance check: a track made by hand through the same code path may differ only in the mask folders
        const int bad = original_track_acceptance(dir, original_track_stem(i), a->data_dir, report, sizeof report, why, sizeof why);
        if (bad < 0) ++failed;
        else if (bad > 0 && detail[0] == '\0') snprintf(detail, sizeof detail, " %s:\n%s", original_track_stem(i), report);
        accept_bad += bad > 0 ? bad : 0;
    }
    snprintf(out, cap, "originals: %d tracks, %d masks: %d equal, %d differ, %d new in the workspace; acceptance: %d differences outside art-source/masks%s%s%s", tracks, masks, equal, differ, added, accept_bad,
             failed > 0 ? "; failed: " : "", failed > 0 ? why : "", detail);
    return differ == 0 && failed == 0 && accept_bad == 0;
}

int app_track_masks_poll(App *a)
{
    const char *root = root_of(a);
    if (!a->tmasks_loaded || root[0] == '\0' || clock_of(a) < a->tmask_check_at) return 0;
    a->tmask_check_at = clock_of(a) + 1.0;
    if (track_masks_newest_psd(root) <= a->tmask_psd_seen) return 0;
    a->tmasks_loaded = 0;   // a PSD was saved again: scan, make the PNGs, and show the photos again with them
    app_track_masks_load(a);
    if (a->kind == APP_KIND_LIGHT) {
        ++a->light.edit_serial;
        a->light.edit_at = a->light.now;
        memset(a->light.compare.shown, 0, sizeof a->light.compare.shown);
    }
    snprintf(a->status, sizeof a->status, a->kind == APP_KIND_LIGHT ? "masks: a PSD was saved again: the PNGs and the photo are made again" : "masks: a PSD was saved again: the PNGs are made again");
    return 1;
}
