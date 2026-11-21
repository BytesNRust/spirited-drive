/* The Masks panel of an original track (app.h's app_track_mask*, dep103 phase B): the folder's masks scanned, the PNGs made from the PSDs that need them, one mask shown over its texture. */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/psd_mask.h"
#include "track_tex.h"

static int stem_of(const App *a, char *stem, size_t cap)
{
    return a->kind == APP_KIND_LIGHT && a->original_dir[0] != '\0' && original_track_stem_of(a->original_dir, stem, cap);
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
    return a->export_src.cd[0] != '\0' ? track_tex_open(a->export_src.cd, stem, NULL, 0) : NULL;
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
    if (!track_masks_scan(a->original_dir, info, n, &a->tmasks, why, sizeof why)) {
        memset(&a->tmasks, 0, sizeof a->tmasks);
        snprintf(a->tmask_note, sizeof a->tmask_note, "%s", why);
        return a->tmasks_loaded = 1, 0;
    }
    return a->tmasks_loaded = 1;
}

int app_track_masks_load(App *a)
{
    if (a->tmasks_loaded) return 1;
    if (!app_track_masks_refresh(a)) return 0;
    int written = 0;
    char why[APP_MSG] = "";
    if (a->tmasks.to_make > 0) {
        const int ok = track_masks_generate(a->original_dir, &a->tmasks, &written, why, sizeof why);
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
    if (!row->has_png || !track_masks_load(a->original_dir, row->name, &marks, &mw, &mh)) {
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
    if (!track_masks_regenerate(a->original_dir, name, why, sizeof why)) return snprintf(a->tmask_note, sizeof a->tmask_note, "%s", why), 0;
    app_track_masks_refresh(a);
    for (int i = 0; i < a->tmasks.count; ++i)
        if (strcmp(a->tmasks.row[i].name, name) == 0) app_track_mask_select(a, i);
    snprintf(a->status, sizeof a->status, "masks: %s made again from its PSD", name);
    return 1;
}

/* Publish to the game (app.h): the preview is the light files and the masks' PNGs of the open original track against data/; the copy writes the changed and new ones */
int app_original_publish_ask(App *a)
{
    OriginalPublish *plan = &a->publish_plan;
    char why[OT_WHY] = "", stem[16];
    if (!app_original_can_publish(a)) return snprintf(a->status, sizeof a->status, "publish: Studio has no game data folder (--port-data)"), 0;
    if (app_modified(a)) return snprintf(a->status, sizeof a->status, "publish: Save the light file first"), 0;
    if (!original_track_stem_of(a->original_dir, stem, sizeof stem)) return 0;
    app_track_masks_load(a);   // the PNGs are made from the PSDs before they are compared
    if (!original_track_publish_plan(a->original_dir, a->data_dir, plan, why, sizeof why) || !track_masks_publish_plan(a->original_dir, a->data_dir, stem, plan, why, sizeof why))
        return snprintf(a->status, sizeof a->status, "publish: %s", why), 0;
    original_track_publish_text(plan, stem, a->publish_text, sizeof a->publish_text);
    a->publish_dialog = 1;
    return 1;
}

int app_original_publish(App *a)
{
    char why[OT_WHY] = "";
    int written = 0;
    a->publish_dialog = 0;
    if (!app_original_can_publish(a)) return 0;
    if (!original_track_publish(a->original_dir, a->data_dir, &written, why, sizeof why) || !track_masks_publish(a->original_dir, a->data_dir, &a->publish_plan, &written, why, sizeof why))
        return snprintf(a->status, sizeof a->status, "publish: %s", why), 0;
    snprintf(a->status, sizeof a->status, "published to the game: %d file%s written into %s", written, written == 1 ? "" : "s", a->data_dir);
    return 1;
}
