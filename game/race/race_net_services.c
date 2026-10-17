/* The network menu's list of services (race_net_services.h) */
#include "game/race/race_net_services.h"

#include <stddef.h>
#include <string.h>

const char RACE_NET_SERVICE_LIST[] = "ServiceList";
const char RACE_NET_NO_SERVICES[] = "No network services found.";
const char RACE_NET_NO_SERVICES_DE[] = "Keinen Netzwerk-Service gefunden.";

int32_t race_net_service_found(void *ctx, const uint8_t *guid, const char *name, int32_t a, int32_t b)
{
    RaceNetServices *const s = ctx;
    (void)a, (void)b;
    if (s->list == NULL) return 0;
    strcpy((char *)s->list + s->count * RACE_NET_SERVICE_SIZE, name);
    memcpy(s->list + s->count * RACE_NET_SERVICE_SIZE + RACE_NET_SERVICE_GUID, guid, 16);
    s->count = (int16_t)(s->count + 1);
    return 1;
}

uint8_t *race_net_services_list(const RaceNetServicesOps *ops, RaceNetServices *s, int32_t a, int32_t b,
                                uint32_t most)
{
    if (s->list != NULL) {
        ops->free(ops->ctx, s->list);
        s->list = NULL;
    }
    s->list = ops->alloc(ops->ctx, RACE_NET_SERVICE_LIST, RACE_NET_SERVICES_BYTES, 0x20);
    s->shown = 0;
    s->count = 0;
    const NetServicesCallback cb = { s, race_net_service_found };
    if (ops->services(ops->ctx, &cb, a, b, most) == 0) return NULL;
    s->shown = s->count;
    if (s->shown == 0) {
        ops->free(ops->ctx, s->list);
        s->list = NULL;
    }
    return s->list;
}

uint8_t *race_net_services_first(const RaceNetServicesOps *ops, RaceNetServices *s, int16_t german)
{
    if (s->open != 0) {
        s->open = 0;
        ops->kill_service(ops->ctx);
    }
    if (s->list == NULL) race_net_services_list(ops, s, 0x2580, 0, 0xc);
    if (s->list == NULL) {
        ops->banner_wait(ops->ctx, german != 0 ? RACE_NET_NO_SERVICES_DE : RACE_NET_NO_SERVICES, 0x12c);
        return NULL;
    }
    s->count = 0;
    return s->list;
}

uint8_t *race_net_services_next(const RaceNetServicesOps *ops, RaceNetServices *s, int16_t german)
{
    if (s->list == NULL) return race_net_services_first(ops, s, german);
    if (s->open != 0) {
        s->open = 0;
        ops->kill_service(ops->ctx);
    }
    s->count = (int16_t)(s->count + 1);
    if (s->count >= s->shown) s->count = 0;
    return s->list + s->count * RACE_NET_SERVICE_SIZE;
}
