#include "game/frontend/disk_room.h"

/* 0046733c */
int disk_has_room(int32_t needed, const DiskRoomOps *ops)
{
    enum { CWD = 0x28, DRIVE_A = 0x40 };
    char cwd[CWD] = { 0 };
    DiskFree d;
    ops->cwd(ops->ctx, cwd, CWD);
    if (ops->disk_free(ops->ctx, (int32_t)(uint8_t)cwd[0] - DRIVE_A, &d) != 0) return 0;
    const uint32_t free_bytes = (uint32_t)d.avail_clusters * d.bytes_per_sector * d.sectors_per_cluster;
    return (int32_t)free_bytes >= needed;
}
