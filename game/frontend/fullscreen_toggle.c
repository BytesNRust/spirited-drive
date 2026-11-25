/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see fullscreen_toggle.h. */
#include "game/frontend/fullscreen_toggle.h"

#include "game/frontend/menu_glue_private.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"

void fullscreen_toggle_now(void)
{
    const PortSettings was = port_settings_load();
    PortSettings now = was;
    now.fullscreen = was.fullscreen ? 0 : 1;
    if (!port_settings_save(&now)) port_log("fullscreen: port.cfg could not be saved\n");
    menu_glue_window_follows(&was);
}
