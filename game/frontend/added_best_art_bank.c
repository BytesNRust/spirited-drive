/* port (dep59 step 5, T7e2): the added track's Best Times bank put as a bank the menus draw from (added_best_art.h); apart from
 * added_best_art.c so the tests take that without ea_lib */
#include "game/frontend/added_best_art.h"
#include "game/frontend/ea_lib.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"

#include <stdlib.h>

int32_t added_best_art_bank(void)
{
    size_t size = 0, out_size = 0;
    uint8_t *coast = resource_load_art("tracksel/" ADDED_BEST_BASE ".qfs", &size), *out = NULL;
    char why[96] = "";
    const int made = coast != NULL && added_best_art_make(coast, size, &out, &out_size, why, sizeof why);
    resource_free(coast);
    if (!made) {
        port_log("added track art: Best Times' bank is not made (%s): Coastal's art instead\n", why);
        return 0;
    }
    return ea_bank_put(out, out_size, "added_bst");
}

int32_t added_best_art_records_bank(void)
{
    size_t size = 0, out_size = 0;
    uint8_t *coast = resource_load_art("tracksel/" ADDED_BEST_BASE ".qfs", &size), *out = NULL;
    char why[96] = "";
    const int made = coast != NULL && added_best_art_make_records(coast, size, &out, &out_size, why, sizeof why);
    resource_free(coast);
    if (!made) {
        port_log("car records: the screen's bank is not made (%s)\n", why);
        return 0;
    }
    return ea_bank_put(out, out_size, "records_bst");
}
