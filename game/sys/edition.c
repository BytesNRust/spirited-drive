/* port (dep59 step 4, T10): see edition.h. */
#include "game/sys/edition.h"

#include "formats/edition_ini.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdio.h>
#include <string.h>

int edition_set(const char *id)
{
    const char *const want = id != NULL ? id : "";
    if (want[0] != '\0' && !edition_id_ok(want)) {
        port_log("edition: \"%.40s\" is not an edition's id: not changed\n", want);
        return -1;
    }
    PortSettings s = port_settings_of_profile(PROFILE_RESTOMOD);
    if (strcmp(s.edition, want) == 0) return 0;
    snprintf(s.edition, sizeof s.edition, "%s", want);
    const PortExtras x = port_extras_of_profile(PROFILE_RESTOMOD);
    if (!port_mode_save(&s, &x)) {
        port_log("edition: restomod.cfg could not be saved\n");
        return -1;
    }
    port_log("mark:edition=%s\n", want[0] != '\0' ? want : "-");
    return 1;
}
