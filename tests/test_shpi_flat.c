/* dep85 step 1: a texture bank built from its flat files alone (shpi_from_flat), with no bank to start from. shpi_export writes
 * entries.tsv (a row per entry in directory order, its physical place in `pos`) and bank.tsv (the head's tag and the bytes between the
 * directory and the first entry); shpi_from_flat gives the bank back byte for byte: a synthetic bank with every case the CD has (a tag,
 * a gap, a physical order not the directory's, !PAL not first, palette and bitmap tails), and each of the CD's 78 banks. What it refuses
 * it names. An RGB picture on the palette's own colours gives the same bank. The CD part is skipped without original/. */
#include "formats/cfm_build.h"
#include "formats/fsh.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "formats/shpi_exchange.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint8_t pal[768], tex1[4 * 2], tex2[2 * 2];
static const uint8_t pal_tail[] = { 0x00, 0x00 }, tex1_tail[] = { 0x7c, 0, 0, 0, 0, 0, 0, 0 }, tex2_tail[] = { 0xff }, gap[] = { 0x9d, 0xfe };

static ShpiDescEntry entry(const char *name, uint8_t type, uint16_t w, uint16_t h, const uint8_t *data, uint32_t dir_index)
{
    ShpiDescEntry e;
    memset(&e, 0, sizeof e);
    memcpy(e.name, name, 4);
    e.type = type, e.width = w, e.height = h, e.data = data, e.block_size = SHPI_BLOCK_AUTO, e.dir_index = dir_index;
    return e;
}

/* tex1 in the directory first and in the file last, !PAL second in both, tex2 last in the directory and first in the file; tag ABCD;
 * two bytes between the directory and the first entry */
static int synthetic(uint8_t **bank, size_t *len)
{
    for (int k = 0; k < 256; ++k) pal[3 * k] = (uint8_t)k, pal[3 * k + 1] = (uint8_t)(255 - k), pal[3 * k + 2] = (uint8_t)(k ^ 0x55);   /* 256 colours, each once */
    for (int i = 0; i < 8; ++i) tex1[i] = (uint8_t)(10 + i);
    for (int i = 0; i < 4; ++i) tex2[i] = (uint8_t)(200 + i);
    ShpiDescEntry e[3] = { entry("tex2", 0x7b, 2, 2, tex2, 2), entry("!PAL", 0x24, 0, 0, pal, 1), entry("tex1", 0x7b, 4, 2, tex1, 0) };
    e[0].lead = gap, e[0].lead_len = sizeof gap;
    e[0].tail = tex2_tail, e[0].tail_len = sizeof tex2_tail;
    e[1].tail = pal_tail, e[1].tail_len = sizeof pal_tail, e[1].misc[0] = 256;
    e[2].tail = tex1_tail, e[2].tail_len = sizeof tex1_tail, e[2].block_size = 16 + 8;
    const ShpiHead head = { { 'A', 'B', 'C', 'D' }, 0, 0 };
    ExchangeError err = { { 0 } };
    return shpi_build_head(&head, e, 3, bank, len, &err);
}

static int same(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len)
{
    return a != NULL && b != NULL && a_len == b_len && memcmp(a, b, a_len) == 0;
}

/* the bank from `flat`: 1 when it is `bank`; the error in `err` */
static int rebuilds(const FileSet *flat, const uint8_t *bank, size_t len, ExchangeError *err)
{
    uint8_t *out = NULL;
    size_t out_len = 0;
    ImportStats stats;
    memset(err, 0, sizeof *err);
    const int ok = shpi_from_flat(flat, NULL, &out, &out_len, &stats, err) && same(out, out_len, bank, len);
    free(out);
    return ok;
}

/* `flat` with file `name` replaced by `text` */
static void replaced(FileSet *to, const FileSet *flat, const char *name, const char *text)
{
    fileset_init(to);
    for (size_t i = 0; i < flat->count; ++i)
        if (strcmp(flat->file[i].name, name) != 0) fileset_add(to, flat->file[i].name, flat->file[i].data, flat->file[i].size);
    if (text != NULL) fileset_add(to, name, (const uint8_t *)text, strlen(text));
}

static void check_synthetic(void)
{
    uint8_t *bank = NULL;
    size_t len = 0;
    CHECK(synthetic(&bank, &len));
    if (bank == NULL) return;
    CHECK(memcmp(bank + 12, "ABCD", 4) == 0);
    ShpiHead head;
    CHECK(shpi_describe_head(bank, len, &head) && memcmp(head.tag, "ABCD", 4) == 0);
    FileSet flat;
    fileset_init(&flat);
    ExchangeError err = { { 0 } };
    CHECK(shpi_export(bank, len, NULL, &flat, &err));
    const SetFile *b = fileset_find(&flat, SHPI_BANK_FILE);
    CHECK(b != NULL && memmem(b->data, b->size, "tag\t41424344", 12) != NULL && memmem(b->data, b->size, "lead_hex\t9dfe", 13) != NULL);
    CHECK(b != NULL && memmem(b->data, b->size, "size_word\tauto\n", 15) != NULL);   /* the word is the bank's length */
    CHECK(rebuilds(&flat, bank, len, &err));
    if (err.message[0] != '\0') fprintf(stderr, "synthetic: %s\n", err.message);

    /* four bytes after the bank's declared length, as a CFM may hold them: they travel in the last entry's tail (as the unpack has always
     * kept them, so pack --from is unchanged), and bank.tsv keeps the size word as stored, not the length */
    uint8_t *longer = malloc(len + 4);
    if (longer != NULL) {
        memcpy(longer, bank, len), memcpy(longer + len, "\x12\x34\x00\x00", 4);
        FileSet more;
        fileset_init(&more);
        char size_line[32];
        snprintf(size_line, sizeof size_line, "size_word\t%zu\n", len);
        CHECK(shpi_export(longer, len + 4, NULL, &more, &err));
        const SetFile *mb = fileset_find(&more, SHPI_BANK_FILE);
        CHECK(mb != NULL && memmem(mb->data, mb->size, size_line, strlen(size_line)) != NULL);
        CHECK(rebuilds(&more, longer, len + 4, &err));
        fileset_free(&more);
        free(longer);
    }

    FileSet bad;
    replaced(&bad, &flat, SHPI_BANK_FILE, NULL);   /* no bank.tsv: no default head */
    CHECK(!rebuilds(&bad, bank, len, &err) && strstr(err.message, SHPI_BANK_FILE) != NULL);
    fileset_free(&bad);
    replaced(&bad, &flat, SHPI_BANK_FILE, "lead_hex\t9dfe\nsize_word\tauto\n");   /* no tag */
    CHECK(!rebuilds(&bad, bank, len, &err) && strstr(err.message, "tag") != NULL);
    fileset_free(&bad);
    replaced(&bad, &flat, SHPI_BANK_FILE, "tag\t41424344\nlead_hex\t9dfe\nsize_word\tauto\nsize\t9\n");   /* a key it does not know */
    CHECK(!rebuilds(&bad, bank, len, &err) && strstr(err.message, "size") != NULL);
    fileset_free(&bad);

    /* two entries in one physical place */
    const SetFile *t = fileset_find(&flat, SHPI_ENTRIES_FILE);
    char *text = t != NULL ? calloc(1, t->size + 1) : NULL;
    if (text != NULL) {
        memcpy(text, t->data, t->size);
        char *last = strrchr(text, '\t');   /* the last row's pos, its last cell */
        CHECK(last != NULL && last[1] == '0');
        if (last != NULL) last[1] = '1';
        replaced(&bad, &flat, SHPI_ENTRIES_FILE, text);
        CHECK(!rebuilds(&bad, bank, len, &err) && strstr(err.message, "pos") != NULL);
        fileset_free(&bad);
        free(text);
    }

    /* tex1 painted as RGB on the palette's own colours: the same bank, and the picture as it was */
    char file[SHPI_FILE_NAME_SIZE];
    CHECK(shpi_entry_file(bank, len, "tex1", file));
    uint8_t rgba[4 * 8];
    for (int i = 0; i < 8; ++i)
        for (int c = 0; c < 3; ++c) rgba[4 * i + c] = pal[3 * tex1[i] + c];   /* 0x24: 8 bits a channel, as stored */
    for (int i = 0; i < 8; ++i) rgba[4 * i + 3] = 0xff;
    uint8_t *png = NULL;
    size_t png_len = 0;
    CHECK(png_write_rgba(rgba, 4, 2, &png, &png_len) == PNG_OK);
    fileset_init(&bad);
    for (size_t i = 0; i < flat.count; ++i)
        fileset_add(&bad, flat.file[i].name, strcmp(flat.file[i].name, file) == 0 ? png : flat.file[i].data,
                    strcmp(flat.file[i].name, file) == 0 ? png_len : flat.file[i].size);
    CHECK(rebuilds(&bad, bank, len, &err));
    const SetFile *kept = fileset_find(&bad, file);
    CHECK(kept != NULL && same(kept->data, kept->size, png, png_len));
    fileset_free(&bad);
    free(png);
    fileset_free(&flat);
    free(bank);
}

/* an entries.tsv of before `pos`: the directory's order is the file's */
static void check_old_entries(void)
{
    static uint8_t p[768], t[3 * 3];
    ShpiDescEntry e[2] = { entry("!PAL", 0x24, 0, 0, p, 0), entry("abcd", 0x7b, 3, 3, t, 1) };
    uint8_t *bank = NULL;
    size_t len = 0;
    ExchangeError err = { { 0 } };
    CHECK(shpi_build(e, 2, &bank, &len, &err));
    FileSet flat, old;
    fileset_init(&flat);
    CHECK(shpi_export(bank, len, NULL, &flat, &err));
    const SetFile *f = fileset_find(&flat, SHPI_ENTRIES_FILE);
    char *text = f != NULL ? calloc(1, f->size + 1) : NULL;
    if (text != NULL) {
        size_t n = 0;
        for (size_t i = 0; i < f->size; ++i) {   /* each line without its last cell */
            if (f->data[i] == '\n') {
                char *tab = text + n;
                while (tab > text && tab[-1] != '\n' && *tab != '\t') --tab;
                if (*tab == '\t') n = (size_t)(tab - text);
            }
            text[n++] = (char)f->data[i];
        }
        text[n] = '\0';
        CHECK(strstr(text, "\tpos") == NULL);
        replaced(&old, &flat, SHPI_ENTRIES_FILE, text);
        CHECK(rebuilds(&old, bank, len, &err));
        fileset_free(&old);
        free(text);
    }
    fileset_free(&flat);
    free(bank);
}

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

/* each bank of each CD CFM (both models), as the CFM holds it (to the next child: 4 banks have bytes of their own after their declared
 * length): unpacked, then built from the flat files alone, it is the CD's bytes */
static void check_cd(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) return;
    int banks = 0, built = 0;
    struct dirent *de;
    while ((de = readdir(h)) != NULL) {
        const size_t name_len = strlen(de->d_name);
        if (name_len < 5 || strcasecmp(de->d_name + name_len - 4, ".CFM") != 0) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        size_t n = 0;
        uint8_t *cfm = read_all(path, &n);
        const uint8_t *kid[CFM_CHILDREN];
        size_t kid_len[CFM_CHILDREN];
        ExchangeError cerr = { { 0 } };
        for (int child = 1; cfm != NULL && child < CFM_CHILDREN && cfm_describe(cfm, n, kid, kid_len, &cerr); child += 2) {
            banks++;   /* the bank as the CFM holds it: to the next child, the bytes after its declared length too */
            FileSet flat;
            fileset_init(&flat);
            ExchangeError err = { { 0 } };
            const int ok = shpi_export(kid[child], kid_len[child], NULL, &flat, &err) && rebuilds(&flat, kid[child], kid_len[child], &err);
            if (!ok) fprintf(stderr, "shpi_flat: %s child %d: %s\n", de->d_name, child, err.message[0] ? err.message : "other bytes");
            built += ok;
            fileset_free(&flat);
        }
        free(cfm);
    }
    closedir(h);
    CHECK_EQ_INT(banks, 78);
    CHECK_EQ_INT(built, banks);
}

int main(void)
{
    check_synthetic();
    check_old_entries();
    check_cd();
    return TEST_REPORT();
}
