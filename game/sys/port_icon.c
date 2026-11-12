/* port: see port_icon.h. */
#include "game/sys/port_icon.h"

#include "formats/ico.h"
#include "game/platform/platform.h"
#include "game/sys/port_icon_ours.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"

#include <stdio.h>
#include <stdlib.h>

/* the CD's NFSICONN.ICO, as before our own icon */
static void apply_cd_icon(void)
{
    static const char *const NAMES[] = { "NFSICONN.ICO", "nfsiconn.ico", "NFSICONN.ico" };
    uint8_t *file = NULL;
    size_t size = 0;
    for (size_t i = 0; file == NULL && i < sizeof NAMES / sizeof NAMES[0]; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", resource_root(), NAMES[i]);
        file = platform_read_file(path, &size);
    }
    if (file == NULL) return;
    uint8_t *const rgba = malloc((size_t)ICO_MAX_SIDE * ICO_MAX_SIDE * 4);
    int w = 0, h = 0;
    if (rgba != NULL && ico_decode(file, size, rgba, (size_t)ICO_MAX_SIDE * ICO_MAX_SIDE * 4, &w, &h)) {
        const PlatformIconImage image = { rgba, w, h };
        platform_set_window_icon(&image, 1);
        port_log_detail("icon: the CD's %s, %dx%d\n", NAMES[0], w, h);
    }
    free(rgba);
    platform_free_file(file);
}

void port_icon_apply(void)
{
    if (!port_icon_sets_window()) return;   /* macOS: the bundle's .icns is the Dock icon */
    PortIconImage ours[PORT_ICON_SIZES];
    const int n = port_icon_ours(ours);
    if (n == 0) {
        apply_cd_icon();
        return;
    }
    PlatformIconImage images[PORT_ICON_SIZES];
    for (int i = 0; i < n; i++) images[i] = (PlatformIconImage){ ours[i].rgba, ours[i].side, ours[i].side };
    platform_set_window_icon(images, n);
    port_log_detail("icon: ours, %d sizes from %d px\n", n, ours[0].side);
    port_icon_ours_free(ours, n);
}
