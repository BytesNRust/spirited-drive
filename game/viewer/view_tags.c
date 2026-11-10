/* port (the cars on the road): the name tags over the cars, taken out of view_cars.c: the colours and outline the menu sets, and one tag's draw. */

#include "game/gfx/flat.h"
#include "game/race/car_tag.h"
#include "viewer/view_shared.h"

/* 0050b7d4 / 0050b7b4 */
static const uint8_t *tag_colours;
static int tag_colour_count;
static uint32_t tag_outline;

void ported_view_set_tags(const uint8_t *colours, int count, uint32_t outline)
{
    tag_colours = colours, tag_colour_count = count, tag_outline = outline;
}

static void tag_polygon(void *ctx, uint32_t colour, int count, const int32_t *points)
{
    flat_polygon(ctx, colour, count, points);
}

static void tag_line(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour)
{
    flat_line(ctx, x0, y0, x1, y1, colour);
}

static uint8_t tags_hidden;  /* 004fb78b */

void ported_view_set_tags_hidden(int hidden)
{
    tags_hidden = (uint8_t)hidden;
}

void view_tag_draw(const QuadTarget *target, int32_t x, int32_t y, int32_t slot, int32_t z)
{
    if (tag_colours == NULL || slot < 0 || slot >= tag_colour_count) return;
    const CarTagOps ops = { (void *)target, tag_polygon, tag_line };
    const CarTagView v = { tag_colours, tag_outline, SKID_HIGH_RES, shadow_race->humans, tags_hidden };
    car_tag(&ops, &v, x, y, slot, z);
}
