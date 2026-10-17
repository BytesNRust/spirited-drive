/* 00466c00: a file from the art directory through the .fsh loader, the
 * menus' inverse tables (004c5ff4) among its 22 callers' uses. */
#ifndef TNFS_INVERSE_LOAD_H
#define TNFS_INVERSE_LOAD_H

#include <stdint.h>

enum { INVERSE_LOAD_PATH = 0x50 };  /* 00466c00's stack buffer */

typedef struct InverseLoadOps {
    void *ctx;
    int32_t (*load)(void *ctx, const char *path, int32_t flags);  /* 00483e60 (cdecl) */
} InverseLoadOps;

/* 00466c00 (eax the name): sprintf(buffer, "%s%s", the art directory
 * 00524328 (paths.dat's slot 5), name), then 00483e60(buffer, 0x20)'s
 * answer. The original's buffer overflows past 0x50 characters; the port
 * cuts the path there. */
int32_t resource_load_inverse(const char *art_dir, const char *name, const InverseLoadOps *ops);

#endif
