/* Studio's Race look (editor/car_race.c RACE_LOOK_ON, T5) against the game's own Restomod table (game/sys/port_profile.c): every key Studio writes is one the
 * player may set in Restomod (else the file's value is ignored and a switch does nothing), the values it writes with every switch on are the table's (so a run is
 * Restomod whole and the list cannot go stale in silence), and each key is one the game reads (written on Stock's values it changes them). A game test: the
 * Studio's own targets never link the game. */
#include "car_race.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const PortSettings restomod = port_profile_settings(PROFILE_RESTOMOD, port_settings_defaults(), SCREEN_4_3);
    const PortSettings stock = port_profile_settings(PROFILE_STOCK, port_settings_defaults(), SCREEN_4_3);
    int lines = 0;
    for (const char *p = RACE_LOOK_ON; *p != '\0';) {
        const char *nl = strchr(p, '\n');
        const size_t len = nl != NULL ? (size_t)(nl - p) : strlen(p);
        char line[64], key[64];
        snprintf(line, sizeof line, "%.*s", (int)len, p);
        snprintf(key, sizeof key, "%s", line);
        char *eq = strchr(key, '=');
        CHECK(eq != NULL);
        if (eq != NULL) *eq = '\0';
        CHECK(port_profile_key_free(PROFILE_RESTOMOD, key));   /* the file's value counts in Restomod */
        PortSettings on_restomod = restomod, on_stock = stock;
        port_profile_read_free(PROFILE_RESTOMOD, line, strlen(line), port_settings_take, &on_restomod);
        port_profile_read_free(PROFILE_RESTOMOD, line, strlen(line), port_settings_take, &on_stock);
        if (memcmp(&on_restomod, &restomod, sizeof restomod) != 0) fprintf(stderr, "%s is not Restomod's value\n", line);
        CHECK(memcmp(&on_restomod, &restomod, sizeof restomod) == 0);   /* Restomod's own value */
        if (memcmp(&on_stock, &stock, sizeof stock) == 0) fprintf(stderr, "%s changes nothing on Stock's values: a key the game does not read?\n", line);
        CHECK(memcmp(&on_stock, &stock, sizeof stock) != 0);           /* a key the game reads */
        ++lines;
        p += len + (nl != NULL);
    }
    CHECK(lines >= 4);
    const char *const switched[4] = { "body_shine=4", "car_colours=1", "car_soft_shadow=1", "texture_filter=1" };   /* the four switches' keys are in the list */
    for (int i = 0; i < 4; ++i) CHECK(strstr(RACE_LOOK_ON, switched[i]) != NULL);
    CHECK(port_profile_key_free(PROFILE_RESTOMOD, "render_scale") && port_profile_key_free(PROFILE_RESTOMOD, "extra_liveries"));
    return TEST_REPORT();
}
