/* The load / save screens' file list: names of 0x14 bytes each, one after
 * the other (004fa1dc, up to 100 read by 0042b1cb's caller). */
#ifndef TNFS_FILE_LIST_H
#define TNFS_FILE_LIST_H

#include <stdint.h>

enum { FILE_LIST_NAME = 0x14 };

/* 0042ad94: drops the names equal, ignoring the case of A..Z, to
 * REPLAY.RPL or REPLAY1..3.RPL. A drop copies each later name over the one
 * before it with strcpy, through the name just past the end (so names holds
 * *count + 1 of them), and lowers *count. */
void file_list_drop_replays(uint8_t *names, int16_t *count);

#endif
