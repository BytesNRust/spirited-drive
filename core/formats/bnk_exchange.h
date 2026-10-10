/* BNK <-> WAV + TSV. An export gives one NNN.wav per PCM block (NNN = the lowest slot using it; slots that
 * share data share the file), NNN.tail for bytes after the sample frames (the CD pads its samples), and
 * bank.tsv with a row per slot:
 *     slot  file  flags  loop_start  loop_length  length  rate  bits  channels  tail  rest_hex
 * `length` is the frame count of the header, or "auto" when it equals the WAV's (0 stays 0: the collision
 * and gear-change samples take their length from the data extent). `rate`, `bits` and `channels` follow the
 * WAV for the slot that owns it (the lowest slot of a block); slots that share the data (the CD reads one
 * block as 8-bit in one slot and 16-bit in another) keep the values in their row. `rest_hex` is the 72-byte
 * header with the fields above and the data offset (which follows the layout) zeroed. 8-bit data is signed in the game and unsigned in WAV; wav.h converts. An import
 * rebuilds the bank over the original's offset table and header positions; replace-only: the slots, and
 * which of them share data, are the original's. An unedited export imports back to the original bytes. */
#ifndef TNFS_BNK_EXCHANGE_H
#define TNFS_BNK_EXCHANGE_H

#include "formats/exchange.h"

int bnk_export(const uint8_t *data, size_t size, FileSet *out, ExchangeError *err);
int bnk_import(const uint8_t *original, size_t size, const FileSet *in, uint8_t **out, size_t *out_len, ExchangeError *err);

#endif
