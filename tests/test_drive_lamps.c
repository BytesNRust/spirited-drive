/* The lamp texels on the real P911 CFM (drive_lamps.h): found from a synthetic mask, the toggle reaches the very bytes the loader's bitmap
 * entries (chunk, SHPI, entry payload) read, a low light points them at the shared entries and the day puts the file back byte for byte, a
 * texture that is not a body texture is read too, and a car whose texture has no mask has none. Needs the CD's CARFAMS: $TNFS_DATA or original/
 * of the source tree; skipped when it is not there. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/png.h"
#include "game/race/car_mask.h"
#include "game/viewer/drive_lamps.h"
#include "test.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the masks: the P911's rsid (a body texture) and bott (not one), synthetic, under the path TNFS_CAR_MASKS names */
static struct {
    char name[8];
    uint8_t *png;
    size_t len;
} masks[2];

uint8_t *platform_read_file(const char *path, size_t *size)
{
    for (int i = 0; i < 2; i++) {
        char want[64];
        snprintf(want, sizeof want, "/m/P911/%s_mask.png", masks[i].name);
        if (masks[i].png != NULL && strcmp(path, want) == 0) {
            uint8_t *copy = malloc(masks[i].len);
            memcpy(copy, masks[i].png, masks[i].len);
            *size = masks[i].len;
            return copy;
        }
    }
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }
uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    (void)name;
    *size = 0;
    return NULL;
}
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

/* the texture `name` of the high detail bank, found as the loader finds it */
static int texture(uint8_t *cfm, size_t size, const char *name, FshEntry *out)
{
    Chunk file, shpi;
    Fsh bank;
    if (chunk_open(&file, cfm, size) != CHUNK_OK || chunk_child(&file, 1, &shpi) != CHUNK_OK) return 0;
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return 0;
    for (uint32_t i = 0; i < bank.count; i++)
        if (fsh_entry(&bank, i, out) == FSH_OK && memcmp(out->name, name, 4) == 0) return 1;
    return 0;
}

static void make_mask(int which, const char *name, const FshEntry *e, int class_at_ten)
{
    const int w = e->width, h = e->height;
    uint8_t *mask = malloc((size_t)w * h);
    memset(mask, CAR_MASK_MATTE, (size_t)w * h);
    for (int i = 10; i < 20 && i < w * h; i++) mask[i] = (uint8_t)class_at_ten;   /* ten texels in the first row */
    snprintf(masks[which].name, sizeof masks[which].name, "%s", name);
    CHECK(car_mask_write(mask, w, h, &masks[which].png, &masks[which].len));
    free(mask);
}

static void run(const char *root)
{
    char path[512];
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/P911.CFM", root);
    size_t size = 0;
    uint8_t *cfm = slurp(path, &size);
    if (cfm == NULL) return;
    uint8_t *original = malloc(size);
    memcpy(original, cfm, size);
    FshEntry rsid, bott;
    CHECK(texture(cfm, size, "rsid", &rsid));
    CHECK(texture(cfm, size, "bott", &bott));
    make_mask(0, "rsid", &rsid, CAR_MASK_BRAKE);
    make_mask(1, "bott", &bott, CAR_MASK_REVERSE);
    setenv("TNFS_CAR_MASKS", "/m", 1);
    setenv("TNFS_EMISSIVE_LAMPS", "1", 1);

    drive_lamps_clear(0);
    CHECK_EQ_INT(drive_lamps_find(0, "P911", cfm, size), 20);   /* ten in rsid, ten in bott (not a body texture) */
    CHECK(drive_lamps_has(0, LAMP_BRAKE));
    CHECK(drive_lamps_has(0, LAMP_REVERSE));
    CHECK(!drive_lamps_has(0, LAMP_HEADLIGHT));
    CHECK(!drive_lamps_has(0, LAMP_POSITION));
    CHECK_MEM(cfm, original, size);   /* finding changes nothing */

    drive_lamps_arm();
    drive_lamps_set_entries(0xa1, 0xa2, 0xa3, 0xa4);   /* lit brake, position, headlight, reversing */
    const uint8_t *rsid_texels = rsid.payload, *bott_texels = bott.payload;
    CHECK_EQ_INT(drive_lamps_brake_lit(0), 0);   /* nothing drawn yet */
    drive_lamps_frame(0, 1, 0, 0);   /* low light, the brake released: the brake texels show the position entry */
    CHECK_EQ_INT(drive_lamps_brake_lit(0), 0);
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(rsid_texels[i], 0xa2);
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(bott_texels[i], original[(size_t)(bott.payload - cfm) + i]);   /* the reversing light is not lit by the dark */
    CHECK_EQ_INT(rsid_texels[9], original[(size_t)(rsid.payload - cfm) + 9]);   /* the next texels are left */
    CHECK_EQ_INT(rsid_texels[20], original[(size_t)(rsid.payload - cfm) + 20]);
    drive_lamps_frame(0, 1, 1, 0);   /* braking: the lit entry */
    CHECK_EQ_INT(drive_lamps_brake_lit(0), 0xa1);   /* the flare's lamp: the entry the car's brake texels show */
    CHECK_EQ_INT(drive_lamps_lit_entry(0, LAMP_BRAKE), 0xa1);
    CHECK_EQ_INT(drive_lamps_lit_entry(0, LAMP_REVERSE), 0);   /* in drive: the reversing light is out */
    CHECK_EQ_INT(drive_lamps_lit_entry(0, LAMP_HEADLIGHT), 0);   /* and this car has no headlight texels */
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(rsid_texels[i], 0xa1);
    drive_lamps_frame(0, 0, 1, 0);   /* day, even braking: byte for byte what the file was */
    CHECK_EQ_INT(drive_lamps_brake_lit(0), 0);      /* and no lit lamp */
    CHECK_MEM(cfm, original, size);
    drive_lamps_frame(0, 0, 0, 1);   /* day, in reverse gear: the reversing light is lit, the brake's texels are not */
    CHECK_EQ_INT(drive_lamps_lit_entry(0, LAMP_REVERSE), 0xa4);   /* its halo's lamp, in any light */
    CHECK_EQ_INT(drive_lamps_lit_entry(0, LAMP_BRAKE), 0);
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(bott_texels[i], 0xa4);
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(rsid_texels[i], original[(size_t)(rsid.payload - cfm) + i]);
    drive_lamps_frame(0, 1, 1, 1);   /* low light, braking, in reverse: all lit */
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(bott_texels[i], 0xa4);
    for (int i = 10; i < 20; i++) CHECK_EQ_INT(rsid_texels[i], 0xa1);
    drive_lamps_frame(0, 0, 0, 0);   /* out of reverse in daylight: byte for byte what the file was */
    CHECK_MEM(cfm, original, size);

    /* the light at the car: a shade of 0.94 by day does not light them, a tunnel's 0.5 does, they hold until the light has been back for LAMP_SHADE_HOLD frames,
     * and a new arming (a new race) starts them out */
    {
        const float sun[3] = { 1, 1, 1 }, shade[3] = { 0.94f, 0.94f, 0.94f }, tunnel[3] = { 0.5f, 0.5f, 0.5f }, back[3] = { 0.9f, 0.9f, 0.9f };
        drive_lamps_arm();
        drive_lamps_light(0, 0, sun, shade, 0, 0);
        CHECK_MEM(cfm, original, size);
        drive_lamps_light(0, 0, sun, tunnel, 0, 0);
        for (int i = 10; i < 20; i++) CHECK_EQ_INT(rsid_texels[i], 0xa2);
        for (int f = 0; f < LAMP_SHADE_HOLD - 1; f++) drive_lamps_light(0, 0, sun, back, 0, 0);
        for (int i = 10; i < 20; i++) CHECK_EQ_INT(rsid_texels[i], 0xa2);   /* still lit */
        drive_lamps_light(0, 0, sun, back, 0, 0);
        CHECK_MEM(cfm, original, size);                                     /* out */
        drive_lamps_light(0, 0, sun, tunnel, 0, 0);
        drive_lamps_frame(0, 0, 0, 0);
        drive_lamps_arm();                                                  /* a new race: the day's texels, the state back to out */
        drive_lamps_light(0, 0, sun, back, 0, 0);
        CHECK_MEM(cfm, original, size);
    }

    drive_lamps_clear(0);
    CHECK_EQ_INT(drive_lamps_find(0, "TSupra", cfm, size), 0);   /* no mask for that car: no lamps, nothing written */
    drive_lamps_arm();
    drive_lamps_frame(0, 1, 1, 0);
    CHECK_MEM(cfm, original, size);

    unsetenv("TNFS_EMISSIVE_LAMPS");
    drive_lamps_clear(0);
    CHECK_EQ_INT(drive_lamps_find(0, "P911", cfm, size), 0);    /* the flag off: nothing */
    drive_lamps_clear(0);
    for (int i = 0; i < 2; i++) png_free(masks[i].png), masks[i].png = NULL;
    free(original);
    free(cfm);
}

int main(void)
{
    const char *root = getenv("TNFS_DATA");
    char fallback[512];
#ifdef TNFS_SOURCE_DIR
    snprintf(fallback, sizeof fallback, "%s/original", TNFS_SOURCE_DIR);
#else
    fallback[0] = '\0';
#endif
    if (root == NULL || root[0] == '\0') root = fallback;
    char probe[512];
    snprintf(probe, sizeof probe, "%s/SIMDATA/CARFAMS/P911.CFM", root);
    FILE *f = fopen(probe, "rb");
    if (f == NULL) {
        fprintf(stderr, "test_drive_lamps: no CARFAMS under %s, skipped\n", root);
        return 0;
    }
    fclose(f);
    run(root);
    return TEST_REPORT();
}
