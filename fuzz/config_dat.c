/* config.dat: the original's saved settings and records (a player's file, or one copied in) */
#include "fuzz/fuzz.h"
#include "game/frontend/config_dat.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static ConfigDat c;
    /* it takes exactly CONFIG_DAT_SIZE bytes: a short input is padded with zeros so the fields are reached */
    uint8_t *image = calloc(1, CONFIG_DAT_SIZE);
    if (image == NULL) return 0;
    memcpy(image, data, size < CONFIG_DAT_SIZE ? size : CONFIG_DAT_SIZE);
    (void)config_dat_parse(&c, image, CONFIG_DAT_SIZE);
    (void)config_dat_parse(&c, data, size);
    free(image);
    return 0;
}
