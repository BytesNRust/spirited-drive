#include "game/viewer/reflect_pass.h"

#include "game/race/headlight_pool.h"
#include "game/race/reflector.h"
#include "game/race/track_art.h"

#include <stdlib.h>
#include <string.h>

enum { ENTRIES_MAX = 96, TRANSPARENT_INDEX = 0xff, ROWS_MAX = 16 };

typedef struct {
    const uint8_t *key;   /* the bitmap's payload: the texture is found by it, whichever FshEntry struct is drawn */
    FshEntry copy;
    uint8_t *mask, *pixels;
    size_t count;
    int fresh;            /* the copy's pixels are of the palette now */
} Entry;

static Entry entries[ENTRIES_MAX];
static int entry_count;
static int lamps_on, dark_now;
static double lamp_x, lamp_z, head_x, head_z;
static uint8_t made_palette[768];
static int palette_made;
static uint8_t tables[REFLECTOR_LEVELS][256];
static uint8_t row_tables[REFLECTOR_LEVELS][ROWS_MAX][256];   /* the same, a row for each darkness step of the road's per-pixel path */
static int tables_fresh;
static uint8_t ramp[REFLECTOR_FAMILIES][4];   /* each family's entries, dimmest first */
static int ramp_len[REFLECTOR_FAMILIES];

void reflect_pass_reset(void)
{
    for (int i = 0; i < entry_count; i++) free(entries[i].mask), free(entries[i].pixels);
    memset(entries, 0, sizeof entries);
    entry_count = 0, lamps_on = 0, palette_made = 0, tables_fresh = 0;
}

int reflect_pass_count(void) { return entry_count; }

int reflect_pass_add(const FshEntry *bitmap, const uint8_t *mask, int mw, int mh)
{
    if (bitmap == NULL || bitmap->payload == NULL || mask == NULL || entry_count >= ENTRIES_MAX || bitmap->width == 0 || bitmap->height == 0) return 0;
    const size_t count = (size_t)bitmap->width * bitmap->height;
    if (bitmap->payload_size < count) return 0;
    for (int i = 0; i < entry_count; i++)
        if (entries[i].key == bitmap->payload) return 0;
    Entry e = { .key = bitmap->payload, .copy = *bitmap, .count = count };
    e.mask = malloc(count), e.pixels = malloc(count);
    if (e.mask == NULL || e.pixels == NULL) {
        free(e.mask), free(e.pixels);
        return 0;
    }
    if (mw == bitmap->width && mh == bitmap->height) memcpy(e.mask, mask, count);
    else reflector_mask_scale(mask, mw, mh, e.mask, bitmap->width, bitmap->height);
    e.copy.payload = e.pixels, e.copy.payload_size = count;
    entries[entry_count++] = e;
    return 1;
}

void reflect_pass_frame(int on, double x, double z, double hx, double hz, int dark, const uint8_t palette[768])
{
    lamps_on = on != 0, dark_now = dark != 0;
    lamp_x = x, lamp_z = z, head_x = hx, head_z = hz;
    if (palette != NULL && (!palette_made || memcmp(made_palette, palette, sizeof made_palette) != 0)) {   /* the bitmaps are remapped in place to the screen palette: remake the copies and tables */
        memcpy(made_palette, palette, sizeof made_palette);
        palette_made = 1, tables_fresh = 0;
        for (int i = 0; i < entry_count; i++) entries[i].fresh = 0;
    }
}

int reflect_pass_active(void) { return entry_count > 0 && lamps_on && dark_now && palette_made && (ramp_len[0] | ramp_len[1] | ramp_len[2]) != 0; }

int reflect_pass_gain(double x, double z, double height)
{
    const double dx = x - lamp_x, dz = z - lamp_z;
    const double forward = dx * head_x + dz * head_z - (CONE_LAMP_FORWARD_CM / 100.0), lateral = dx * head_z - dz * head_x;
    return reflector_gain(forward, lateral, height, reflect_pass_active());
}

const FshEntry *reflect_pass_copy(const FshEntry *t)
{
    if (t == NULL || t->payload == NULL) return NULL;
    for (int i = 0; i < entry_count; i++) {
        Entry *e = &entries[i];
        if (e->key != t->payload) continue;
        if (!e->fresh) {
            for (size_t k = 0; k < e->count; k++) e->pixels[k] = e->mask[k] ? t->payload[k] : TRANSPARENT_INDEX;
            e->copy.type = TRACK_TEXTURE_TRANSPARENT_TYPE;   /* the draw is told so as well */
            e->fresh = 1;
        }
        return &e->copy;
    }
    return NULL;
}

static void add_entry(ReflectorFamily family, uint8_t entry)
{
    if (entry != 0 && ramp_len[family] < 4) ramp[family][ramp_len[family]++] = entry;
}

void reflect_pass_set_entries(const uint8_t street[3], uint8_t window, uint8_t brake_lit, uint8_t position, uint8_t headlight, uint8_t reverse)
{
    uint8_t before[sizeof ramp];
    int before_len[REFLECTOR_FAMILIES];
    memcpy(before, ramp, sizeof ramp), memcpy(before_len, ramp_len, sizeof ramp_len);
    memset(ramp, 0, sizeof ramp), memset(ramp_len, 0, sizeof ramp_len);
    add_entry(REFLECTOR_RED, position), add_entry(REFLECTOR_RED, brake_lit);
    if (street != NULL) add_entry(REFLECTOR_WARM, street[2]), add_entry(REFLECTOR_WARM, street[1]), add_entry(REFLECTOR_WARM, street[0]);
    else add_entry(REFLECTOR_WARM, window);
    add_entry(REFLECTOR_WHITE, headlight), add_entry(REFLECTOR_WHITE, reverse);
    for (int f = 0; f < REFLECTOR_FAMILIES; f++)   /* a family with none borrows the first family that has */
        if (ramp_len[f] == 0)
            for (int o = 0; o < REFLECTOR_FAMILIES && ramp_len[f] == 0; o++)
                if (ramp_len[o] > 0) memcpy(ramp[f], ramp[o], sizeof ramp[f]), ramp_len[f] = ramp_len[o];
    if (memcmp(before, ramp, sizeof ramp) != 0 || memcmp(before_len, ramp_len, sizeof ramp_len) != 0) tables_fresh = 0;
}

const uint8_t *reflect_pass_table(int level)
{
    if (level < 1) level = 1;
    if (level >= REFLECTOR_LEVELS) level = REFLECTOR_LEVELS - 1;
    if (!tables_fresh) {
        for (int l = 1; l < REFLECTOR_LEVELS; l++)
            for (int c = 0; c < 256; c++) {
                const ReflectorFamily f = reflector_family(made_palette + 3 * c);
                tables[l][c] = ramp_len[f] > 0 ? ramp[f][reflector_step(l, ramp_len[f])] : (uint8_t)c;
            }
        tables_fresh = 1;
    }
    return tables[level];
}

const uint8_t (*reflect_pass_rows(int level, int rows))[256]
{
    const uint8_t *t = reflect_pass_table(level);
    if (level < 1) level = 1;
    if (level >= REFLECTOR_LEVELS) level = REFLECTOR_LEVELS - 1;
    for (int r = 0; r < ROWS_MAX && r < (rows > 0 ? rows : 1); r++) memcpy(row_tables[level][r], t, 256);
    return row_tables[level];
}
