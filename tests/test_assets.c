/* Integration test over the real CD data. Every QFS given on the command line
 * must decompress cleanly and its SHPI length field must equal the decoded
 * size (an oracle independent of the decoder). Every SHPI container, from a
 * QFS or a plain FSH, must parse entry by entry, palettes included.
 * Registered by CMake only when original/ holds the extracted CD. */
#include "formats/fsh.h"
#include "formats/qfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long len = ftell(f);
        if (len >= 0 && fseek(f, 0, SEEK_SET) == 0 && (data = malloc((size_t)len + 1)) != NULL) {
            *size = fread(data, 1, (size_t)len, f);
            if (*size != (size_t)len) {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(f);
    return data;
}

typedef struct {
    int containers;
    int entries;
    int bitmaps;
    int palettes;
} Stats;

static int check_fsh(const char *path, const uint8_t *data, size_t size, Stats *stats)
{
    Fsh fsh;
    FshResult r = fsh_open(&fsh, data, size);
    if (r != FSH_OK) {
        fprintf(stderr, "%s: %s\n", path, fsh_result_str(r));
        return 0;
    }
    stats->containers++;
    for (uint32_t i = 0; i < fsh.count; i++) {
        FshEntry e;
        uint8_t rgb[768];
        if ((r = fsh_entry(&fsh, i, &e)) == FSH_OK && e.type != FSH_TYPE_BITMAP) {
            r = fsh_palette_rgb(&e, rgb);
            stats->palettes += r == FSH_OK;
        } else if (r == FSH_OK) {
            stats->bitmaps++;
        }
        if (r != FSH_OK) {
            fprintf(stderr, "%s: entry %u: %s\n", path, i, fsh_result_str(r));
            return 0;
        }
        stats->entries++;
    }
    return 1;
}

static int check_file(const char *path, int *shpi_count, Stats *stats)
{
    size_t src_len = 0, out_len = 0;
    uint8_t *src = read_file(path, &src_len);
    if (src == NULL) {
        fprintf(stderr, "%s: cannot read\n", path);
        return 0;
    }
    if (!qfs_is_compressed(src, src_len)) {
        int ok = check_fsh(path, src, src_len, stats);
        free(src);
        return ok;
    }
    QfsResult r = qfs_decompressed_size(src, src_len, &out_len);
    uint8_t *out = r == QFS_OK ? malloc(out_len ? out_len : 1) : NULL;
    if (r == QFS_OK && out == NULL) r = QFS_ERR_OVERFLOW;
    if (r == QFS_OK) r = qfs_decompress(src, src_len, out, out_len);

    int ok = r == QFS_OK;
    if (!ok) {
        fprintf(stderr, "%s: %s\n", path, qfs_result_str(r));
    } else if (out_len >= 8 && memcmp(out, "SHPI", 4) == 0) {
        const uint32_t declared = out[4] | (out[5] << 8) | ((uint32_t)out[6] << 16) | ((uint32_t)out[7] << 24);
        ++*shpi_count;
        if (declared != out_len) {
            fprintf(stderr, "%s: SHPI declares %u bytes, decoded %zu\n", path, declared, out_len);
            ok = 0;
        }
        ok = ok && check_fsh(path, out, out_len, stats);
    } else {
        fprintf(stderr, "%s: decoded %zu bytes, payload starts '%.4s' (not SHPI)\n", path, out_len, (const char *)out);
    }
    free(out);
    free(src);
    return ok;
}

int main(int argc, char **argv)
{
    int failures = 0, shpi = 0;
    Stats stats = { 0, 0, 0, 0 };
    for (int i = 1; i < argc; i++) {
        if (!check_file(argv[i], &shpi, &stats)) failures++;
    }
    fprintf(stderr, "%d files, %d QFS sizes verified, %d SHPI containers, %d entries "
            "(%d bitmaps, %d palettes), %d failures\n", argc - 1, shpi, stats.containers,
            stats.entries, stats.bitmaps, stats.palettes, failures);
    return failures == 0 && argc > 1 ? 0 : 1;
}
