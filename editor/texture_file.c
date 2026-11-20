/* «Show texture file»: a texture's bank PNG in a car kit (texture_file.h). */
#include "texture_file.h"

#include "formats/car_kit_paths.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { ROW_CAP = 1024, COL_NAME_HEX = 1, COL_FILE = 11 };

/* the `col`th tab-separated cell of `row` into `out`; 0 when the row has fewer cells or it does not fit */
static int cell(const char *row, int col, char *out, size_t cap)
{
    for (int i = 0; i < col; i++) {
        row = strchr(row, '\t');
        if (row == NULL) return 0;
        row++;
    }
    const size_t n = strcspn(row, "\t\r\n");
    if (n == 0 || n >= cap) return 0;
    memcpy(out, row, n);
    out[n] = '\0';
    return 1;
}

/* the bitmap file of `name_hex` in `bank`'s entries.tsv, as `bank`/<file>, when the row names a PNG that is there */
static int in_bank(const char *bank, const char *name_hex, char *out, size_t cap)
{
    char path[4096], row[ROW_CAP], hex[16], file[64];
    if (snprintf(path, sizeof path, "%s/entries.tsv", bank) >= (int)sizeof path) return 0;
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    int ok = 0;
    while (!ok && fgets(row, sizeof row, f) != NULL) {
        if (row[0] == '#' || !cell(row, COL_NAME_HEX, hex, sizeof hex) || strcmp(hex, name_hex) != 0) continue;
        if (!cell(row, COL_FILE, file, sizeof file) || strchr(file, '/') != NULL) break;
        const size_t len = strlen(file);
        if (len < 4 || strcmp(file + len - 4, ".png") != 0) break;   /* a palette (.bin) is no texture to paint */
        ok = snprintf(out, cap, "%s/%s", bank, file) < (int)cap && access(out, R_OK) == 0;
    }
    fclose(f);
    return ok;
}

int texture_file_of(const char *kit_root, int level, const char *name, char *out, size_t cap)
{
    if (out == NULL || cap == 0) return 0;
    out[0] = '\0';
    if (kit_root == NULL || name == NULL || (level != 0 && level != 1)) return 0;
    char name_hex[9];
    unsigned char b[4] = {0, 0, 0, 0};
    memcpy(b, name, strnlen(name, 4));
    snprintf(name_hex, sizeof name_hex, "%02x%02x%02x%02x", b[0], b[1], b[2], b[3]);
    char bank[4096];
    if (snprintf(bank, sizeof bank, "%s/%s", kit_root, car_kit_path(level == 0 ? CAR_KIT_NEAR_BANK : CAR_KIT_FAR_BANK)) < (int)sizeof bank &&
        in_bank(bank, name_hex, out, cap)) return 1;
    out[0] = '\0';
    return 0;
}
