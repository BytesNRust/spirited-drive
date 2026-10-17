/* The network menu's list of services (00443258, 004432d4): the
 * transport's getnetservices (00478e6c) copied into a block of names and
 * GUIDs that 00442514 / 004425a0 step through. */
#ifndef TNFS_RACE_NET_SERVICES_H
#define TNFS_RACE_NET_SERVICES_H

#include "game/net/net_service.h"

#include <stdint.h>

/* A service in the list: its name (+0, the string as the driver gives it),
 * its GUID (+0x49, 16 bytes); the block holds ten */
enum { RACE_NET_SERVICE_SIZE = 0x59, RACE_NET_SERVICE_GUID = 0x49, RACE_NET_SERVICES_BYTES = 0x37a };

typedef struct {
    uint8_t *list;  /* 004bc6c0: the block, NULL when none */
    int16_t shown;  /* 004bc6d4: the count once the list is made */
    int16_t count;  /* 004bc6d6: the services put so far, then the one picked */
    int16_t open;   /* 004bc6d2: a service started on the race's session (0050a260) */
} RaceNetServices;

extern const char RACE_NET_SERVICE_LIST[];  /* 004b00a4 "ServiceList", the block's name */
extern const char RACE_NET_NO_SERVICES[];      /* 004afe54 */
extern const char RACE_NET_NO_SERVICES_DE[];   /* 004afe30 */

typedef struct {
    void *ctx;
    void (*free)(void *ctx, uint8_t *p);                                              /* 00475260 (cdecl) */
    uint8_t *(*alloc)(void *ctx, const char *name, uint32_t size, uint32_t flags);  /* 00474c8c (cdecl) */
    /* 00478e6c getnetservices (cdecl (callback, a, b, most)) */
    int32_t (*services)(void *ctx, const NetServicesCallback *cb, int32_t a, int32_t b, uint32_t most);
    void (*kill_service)(void *ctx);                                  /* 004790b0 (cdecl (0050a260)) */
    void (*banner_wait)(void *ctx, const char *text, int32_t time);  /* 00467214 (watcall) */
} RaceNetServicesOps;

/* 00443258 (getnetservices' callback, cdecl (guid, name, a, b); ctx the
 * RaceNetServices): no block: 0. Else the name (strcpy) at entry 004bc6d6
 * (signed), the GUID's 16 bytes at its +0x49 (the block and the count read
 * again), 004bc6d6 + 1; 1. a and b unused. */
int32_t race_net_service_found(void *ctx, const uint8_t *guid, const char *name, int32_t a, int32_t b);

/* 004432d4 (watcall (a, b, most), from 00442514): the block there freed
 * (00475260) and cleared; a new one (00474c8c("ServiceList", 0x37a, 0x20));
 * 004bc6d4 and 004bc6d6 0; getnetservices(00443258, a, b, most) 0: 0 (the
 * block kept). Else 004bc6d4 = 004bc6d6; none: the block freed and
 * cleared. The block returned. */
uint8_t *race_net_services_list(const RaceNetServicesOps *ops, RaceNetServices *s, int32_t a, int32_t b,
                                uint32_t most);

/* 00442514 (watcall (), from 004425a0, 00442614 and the network menu):
 * 004bc6d2 set: cleared, 004790b0(0050a260). No block: 004432d4(0x2580, 0,
 * 0xc). Still none: 00467214(004afe54, or 004afe30 with 0052b31c (german)
 * set, 0x12c), NULL. Else 004bc6d6 0 and the block (its first entry). */
uint8_t *race_net_services_first(const RaceNetServicesOps *ops, RaceNetServices *s, int16_t german);

#endif
