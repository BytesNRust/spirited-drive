/* port (dep59 step 4b, T7): the CONTENT screen's glue (content_screen.h's ContentOps), "Manage content..." in Game Mode. It draws in
 * Customize Game's family: the clean configuration background, darkened toward the mock-up's brown, the game's text and the original
 * titles' letters over it; the editions are editions/'s files (edition_file.h, edition_store.h), the mods mods/'s (mod_store.h), and each
 * failure is told to the player in the game's banner with the store's reason. */
#include "game/frontend/menu_glue_private.h"
#include "game/frontend/content_backdrop.h"
#include "game/frontend/content_preview.h"
#include "game/frontend/content_screen.h"
#include "formats/fsh.h"
#include "formats/png.h"
#include "formats/stock_content.h"
#include "game/sys/edition_file.h"
#include "game/sys/edition_store.h"
#include "game/sys/mod_set.h"
#include "game/sys/mod_store.h"
#include "game/sys/palette.h"
#include "game/sys/video.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/edition_name_box.h"

#include <string.h>

#include "game/frontend/content_dialog_glue.h"   /* T6 (dev2): the name box and the question after an install */


enum {
    W = CONFIG_BGND_W, H = CONFIG_BGND_H, PIXELS = W * H,
    BANNER_TIME = 0x12c,       /* game_mode.c's TOLD: how long a banner stays */
    WHY_CAP = 160, PATH_CAP = 1024, TEXT_CAP = 160,
    MODS_MAX = 128,            /* the most mods of each kind listed (installed, removed) */
    EDITIONS_MAX = 64,         /* mode_choices.h's MODE_CHOICES_EDITIONS: the editions Game Mode lists */
    NAME_CAP = EDITION_NAME_MAX + 1,
    /* the body darkened 30% toward #120E09 (the mock-up's brown), in the 8-bit palette the clean background carries */
    DARK_RGB = 0x120e09, DARK_NUM = 3, DARK_DEN = 10,
    /* the card's panel darkened a further 60% toward the gallery's panel tint (gallery_layout.h's GL_PANEL_RGB), edged in the rules' colour,
     * so the card's dim words read over the background's light "&" */
    PANEL_RGB = 0x0a0704, PANEL_NUM = 3, PANEL_DEN = 5,
    CREAM_RGB = 0xe8d3b7, YELLOW_RGB = 0xffce06, DIM_RGB = 0x8c8273,   /* text.h's TEXT_CREAM and TEXT_YELLOW */
    TRI_H = 6, TRI_W = 2 * TRI_H - 1               /* the up and down triangles */
};

/* the clean background as composed, then the canvas the preview is laid on before it is cut into a shape; its palette; the darkened
 * picture as a shape, made again when the mods in effect changed (mod_set.h), as central_glue_plus.c's own copy is */
static uint8_t scratch[PIXELS];
static uint8_t bg_palette[768];
static uint8_t dark_shape[CONFIG_BGND_SHAPE_HEADER + PIXELS];
static int dark_ok;
static int fade_pending;     /* the screen just opened: its first page is faded in, in the background's palette */
static uint32_t dark_made;   /* the mods' generation it was made under; 0 never */

/* The menus' boxes (Yes / No, the banner) draw over ea_screen()'s picture and put it on the display whole. CONTENT draws on the display itself,
 * so that picture still held Game Mode's last screen, shown behind the box in CONTENT's palette (noise). For a box's run the picture is the
 * display as it is, then the screen's own again (as content_dialog_glue's name field does). */
typedef struct {
    uint8_t *picture, *own;
    size_t size;
} Borrowed;

static Borrowed borrow_picture(void)
{
    Borrowed b = { NULL, NULL, 0 };
    int32_t pw = 0, ph = 0;
    int w = 0, h = 0;
    uint8_t *const picture = ea_picture_pixels(ea_screen(), &pw, &ph);
    const uint8_t *const display = video_target(&w, &h);
    b.size = (size_t)w * (size_t)h;
    if (picture != NULL && display != NULL && pw == w && ph == h && (b.own = malloc(b.size)) != NULL) {
        memcpy(b.own, picture, b.size);
        memcpy(picture, display, b.size);
        /* the screen under the box darkened as under CONTENT's other dialogs (DIALOG_DARKEN, edition_name_box.h): the exe's box shades only its
         * own rectangle, which over a page of text leaves the text readable through it */
        uint8_t rgb[768], table[256];
        video_get_palette(rgb);
        ea_shade_table(0x10000 - 0x10000 * DIALOG_DARKEN / 100, rgb, table);
        palette_remap_pixels(picture, table, (uint32_t)b.size);
        b.picture = picture;
    } else {
        port_log("content: a box draws over the screen's own picture\n");
    }
    return b;
}

static void give_back_picture(Borrowed *b)
{
    if (b->own != NULL) memcpy(b->picture, b->own, b->size);
    free(b->own);
}

static void told(void *ctx, const char *text)
{
    Borrowed b = borrow_picture();
    menu_glue_banner_wait(ctx, text, BANNER_TIME);
    give_back_picture(&b);
}

static int32_t asked(void *ctx, const char *text)
{
    Borrowed b = borrow_picture();
    const int32_t yes = menu_glue_yes_no(ctx, text);
    give_back_picture(&b);
    return yes;
}

/* the palette entry nearest 0xRRGGBB in the background's own palette (content_backdrop.h: not palette_usable()'s set) */
static int32_t nearest(uint32_t rgb) { return content_backdrop_nearest(bg_palette, rgb); }

/* the darkened copy, made once for the mods in effect: the clean background composed without the bottom corners' DONE and CANCEL (the
 * foot has its own buttons there; EXTRAS' copy keeps them), its pixels remapped, its palette left as it is so the text styles built from
 * it stay right */
static void dark_follow(void)
{
    if (dark_made == mod_set_generation()) return;
    dark_made = mod_set_generation();
    dark_ok = config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 0, scratch, bg_palette);
    if (!dark_ok) {
        port_log("content: no clean configuration background, none darkened\n");
        return;
    }
    uint8_t table[256];
    content_backdrop_darken(bg_palette, DARK_RGB, DARK_NUM, DARK_DEN, table);
    palette_remap_pixels(scratch, table, PIXELS);
    content_backdrop_darken(bg_palette, PANEL_RGB, PANEL_NUM, PANEL_DEN, table);
    content_backdrop_panel(scratch, W, H, table, CONTENT_CARD_PANEL_X0, CONTENT_CARD_PANEL_Y0, CONTENT_CARD_PANEL_X1, CONTENT_CARD_PANEL_Y1,
                           (uint8_t)nearest(CONTENT_RULE_RGB));
    dark_ok = config_bgnd_shape(scratch, 0, 0, W, H, dark_shape) != 0;
}

/* the clean background put on as the EXTRAS screen puts it (its palette and the text styles with it), the darkened copy over it */
static void content_background(void *ctx)
{
    dark_follow();
    ea_select_default();
    glue_own_bgnd_shape(ctx, scratch, bg_palette);
    if (dark_ok) ea_blit_own(dark_shape);
}

static void content_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    glue_plus_text_scaled(ctx, style, text, x, y, num, den);
}
static int32_t content_text_width(void *ctx, const char *text, int32_t num, int32_t den)
{
    return glue_plus_text_width_scaled(ctx, text, num, den);
}

/* a flat rectangle, x0..x1 and y0..y1 inclusive, a row at a time (menu_glue_screens.c's fill_rows) */
static void content_fill(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgb)
{
    (void)ctx;
    const int32_t c = nearest(rgb);
    for (int32_t y = y0; y <= y1; y++) ea_frame(x0, y, x1, y, c);
}

/* left and right: the page indicator's triangles (own_panel_arrow). No helper draws an up or a down one (page_arrows.h cuts the left and
 * right ones alone from the CD), so those are small flat triangles drawn a row at a time in the text's cream, or yellow when lit */
static void content_arrow(void *ctx, int32_t which, int32_t lit, int32_t x, int32_t y)
{
    if (which == CONTENT_ARROW_LEFT || which == CONTENT_ARROW_RIGHT) {
        own_panel_arrow(ctx, which == CONTENT_ARROW_RIGHT, lit, x, y);
        return;
    }
    if (which == CONTENT_ARROW_LEFT_OFF || which == CONTENT_ARROW_RIGHT_OFF) {   /* dim, flat, the size of the page arrows' body */
        const int32_t dim = nearest(DIM_RGB);
        for (int col = 0; col < TRI_H; col++) {
            const int inset = which == CONTENT_ARROW_LEFT_OFF ? col : TRI_H - 1 - col;
            ea_frame(x + col, y + inset, x + col, y + TRI_W - 1 - inset, dim);
        }
        return;
    }
    const int32_t c = nearest(lit ? YELLOW_RGB : CREAM_RGB);
    for (int r = 0; r < TRI_H; r++) {
        const int inset = which == CONTENT_ARROW_UP ? TRI_H - 1 - r : r;
        ea_frame(x + inset, y + r, x + TRI_W - 1 - inset, y + r, c);
    }
}

/* the page on the display, the pointer shown over it. The first one fades in with the background's palette: the screen opens over Game
 * Mode's (faded out by glue_content_screen), and its pixels are the background's indices, which Game Mode's palette shows as noise */
static void content_show(void *ctx)
{
    (void)ctx;
    video_flip();
    if (fade_pending) fade_pending = 0, video_fade_in_quick(0, 0x100, bg_palette);
    pointer_show();
}

/* the pointer's place and buttons, as the dialogs read them (menu_glue_dialogs.c's dialog_poll) */
static int32_t content_pointer(void *ctx, int32_t *x, int32_t *y)
{
    (void)ctx;
    int px, py;
    pointer_sync_from_os();
    pointer_position(&px, &py);
    *x = px, *y = py;
    return pointer_read_buttons();
}

/* a frame's wait with the music fed */
static void content_wait(void *ctx)
{
    (void)ctx;
    menu_audio_music_service();
    video_wait_vblank();
}

/* --- the editions: 0 the standard one, then editions/'s ids in edition_list's order --- */

static char listed[EDITIONS_MAX][EDITION_ID_MAX + 1];

static int32_t content_editions(void *ctx)
{
    (void)ctx;
    return 1 + edition_list(listed, EDITIONS_MAX);
}

/* the id of edition `index`: 1, or 0 for the standard edition and an index past the list */
static int edition_id_at(int32_t index, char out[EDITION_ID_MAX + 1])
{
    const int n = edition_list(listed, EDITIONS_MAX);
    if (index < 1 || index > n) return 0;
    snprintf(out, EDITION_ID_MAX + 1, "%s", listed[index - 1]);
    return 1;
}

/* the index the edition `id` has now; 0 (the standard edition) when it is not listed */
static int32_t edition_index_of(const char *id)
{
    const int n = edition_list(listed, EDITIONS_MAX);
    for (int i = 0; i < n; i++)
        if (strcmp(listed[i], id) == 0) return i + 1;
    return 0;
}

static int32_t content_edition_load(void *ctx, int32_t index, Edition *out)
{
    (void)ctx;
    char id[EDITION_ID_MAX + 1], why[WHY_CAP];
    if (!edition_id_at(index, id)) return 0;
    if (edition_load(id, out, why, sizeof why)) return 1;
    port_log("content: the edition %s cannot be read: %s\n", id, why);
    return 0;
}

/* Game Mode's title for the standard edition (Restomod '26), an own edition's name= (its id when it cannot be read) */
static void content_edition_name(void *ctx, int32_t index, char *out, size_t cap)
{
    char id[EDITION_ID_MAX + 1];
    Edition e;
    if (!edition_id_at(index, id)) snprintf(out, cap, "%s", port_profile_title(PROFILE_RESTOMOD));
    else snprintf(out, cap, "%s", content_edition_load(ctx, index, &e) ? e.name : id);
}

static int32_t content_edition_save(void *ctx, int32_t index, const Edition *e)
{
    (void)ctx;
    char id[EDITION_ID_MAX + 1], why[WHY_CAP];
    if (!edition_id_at(index, id)) return 0;
    if (edition_store_save(id, e, why, sizeof why)) return 1;
    port_log("content: the edition %s was not saved: %s\n", id, why);
    return 0;
}

/* --- the mods --- */

static ModStoreEntry entries[MODS_MAX];   /* mod_store's answer, copied out at once */
static ModInfo installed_info[MODS_MAX], removed_info[MODS_MAX];
static int installed_backups[MODS_MAX];

/* the installed mods and the removed ones with a backup, into arrays kept until the next call */
static void content_mods(void *ctx, ContentMods *out)
{
    (void)ctx;
    const int installed = mod_store_list(entries, MODS_MAX);
    for (int i = 0; i < installed; i++) installed_info[i] = entries[i].info, installed_backups[i] = entries[i].backups > 0;
    const int removed = mod_store_removed(entries, MODS_MAX);
    for (int i = 0; i < removed; i++) removed_info[i] = entries[i].info;
    *out = (ContentMods){ installed_info, installed_backups, installed, removed_info, removed };
}

static int32_t content_used_in(void *ctx, const char *id)
{
    (void)ctx;
    return mod_store_used_in(id);
}

/* --- installing: the foot's Install..., the card's Update... and a file dropped on the window, one way --- */

/* the mod's id on the edition's list for its kind (MOD_TYPE_*); 1 when it is there after, 0 when the list is full */
static int edition_add_piece(Edition *e, const ModInfo *info)
{
    char(*list)[MOD_INI_ID_MAX + 1] = e->tweak;
    int *count = &e->tweaks, cap = EDITION_TWEAKS_MAX;
    if (info->type == MOD_TYPE_CAR) list = e->car, count = &e->cars, cap = EDITION_LIST_MAX;
    else if (info->type == MOD_TYPE_TRACK) list = e->track, count = &e->tracks, cap = EDITION_TRACKS_MAX;
    for (int i = 0; i < *count; i++)
        if (strcmp(list[i], info->id) == 0) return 1;
    if (*count >= cap) return 0;
    snprintf(list[(*count)++], MOD_INI_ID_MAX + 1, "%s", info->id);
    return 1;
}

/* the question after a new mod is installed while an own edition is shown: on yes, the mod added to it and the edition saved */
static void offer_to_edition(void *ctx, const ModInfo *info, int32_t index)
{
    char id[EDITION_ID_MAX + 1], why[WHY_CAP];
    Edition e;
    if (!edition_id_at(index, id) || !edition_load(id, &e, why, sizeof why)) return;
    if (!glue_content_install_prompt(info->name, info->version, e.name)) return;
    if (!edition_add_piece(&e, info)) told(ctx, "The edition has no room for it");
    else if (!edition_store_save(id, &e, why, sizeof why)) told(ctx, why);
}

/* the zip at `path` installed (or an installed mod updated); a new mod offered to the own edition `edition` shows (NULL or the standard
 * edition: none), else the player told what was installed. 1 when installed */
static int32_t install_zip(void *ctx, const char *path, const int32_t *edition)
{
    ModInfo info;
    char old[MOD_INI_VERSION_MAX + 1] = "", why[WHY_CAP] = "", text[TEXT_CAP];
    if (!mod_store_install(path, &info, old, sizeof old, why, sizeof why)) {
        port_log("content: %s was not installed: %s\n", path, why);
        told(ctx, why);
        return 0;
    }
    if (old[0] == '\0' && edition != NULL && *edition > 0) {
        offer_to_edition(ctx, &info, *edition);   /* its question says what was installed */
        return 1;
    }
    if (old[0] == '\0') {   /* the standard edition shown: the same dialog, OK alone */
        (void)glue_content_install_prompt(info.name, info.version, NULL);
        return 1;
    }
    snprintf(text, sizeof text, "Updated %s %s to %s", info.name, old, info.version);
    told(ctx, text);
    return 1;
}

/* the system's open dialog for a zip, then install_zip; 0 when none was picked */
static int32_t install_picked(void *ctx, const int32_t *edition)
{
    char path[PATH_CAP];
    if (!platform_pick_zip(path, sizeof path)) return 0;
    return install_zip(ctx, path, edition);
}

static int32_t content_dropped(void *ctx, int32_t *edition)
{
    char path[PATH_CAP];
    if (!platform_dropped_file(path, sizeof path)) return 0;
    return install_zip(ctx, path, edition);
}

/* --- the card's buttons --- */

static int32_t uninstall(void *ctx, const char *id)
{
    char text[TEXT_CAP], why[WHY_CAP];
    snprintf(text, sizeof text, "Uninstall %s?", id);
    if (!asked(ctx, text)) return 0;
    if (mod_store_uninstall(id, why, sizeof why)) return 1;
    told(ctx, why);
    return 0;
}

static int32_t restore(void *ctx, const char *id)
{
    char why[WHY_CAP];
    if (mod_store_restore(id, why, sizeof why)) return 1;
    told(ctx, why);
    return 0;
}

/* the card has no edition: an update made there is offered to none (an updated mod is in the editions it was in) */
static int32_t content_card(void *ctx, int32_t button, int32_t tab, const char *id)
{
    (void)tab;
    switch (button) {
    case CONTENT_CARD_UPDATE: return install_picked(ctx, NULL);
    case CONTENT_CARD_UNINSTALL: return uninstall(ctx, id);
    case CONTENT_CARD_PREVIOUS: return restore(ctx, id);
    default: return 0;
    }
}

/* --- the foot's buttons --- */

/* New, and Duplicate on the standard edition: a new edition with the standard set (edition_store_create's), shown after */
static int32_t edition_new(void *ctx, int32_t *edition, const char *title)
{
    char name[NAME_CAP] = "", id[EDITION_ID_MAX + 1], why[WHY_CAP];
    if (!glue_edition_name_box(title, name)) return 0;
    if (!edition_store_create(name, id, why, sizeof why)) {
        told(ctx, why);
        return 0;
    }
    *edition = edition_index_of(id);
    return 1;
}

static int32_t edition_duplicate(void *ctx, int32_t *edition)
{
    char from[EDITION_ID_MAX + 1], name[NAME_CAP] = "", id[EDITION_ID_MAX + 1], why[WHY_CAP];
    if (!edition_id_at(*edition, from)) return edition_new(ctx, edition, "Duplicate edition");
    if (!glue_edition_name_box("Duplicate edition", name)) return 0;
    if (!edition_store_duplicate(from, name, id, why, sizeof why)) {
        told(ctx, why);
        return 0;
    }
    *edition = edition_index_of(id);
    return 1;
}

static int32_t edition_rename(void *ctx, int32_t index)
{
    char id[EDITION_ID_MAX + 1], name[NAME_CAP], why[WHY_CAP];
    if (!edition_id_at(index, id)) return 0;
    content_edition_name(ctx, index, name, sizeof name);
    if (!glue_edition_name_box("Rename edition", name)) return 0;
    if (edition_store_rename(id, name, why, sizeof why)) return 1;
    told(ctx, why);
    return 0;
}

/* the edition shown removed, once the player said yes; the standard edition shown after */
static int32_t edition_delete(void *ctx, int32_t *edition)
{
    char id[EDITION_ID_MAX + 1], name[NAME_CAP], text[TEXT_CAP], why[WHY_CAP];
    if (!edition_id_at(*edition, id)) return 0;
    content_edition_name(ctx, *edition, name, sizeof name);
    snprintf(text, sizeof text, "Delete edition %s?", name);
    if (!asked(ctx, text)) return 0;
    if (!edition_store_delete(id, why, sizeof why)) {
        told(ctx, why);
        return 0;
    }
    *edition = 0;
    return 1;
}

static int32_t content_foot(void *ctx, int32_t button, int32_t *edition)
{
    switch (button) {
    case CONTENT_FOOT_NEW: return edition_new(ctx, edition, "New edition");
    case CONTENT_FOOT_DUPLICATE: return edition_duplicate(ctx, edition);
    case CONTENT_FOOT_RENAME: return edition_rename(ctx, *edition);
    case CONTENT_FOOT_DELETE: return edition_delete(ctx, edition);
    case CONTENT_FOOT_INSTALL: return install_picked(ctx, edition);
    default: return 0;
    }
}

static void content_refused(void *ctx, int32_t lacks)
{
    const int car = (lacks & CONTENT_RULES_NO_CAR) != 0, track = (lacks & CONTENT_RULES_NO_TRACK) != 0;
    told(ctx, car && track ? "An edition needs a car and a track" : car ? "An edition needs a car" : "An edition needs a track");
}

/* --- the preview --- */

/* Control Central's card art on the CD (central_draw.c's CAR_NAMES and TRACK_NAMES, by the stock pieces' numbers; 8..10 are no car;
 * Lunar Springs, 10, shows the moon's card as the hidden track did) */
static const char *const CAR_CARDS[12] = { "rx71", "nsx1", "sup1", "9111", "vet1", "vip1", "5121", "dia1", "war1", "war1", "war1", "war1" };
static const char *const TRACK_CARDS[STOCK_TRACK_COUNT] = { "city", "coast", "alpine", "springs", "avalley", "bsienna", "vridge",
                                                            "tpolis", "lvegas", "moon", "moon" };

/* Control Central's palette (central.qfs's !pal, which its cards are drawn in), read once for the mods in effect; NULL when none */
static const uint8_t *central_palette(void)
{
    static const char PAL[4] = { '!', 'p', 'a', 'l' };
    static uint8_t pal[768];
    static int ok;
    static uint32_t made;   /* the mods' generation it was read under; 0 never */
    if (made != mod_set_generation()) {
        made = mod_set_generation();
        size_t size = 0;
        uint8_t *const data = resource_load_art("control/central.qfs", &size);
        Fsh f;
        FshEntry e;
        uint32_t at;
        ok = data != NULL && fsh_open(&f, data, size) == FSH_OK && fsh_find(&f, PAL, &at) == FSH_OK && fsh_entry(&f, at, &e) == FSH_OK &&
             fsh_palette_rgb(&e, pal) == FSH_OK;
        if (!ok) port_log("content: no palette in control/central.qfs\n");
        resource_free(data);
    }
    return ok ? pal : NULL;
}

/* the card `card`'s picture ("0000" of control/<card>.qfs) as RGB, malloc'd; NULL when the CD has none */
static uint8_t *cd_card_rgb(const char *card, int *w, int *h)
{
    static const char TAG[4] = { '0', '0', '0', '0' };
    const uint8_t *const pal = central_palette();
    char name[32];
    snprintf(name, sizeof name, "control/%s.qfs", card);
    size_t size = 0;
    uint8_t *const data = pal != NULL ? resource_load_art(name, &size) : NULL, *rgb = NULL;
    Fsh f;
    FshEntry e;
    uint32_t at;
    if (data != NULL && fsh_open(&f, data, size) == FSH_OK && fsh_find(&f, TAG, &at) == FSH_OK && fsh_entry(&f, at, &e) == FSH_OK &&
        e.type == FSH_TYPE_BITMAP && e.width > 0 && e.height > 0 && e.payload_size >= (size_t)e.width * e.height)
        rgb = malloc((size_t)e.width * e.height * 3);
    if (rgb != NULL) {
        for (size_t i = 0; i < (size_t)e.width * e.height; i++) memcpy(rgb + 3 * i, pal + 3 * e.payload[i], 3);
        *w = e.width, *h = e.height;
    } else if (pal != NULL) {
        port_log("content: no card picture in %s\n", name);
    }
    resource_free(data);
    return rgb;
}

/* an added car's card.png (added_card_glue.c reads the same file) as RGB, malloc'd; NULL when it has none the game can read */
static uint8_t *added_card_rgb(const char *id, int *w, int *h)
{
    char rel[MOD_INI_ID_MAX + 32];
    if (!mod_ini_id_ok(id)) return NULL;
    snprintf(rel, sizeof rel, "mods/%s/card.png", id);
    size_t size = 0;
    uint8_t *const data = platform_user_read_file_at(rel, &size);
    if (data == NULL) return NULL;
    PngImage img;
    const int read = png_read(data, size, &img) == PNG_OK;
    platform_free_file(data);
    if (!read) {
        port_log("content: %s: card.png is not a picture the game can read\n", id);
        return NULL;
    }
    const size_t n = (size_t)img.width * img.height;
    uint8_t *const rgb = malloc(n * 3);
    for (size_t i = 0; rgb != NULL && i < n; i++)
        memcpy(rgb + 3 * i, img.indexed ? img.palette + 3 * img.pixels[i] : img.pixels + 4 * i, 3);
    if (rgb != NULL) *w = (int)img.width, *h = (int)img.height;
    png_free(img.pixels);
    return rgb;
}

/* the picture of piece `id` of `tab`, malloc'd RGB; NULL when it has none. An added track and a tweak have no picture the game can read
 * yet: their box stays empty */
static uint8_t *preview_rgb(int32_t tab, const char *id, int *w, int *h)
{
    if (tab == CONTENT_CARS) {
        const int car = stock_car_of_id(id);
        return car >= 0 && car < 12 ? cd_card_rgb(CAR_CARDS[car], w, h) : added_card_rgb(id, w, h);
    }
    const int track = tab == CONTENT_TRACKS ? stock_track_of_id(id) : -1;
    return track >= 0 && track < STOCK_TRACK_COUNT ? cd_card_rgb(TRACK_CARDS[track], w, h) : NULL;
}

/* `rgb` (w x h) reduced by area to fit bw x bh, its proportions kept and never enlarged, in the background's palette into `scratch` at
 * the box's middle, then blitted; (x0, y0) the box's top left */
static void preview_draw(const uint8_t *rgb, int w, int h, int32_t x0, int32_t y0, int bw, int bh)
{
    int ow, oh;
    if ((long)w * bh > (long)h * bw) ow = w < bw ? w : bw, oh = (int)((long)h * ow / w);
    else oh = h < bh ? h : bh, ow = (int)((long)w * oh / h);
    if (ow < 1 || oh < 1) return;
    uint8_t *const small = malloc((size_t)ow * oh * 3), *const shape_out = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)ow * oh);
    if (small != NULL && shape_out != NULL) {
        content_reduce_area(rgb, w, h, small, ow, oh);
        const int left = x0 + (bw - ow) / 2, top = y0 + (bh - oh) / 2;
        uint8_t usable[256];
        content_backdrop_usable(usable);
        for (int y = 0; y < oh; y++)
            for (int x = 0; x < ow; x++) {
                const uint8_t *const p = small + 3 * ((size_t)y * ow + x);
                scratch[(size_t)(top + y) * W + left + x] = (uint8_t)palette_nearest(bg_palette, usable, p[0], p[1], p[2]);
            }
        if (config_bgnd_shape(scratch, left, top, ow, oh, shape_out) != 0) ea_blit_own(shape_out);
    }
    free(small);
    free(shape_out);
}

static void content_preview(void *ctx, int32_t tab, const char *id, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    (void)ctx;
    if (x0 < 0 || y0 < 0 || x1 >= W || y1 >= H || x1 < x0 || y1 < y0) return;
    int w = 0, h = 0;
    uint8_t *const rgb = preview_rgb(tab, id, &w, &h);
    if (rgb != NULL) preview_draw(rgb, w, h, x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    free(rgb);
}

/* port (dep59 step 4b, T7): the CONTENT screen on the edition `edition` (0 the standard one, then editions/'s), and the one it closed on,
 * into `edition` and returned */
int32_t glue_content_screen(int32_t *edition)
{
    const ContentOps ops = { content_background, content_text, content_text_width, own_title_text, content_fill, content_arrow,
                             content_preview, content_show, key, content_pointer, content_wait, content_editions,
                             content_edition_name, content_edition_load, content_edition_save, content_mods, content_used_in,
                             content_card, content_foot, content_refused, NULL, content_dropped };
    const int music = *menu_audio_silent() == 0;
    if (music) {   /* the car gallery's (calmer) music, as Control Central's gallery starts it (central_select.c) */
        menu_audio_music_fade_out();
        video_fade_out_quick(0, 0x100);
        menu_audio_music_end();
        menu_audio_music_start("showcase.asf");
    } else {
        video_fade_out_quick(0, 0x100);   /* Game Mode's page out; it fades its own back in after (game_mode.c's manage) */
    }
    fade_pending = 1;
    content_screen_run(&ops, edition);
    if (music) menu_audio_music_fade_out();   /* a stop waits for the voice's fade (as on the way in) */
    video_fade_out_quick(0, 0x100);
    if (music) {
        menu_audio_music_end();
        menu_audio_music_pick();   /* the menus' song again */
    }
    return *edition;
}
