#include "game/frontend/config.h"

#include "game/frontend/frontend.h"
#include "game/sys/resource.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

enum { PATH_MAX_LEN = 512 };

static const char CONFIG_NAME[] = "config.dat";

static ConfigDat config;

ConfigDat *config_current(void)
{
    return &config;
}

static int parse_file(uint8_t *data, size_t size, const char *origin)
{
    if (data == NULL) return 0;
    const ConfigResult r = config_dat_parse(&config, data, size);
    platform_free_file(data);
    if (r != CONFIG_OK) fprintf(stderr, "config: ignoring %s (%zu bytes, expected %d)\n", origin, size, CONFIG_DAT_SIZE);
    return r == CONFIG_OK;
}

/* The CD copy, where the original keeps it; the ISO upper-cases the path. */
static int load_from_cd(void)
{
    char path[PATH_MAX_LEN], upper[PATH_MAX_LEN];
    size_t size;
    snprintf(path, sizeof path, "%s%s", resource_dir(PATH_CONFIG), CONFIG_NAME);
    if (parse_file(platform_read_file(path, &size), size, path)) return 1;
    snprintf(upper, sizeof upper, "%s", path);
    for (char *c = upper; *c; c++) {
        if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
    }
    return parse_file(platform_read_file(upper, &size), size, upper);
}

/* 0042a010 */
int config_load(void)
{
    size_t size = 0;
    const int found = parse_file(platform_read_user_file(CONFIG_NAME, &size), size, "saved config.dat") || load_from_cd();
    if (!found) config_dat_defaults(&config, resource_german());
    config_dat_name(&config, frontend_player_name);
    /* TODO(0042a010): when 004fcdcc is 0 (unknown flag), the words at 0052ad26,
     * 0052ad28 and 0052ad2a that hold 1..3 are reset to 0 together with the
     * neighbouring words 0052ad2c..0052ad3a, back to their defaults. */
    return found ? 0 : 1;
}

/* 0042a504 */
void config_save(void)
{
    uint8_t out[CONFIG_DAT_SIZE];
    config_dat_set_name(&config, frontend_player_name);
    const size_t n = config_dat_serialise(&config, out, sizeof out);
    /* TODO(0046733c): the original first checks that the disk has room. */
    if (n == 0 || !platform_write_user_file(CONFIG_NAME, out, n)) fprintf(stderr, "config: cannot save config.dat\n");
}
