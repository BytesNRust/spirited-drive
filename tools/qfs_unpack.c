/* Decompresses QFS files with the port's decoder: qfs_unpack OUTDIR FILE...
 * writes OUTDIR/<basename>.bin, the same layout as tools/emu_oracle.py qfs. */
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

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    if (back != NULL && (slash == NULL || back > slash)) slash = back;
    return slash != NULL ? slash + 1 : path;
}

static int unpack(const char *outdir, const char *path)
{
    size_t src_len = 0, out_len = 0;
    uint8_t *src = read_file(path, &src_len);
    if (src == NULL) {
        fprintf(stderr, "%s: cannot read\n", path);
        return 0;
    }
    QfsResult r = qfs_decompressed_size(src, src_len, &out_len);
    uint8_t *out = r == QFS_OK ? malloc(out_len ? out_len : 1) : NULL;
    if (r == QFS_OK && out == NULL) r = QFS_ERR_OVERFLOW;
    if (r == QFS_OK) r = qfs_decompress(src, src_len, out, out_len);

    int ok = 0;
    if (r != QFS_OK) {
        fprintf(stderr, "%s: %s\n", path, qfs_result_str(r));
    } else {
        char dest[4096];
        snprintf(dest, sizeof dest, "%s/%s.bin", outdir, base_name(path));
        FILE *f = fopen(dest, "wb");
        ok = f != NULL && fwrite(out, 1, out_len, f) == out_len;
        if (f != NULL && fclose(f) != 0) ok = 0;
        if (!ok) fprintf(stderr, "%s: cannot write\n", dest);
    }
    free(out);
    free(src);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s OUTDIR FILE...\n", argv[0]);
        return 2;
    }
    int failures = 0;
    for (int i = 2; i < argc; i++) {
        if (!unpack(argv[1], argv[i])) failures++;
    }
    return failures == 0 ? 0 : 1;
}
