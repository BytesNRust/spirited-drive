/* The player's .cfg files: port.cfg and the modes' files (settings and extras), controls.cfg */
#include "fuzz/fuzz.h"
#include "game/sys/port_controls.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *text = (const char *)data;
    (void)port_settings_parse(text, size);
    (void)port_extras_parse(text, size);
    (void)port_controls_parse(text, size);
    return 0;
}
