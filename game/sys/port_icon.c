/* port: see port_icon.h. */
#include "game/sys/port_icon.h"

#include "formats/ico.h"
#include "game/platform/platform.h"
#include "game/sys/resource.h"

#include <stdio.h>
#include <stdlib.h>

void port_icon_apply(void)
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
    if (rgba != NULL && ico_decode(file, size, rgba, (size_t)ICO_MAX_SIDE * ICO_MAX_SIDE * 4, &w, &h))
        platform_set_window_icon(rgba, w, h);
    free(rgba);
    platform_free_file(file);
}
