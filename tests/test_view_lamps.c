/* The tunnel lamps' module (view_lamps.c) on a synthetic lamp texture: the lamp list and its lookup by bitmap, the tables that keep the
 * lamp's colours, the halo noted from a drawn quad and drawn over the finished pixels, and the same with lamp_flares off. */
#include "game/viewer/view_lamps.h"

#include "game/sys/palette.h"
#include "viewer/ported_view.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { TEX = 16, LAMP_COLOUR = 200, BACK = 10, W = 64, H = 64 };

static const TunnelLampTexture LAMP = { 0, 1, { LAMP_COLOUR }, { 4, 4, 11, 11 }, NULL };   /* the fitting is the texture's middle */

static TrackArt *make_art(uint8_t texels[TEX * TEX])
{
    TrackArt *art = calloc(1, sizeof *art);
    memset(texels, BACK, TEX * TEX);
    for (int y = 4; y <= 11; y++) memset(texels + y * TEX + 4, LAMP_COLOUR, 8);
    TrackTexture *t = &art->slots[0][0];
    t->present = 1;
    t->bitmap.width = TEX, t->bitmap.height = TEX;
    t->bitmap.payload = texels, t->bitmap.payload_size = TEX * TEX;
    for (int c = 0; c < 256; c++) art->palette[3 * c] = art->palette[3 * c + 1] = art->palette[3 * c + 2] = (uint8_t)(c / 2);
    art->palette[3 * LAMP_COLOUR] = 255, art->palette[3 * LAMP_COLOUR + 1] = 255, art->palette[3 * LAMP_COLOUR + 2] = 200;
    return art;
}

/* the quad the lamp texture is drawn as: its middle (the fitting) lands at (18, 18) */
static const int32_t QUAD[8] = { 10, 10, 26, 10, 26, 26, 10, 26 };

static void fill(uint8_t *pixels)
{
    memset(pixels, BACK, W * H);
    for (int y = 16; y <= 20; y++) memset(pixels + y * W + 16, LAMP_COLOUR, 5);   /* the lamp's own colour round the centre */
}

static int differing(const uint8_t *a, const uint8_t *b, int x0, int x1)
{
    int n = 0;
    for (int y = 0; y < H; y++)
        for (int x = x0; x < x1; x++) n += a[y * W + x] != b[y * W + x];
    return n;
}

static void test_lamp_list_and_lookup(void)
{
    uint8_t texels[TEX * TEX];
    TrackArt *art = make_art(texels);
    ported_view_set_tunnel_lamps(&LAMP, 1);
    CHECK_EQ_INT(view_lamps_count(), 1);
    CHECK_EQ_INT(view_lamps_of(&art->slots[0][0].bitmap), -1);   /* the bitmaps are not known until the frame sets them */
    view_lamps_textures(art);
    CHECK_EQ_INT(view_lamps_of(&art->slots[0][0].bitmap), 0);
    CHECK_EQ_INT(view_lamps_of(&art->slots[1][0].bitmap), -1);
    ported_view_set_tunnel_lamps(NULL, 0);
    CHECK_EQ_INT(view_lamps_count(), 0);
    CHECK_EQ_INT(view_lamps_of(&art->slots[0][0].bitmap), -1);   /* no lamp is left to be one */
    free(art);
}

static void test_tables_keep_the_lamps_colours(void)
{
    uint8_t texels[TEX * TEX];
    static uint8_t tunnel[MODERN_FOG_LEVELS][256];
    TrackArt *art = make_art(texels);
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) tunnel[l][c] = (uint8_t)(c / 2);
    ported_view_set_tunnel_lamps(&LAMP, 1);
    view_lamps_make_tables(art, (const uint8_t (*)[256])tunnel);
    CHECK_EQ_INT(view_lamps_of(&art->slots[0][0].bitmap), 0);
    for (int l = 0; l < MODERN_FOG_LEVELS; l++) {
        CHECK_EQ_INT(view_lamps_table(0)[l][LAMP_COLOUR], LAMP_COLOUR);   /* the fitting's colour is not darkened */
        CHECK_EQ_INT(view_lamps_table(0)[l][50], tunnel[l][50]);          /* the rest is the tunnel's */
    }
    ported_view_set_tunnel_lamps(NULL, 0);
    free(art);
}

static void test_a_noted_lamp_gets_a_halo(void)
{
    uint8_t texels[TEX * TEX], usable[256], before[W * H], after[W * H];
    TrackArt *art = make_art(texels);
    memset(usable, 1, sizeof usable);
    palette_usable_set(usable);
    ported_view_set_tunnel_lamps(&LAMP, 1);
    view_lamps_set_flares(1);
    CHECK_EQ_INT(view_lamps_flares(), 1);
    view_lamps_textures(art);
    view_lamps_frame_start();
    view_lamps_note_full(&art->slots[0][0].bitmap, QUAD);
    fill(before), fill(after);
    QuadTarget t = { after, W, 0, 0, W, H };
    view_lamps_draw(&t, art->palette);
    CHECK(differing(before, after, 0, W) > 0);                       /* the halo lights the pixels round the fitting */
    for (int y = 16; y <= 20; y++)
        for (int x = 16; x <= 20; x++) CHECK_EQ_INT(after[y * W + x], LAMP_COLOUR);   /* and leaves the fitting's own colour */
    /* clipped at x 18: nothing to its left changes, the halo is cut there */
    fill(after);
    t.clip_left = 18;
    view_lamps_draw(&t, art->palette);
    CHECK_EQ_INT(differing(before, after, 0, 18), 0);
    CHECK(differing(before, after, 18, W) > 0);
    /* a new frame has noted nothing: no halo */
    view_lamps_frame_start();
    fill(after);
    t.clip_left = 0;
    view_lamps_draw(&t, art->palette);
    CHECK_EQ_INT(differing(before, after, 0, W), 0);
    /* a lamp whose own colour is not at the centre is covered by something nearer: no halo */
    view_lamps_note_full(&art->slots[0][0].bitmap, QUAD);
    memset(after, BACK, sizeof after);
    view_lamps_draw(&t, art->palette);
    for (int i = 0; i < W * H; i++) CHECK_EQ_INT(after[i], BACK);
    ported_view_set_tunnel_lamps(NULL, 0);
    free(art);
}

static void test_a_camera_quad_lamp_gets_a_halo(void)
{
    /* Modern 3D draws the track's polygons from camera space (draw_camera), not from screen corners: the lamp must be noted there too */
    uint8_t texels[TEX * TEX], usable[256], before[W * H], after[W * H];
    TrackArt *art = make_art(texels);
    memset(usable, 1, sizeof usable);
    palette_usable_set(usable);
    ported_view_set_tunnel_lamps(&LAMP, 1);
    view_lamps_set_flares(1);
    view_lamps_textures(art);
    View view = { 0 };
    view.centre_x = 32, view.centre_y = 32, view.focal_x = 32, view.focal_y = -32, view.near_z = 1;
    /* z 32 and focal 32: screen x = 32 + x, y = 32 - y, the corners of QUAD */
    const int32_t c0[3] = { -22, 22, 32 }, c1[3] = { -6, 22, 32 }, c2[3] = { -6, 6, 32 }, c3[3] = { -22, 6, 32 };
    const int32_t *const cam[4] = { c0, c1, c2, c3 };
    const int32_t right = (TEX << 16) - 1, bottom = (TEX << 16) - 1;
    const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
    view_lamps_frame_start();
    view_lamps_note_cam(&art->slots[0][0].bitmap, &view, uv, cam);
    fill(before), fill(after);
    QuadTarget t = { after, W, 0, 0, W, H };
    view_lamps_draw(&t, art->palette);
    CHECK(differing(before, after, 0, W) > 0);
    /* a corner behind the near plane: not noted (its screen place is unknown), no halo */
    const int32_t behind[3] = { -22, 6, 0 };
    const int32_t *const cut[4] = { c0, c1, c2, behind };
    view_lamps_frame_start();
    view_lamps_note_cam(&art->slots[0][0].bitmap, &view, uv, cut);
    fill(after);
    view_lamps_draw(&t, art->palette);
    CHECK_EQ_INT(differing(before, after, 0, W), 0);
    ported_view_set_tunnel_lamps(NULL, 0);
    free(art);
}

static void test_flares_off_notes_and_draws_nothing(void)
{
    uint8_t texels[TEX * TEX], before[W * H], after[W * H];
    TrackArt *art = make_art(texels);
    ported_view_set_tunnel_lamps(&LAMP, 1);
    view_lamps_set_flares(0);
    CHECK_EQ_INT(view_lamps_flares(), 0);
    view_lamps_textures(art);
    view_lamps_frame_start();
    view_lamps_note_full(&art->slots[0][0].bitmap, QUAD);
    fill(before), fill(after);
    QuadTarget t = { after, W, 0, 0, W, H };
    view_lamps_draw(&t, art->palette);
    CHECK_EQ_INT(differing(before, after, 0, W), 0);
    view_lamps_set_flares(1);   /* what was noted with the switch off is not kept: nothing to draw */
    view_lamps_draw(&t, art->palette);
    CHECK_EQ_INT(differing(before, after, 0, W), 0);
    ported_view_set_tunnel_lamps(NULL, 0);
    free(art);
}

int main(void)
{
    test_lamp_list_and_lookup();
    test_tables_keep_the_lamps_colours();
    test_a_noted_lamp_gets_a_halo();
    test_a_camera_quad_lamp_gets_a_halo();
    test_flares_off_notes_and_draws_nothing();
    return TEST_REPORT();
}
