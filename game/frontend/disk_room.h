/* Whether the disk has room for a file. */
#ifndef TNFS_DISK_ROOM_H
#define TNFS_DISK_ROOM_H

#include <stdint.h>

/* 00496148's answer, four shorts (Watcom's struct diskfree_t, guessed). */
typedef struct DiskFree {
    uint16_t total_clusters;
    uint16_t avail_clusters;
    uint16_t sectors_per_cluster;
    uint16_t bytes_per_sector;
} DiskFree;

typedef struct DiskRoomOps {
    void (*cwd)(void *ctx, char *buf, int32_t size);              /* 004960cf: eax, edx (getcwd, guessed) */
    int32_t (*disk_free)(void *ctx, int32_t drive, DiskFree *d);  /* 00496148: eax, edx; 0, else an error */
    void *ctx;
} DiskRoomOps;

/* 0046733c: 1 when the current directory's drive (its letter - 0x40, so
 * 'C' is 3) has at least `needed` bytes free: avail clusters x bytes per
 * sector x sectors per cluster, as a 32-bit product compared signed; 0 when
 * it has not, or when 00496148 fails. */
int disk_has_room(int32_t needed, const DiskRoomOps *ops);

#endif
