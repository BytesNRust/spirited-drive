#include "formats/bnk_exchange.h"

#include "formats/bnk.h"
#include "formats/le.h"
#include "formats/tsv.h"
#include "formats/wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FLAGS_AT = 0, EACS_REF_AT = 4, RATE_AT = 0x2c, BITS_AT = 0x30, CHANNELS_AT = 0x31,
    LOOP_START_AT = 0x34, LOOP_LENGTH_AT = 0x38, LENGTH_AT = 0x3c, DATA_AT = 0x40
};

static const char table_file[] = "bank.tsv";

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

/* The header with every field this exchange sets zeroed. */
static void header_rest(const uint8_t *header, uint8_t rest[BNK_HEADER_SIZE])
{
    memcpy(rest, header, BNK_HEADER_SIZE);
    memset(rest + FLAGS_AT, 0, 8);               /* flags and the +4 reference */
    memset(rest + RATE_AT, 0, 4);
    rest[BITS_AT] = rest[CHANNELS_AT] = 0;
    memset(rest + LOOP_START_AT, 0, 16);         /* loop start and length, length, data offset */
}

int bnk_export(const uint8_t *data, size_t size, FileSet *out, ExchangeError *err)
{
    BnkDoc *doc = NULL;
    Bnk bnk;
    BnkResult r = bnk_open(&bnk, data, size);
    if (r == BNK_OK) r = bnk_doc_read(data, size, &doc);
    if (r != BNK_OK) { exchange_fail(err, "not a readable sound bank: %s", bnk_result_str(r)); return 0; }

    TsvWriter tsv;
    tsv_writer_init(&tsv);
    tsv_comment(&tsv, "slot\tfile\tflags\tloop_start\tloop_length\tlength\trate\tbits\tchannels\ttail\trest_hex");
    int first_slot_of[BNK_SLOTS];
    size_t owner_frames[BNK_SLOTS] = {0};
    for (int b = 0; b < BNK_SLOTS; ++b) first_slot_of[b] = -1;
    int ok = 1;
    for (int slot = 0; slot < BNK_SLOTS && ok; ++slot) {
        const int block = bnk_doc_slot_block(doc, slot);
        if (block < 0) continue;
        BnkSample s;
        r = bnk_sample(&bnk, slot, &s);
        if (r != BNK_OK && r != BNK_ERR_DATA) { exchange_fail(err, "slot %d: %s", slot, bnk_result_str(r)); ok = 0; break; }
        size_t extent = 0;
        const uint8_t *pcm = bnk_doc_block(doc, block, &extent);
        const size_t fb = (size_t)bnk_frame_bytes(&s);
        size_t frames = (s.length > 0 && (uint64_t)s.length * fb <= extent) ? s.length : extent / fb;
        const size_t tail = extent - frames * fb;
        char wav_name[16], tail_name[16];
        if (first_slot_of[block] < 0) { first_slot_of[block] = slot; owner_frames[block] = frames; }
        snprintf(wav_name, sizeof wav_name, "%03d.wav", first_slot_of[block]);
        snprintf(tail_name, sizeof tail_name, "%03d.tail", first_slot_of[block]);
        if (first_slot_of[block] == slot) {
            WavInfo info = { s.rate, s.bits, s.channels, frames };
            uint8_t *wav = NULL;
            size_t wav_len = 0;
            const WavResult w = wav_write(pcm, &info, &wav, &wav_len);
            if (w != WAV_OK || !fileset_add_owned(out, wav_name, wav, wav_len)) {
                if (w != WAV_OK) free(wav);
                exchange_fail(err, "slot %d: cannot write %s (%s)", slot, wav_name, w == WAV_OK ? "name taken" : wav_result_str(w));
                ok = 0;
                break;
            }
            if (tail > 0 && !fileset_add(out, tail_name, pcm + frames * fb, tail)) { exchange_fail(err, "cannot write %s", tail_name); ok = 0; break; }
        }
        uint8_t rest[BNK_HEADER_SIZE];
        header_rest(s.header, rest);
        tsv_uint(&tsv, (uint32_t)slot);
        tsv_str(&tsv, wav_name);
        tsv_uint(&tsv, s.flags);
        tsv_uint(&tsv, s.loop_start);
        tsv_uint(&tsv, s.loop_length);
        if (s.length == owner_frames[block]) tsv_str(&tsv, "auto");
        else tsv_uint(&tsv, s.length);
        tsv_uint(&tsv, s.rate);
        tsv_uint(&tsv, (uint32_t)s.bits);
        tsv_uint(&tsv, (uint32_t)s.channels);
        tsv_str(&tsv, tail > 0 ? tail_name : "-");
        tsv_hex(&tsv, rest, BNK_HEADER_SIZE);
        tsv_row(&tsv);
    }
    if (ok && (tsv.failed || !fileset_add(out, table_file, tsv.data, tsv.len))) { exchange_fail(err, "cannot write %s", table_file); ok = 0; }
    tsv_writer_free(&tsv);
    bnk_doc_free(doc);
    return ok;
}

typedef struct {
    int slot;
    uint32_t line;
    char wav[32], tail[32];
    uint32_t flags, loop_start, loop_length, length, rate, bits, channels;
    int auto_length;
    uint8_t rest[BNK_HEADER_SIZE];
} Row;

static int parse_row(const TsvRow *t, Row *row, ExchangeError *err)
{
    uint32_t slot = 0;
    size_t n = 0;
    memset(row, 0, sizeof *row);
    row->line = t->line;
    if (t->cells < 11) { exchange_fail(err, "%s:%u: expected 11 cells, found %d", table_file, t->line, t->cells); return 0; }
    if (tsv_get_uint(t, 0, &slot) != TSV_OK || slot >= BNK_SLOTS) { exchange_fail(err, "%s:%u: bad slot", table_file, t->line); return 0; }
    row->slot = (int)slot;
    if (strlen(t->cell[1]) >= sizeof row->wav || !fileset_name_ok(t->cell[1]) || strlen(t->cell[9]) >= sizeof row->tail) {
        exchange_fail(err, "%s:%u: unusable file name", table_file, t->line);
        return 0;
    }
    strcpy(row->wav, t->cell[1]);
    strcpy(row->tail, t->cell[9]);
    if (tsv_get_uint(t, 2, &row->flags) != TSV_OK || tsv_get_uint(t, 3, &row->loop_start) != TSV_OK ||
        tsv_get_uint(t, 4, &row->loop_length) != TSV_OK) {
        exchange_fail(err, "%s:%u: flags, loop_start and loop_length must be numbers", table_file, t->line);
        return 0;
    }
    if (strcmp(t->cell[5], "auto") == 0) row->auto_length = 1;
    else if (tsv_get_uint(t, 5, &row->length) != TSV_OK) { exchange_fail(err, "%s:%u: length must be auto or a number", table_file, t->line); return 0; }
    if (tsv_get_uint(t, 6, &row->rate) != TSV_OK || tsv_get_uint(t, 7, &row->bits) != TSV_OK ||
        tsv_get_uint(t, 8, &row->channels) != TSV_OK || (row->bits != 8 && row->bits != 16) || (row->channels != 1 && row->channels != 2)) {
        exchange_fail(err, "%s:%u: rate, bits (8 or 16) and channels (1 or 2) must be numbers", table_file, t->line);
        return 0;
    }
    if (tsv_get_hex(t, 10, row->rest, BNK_HEADER_SIZE, &n) != TSV_OK || n != BNK_HEADER_SIZE) {
        exchange_fail(err, "%s:%u: rest_hex must be %d bytes of hex", table_file, t->line, BNK_HEADER_SIZE);
        return 0;
    }
    return 1;
}

int bnk_import(const uint8_t *original, size_t size, const FileSet *in, uint8_t **out, size_t *out_len, ExchangeError *err)
{
    const SetFile *table = fileset_find(in, table_file);
    if (table == NULL) { exchange_fail(err, "%s not found", table_file); return 0; }
    BnkDoc *doc = NULL;
    const BnkResult r = bnk_doc_read(original, size, &doc);
    if (r != BNK_OK) { exchange_fail(err, "original is not a readable sound bank: %s", bnk_result_str(r)); return 0; }

    Row *rows = calloc(BNK_SLOTS, sizeof *rows);
    int seen[BNK_SLOTS] = {0};
    int block_done[BNK_SLOTS] = {0};
    int block_owner[BNK_SLOTS];
    char block_wav[BNK_SLOTS][32];
    for (int b = 0; b < BNK_SLOTS; ++b) block_owner[b] = -1;
    WavInfo block_info[BNK_SLOTS];
    memset(block_wav, 0, sizeof block_wav);
    int ok = rows != NULL;
    size_t nrows = 0;
    TsvReader reader;
    TsvRow t;
    tsv_reader_init(&reader, table->data, table->size);
    while (ok && tsv_next(&reader, &t) == TSV_OK) {
        Row *row = &rows[nrows];
        if (!parse_row(&t, row, err)) { ok = 0; break; }
        if (bnk_doc_slot_block(doc, row->slot) < 0) { exchange_fail(err, "%s:%u: slot %d is empty in the original (adding slots is not supported)", table_file, t.line, row->slot); ok = 0; break; }
        if (seen[row->slot]++) { exchange_fail(err, "%s:%u: slot %d listed twice", table_file, t.line, row->slot); ok = 0; break; }
        ++nrows;
    }
    tsv_reader_free(&reader);
    for (int slot = 0; ok && slot < BNK_SLOTS; ++slot)
        if (bnk_doc_slot_block(doc, slot) >= 0 && !seen[slot]) { exchange_fail(err, "%s lacks slot %d of the original", table_file, slot); ok = 0; }

    for (size_t i = 0; ok && i < nrows; ++i) {
        const Row *row = &rows[i];
        const int block = bnk_doc_slot_block(doc, row->slot);
        if (!block_done[block]) {
            const SetFile *wf = fileset_find(in, row->wav);
            WavInfo info;
            uint8_t *pcm = NULL;
            if (wf == NULL) { exchange_fail(err, "%s:%u: file '%s' not found", table_file, row->line, row->wav); ok = 0; break; }
            const WavResult w = wav_read(wf->data, wf->size, &info, &pcm);
            if (w != WAV_OK) { exchange_fail(err, "%s: %s", row->wav, wav_result_str(w)); ok = 0; break; }
            size_t tail_len = 0;
            const SetFile *tf = strcmp(row->tail, "-") == 0 ? NULL : fileset_find(in, row->tail);
            if (strcmp(row->tail, "-") != 0) {
                if (tf == NULL) { exchange_fail(err, "%s:%u: file '%s' not found", table_file, row->line, row->tail); free(pcm); ok = 0; break; }
                tail_len = tf->size;
            }
            const size_t pcm_len = info.frames * ((size_t)info.bits / 8 * (size_t)info.channels);
            uint8_t *all = malloc(pcm_len + tail_len + 1);
            if (all == NULL) { free(pcm); exchange_fail(err, "out of memory"); ok = 0; break; }
            memcpy(all, pcm, pcm_len);
            if (tail_len) memcpy(all + pcm_len, tf->data, tail_len);
            free(pcm);
            ok = bnk_doc_set_block(doc, block, all, pcm_len + tail_len) == BNK_OK;
            free(all);
            if (!ok) { exchange_fail(err, "cannot replace the data of slot %d", row->slot); break; }
            block_done[block] = 1;
            block_owner[block] = row->slot;
            strcpy(block_wav[block], row->wav);
            block_info[block] = info;
        } else if (strcmp(block_wav[block], row->wav) != 0) {
            exchange_fail(err, "%s:%u: slot %d shares its data with an earlier slot and must use the same file '%s'", table_file, row->line, row->slot, block_wav[block]);
            ok = 0;
            break;
        }
        const int owner = block_owner[block] == row->slot;
        const WavInfo *wi = &block_info[block];
        const WavInfo info_row = owner ? *wi : (WavInfo){ row->rate, (int)row->bits, (int)row->channels, wi->frames };
        const WavInfo *info = &info_row;
        uint8_t *h = bnk_doc_header(doc, row->slot);
        uint8_t eacs_ref[4];
        memcpy(eacs_ref, h + EACS_REF_AT, 4);
        memcpy(h, row->rest, BNK_HEADER_SIZE);
        memcpy(h + EACS_REF_AT, eacs_ref, 4);
        put_u32(h + FLAGS_AT, row->flags);
        put_u32(h + RATE_AT, info->rate);
        h[BITS_AT] = (uint8_t)(info->bits / 8);
        h[CHANNELS_AT] = (uint8_t)info->channels;
        put_u32(h + LOOP_START_AT, row->loop_start);
        put_u32(h + LOOP_LENGTH_AT, row->loop_length);
        put_u32(h + LENGTH_AT, row->auto_length ? (uint32_t)info->frames : row->length);
    }
    if (ok) {
        const size_t total = bnk_doc_size(doc);
        uint8_t *buf = malloc(total ? total : 1);
        size_t len = 0;
        if (buf == NULL || bnk_doc_write(doc, buf, total, &len) != BNK_OK) { free(buf); exchange_fail(err, "cannot assemble the bank"); ok = 0; }
        else { *out = buf; *out_len = len; }
    }
    free(rows);
    bnk_doc_free(doc);
    return ok;
}
