/* port: see log_header.h. */
#include "game/sys/log_header.h"

#include "game/sys/log_path.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "platform/platform.h"

#include <stdlib.h>

void log_header_write(const char *data_root, const char *kind)
{
    char user[1200] = "", shown[1300];
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') home = getenv("USERPROFILE");   /* Windows */
    if (!platform_user_dir(user, sizeof user)) user[0] = '\0';
    log_path_for_report(data_root, home, user, shown, sizeof shown);
    port_log("profile: %s\n", port_profile_title((Profile)port_settings_profile()));
    port_log("data: %s: %s\n", kind != NULL ? kind : "unknown", shown);
}
