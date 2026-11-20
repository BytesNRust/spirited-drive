/* port (body shine, envmap): see car_envmap.h. */
#include "game/race/car_envmap.h"
#include "game/sys/data_path.h"
#include "game/sys/mod_set.h"

#include <stdlib.h>

void car_envmap_build(uint8_t *image)
{
    const char *env = getenv("TNFS_CAR_ENVMAP");
    if (env != NULL && env[0] != '\0' && car_envmap_file(env, image)) return;
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates("car_envmap.png", found);   /* the program's own data, the working directory, a development build's source tree */
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++)
        if (found[i][0] != '\0' && car_envmap_file(found[i], image)) return;
    car_envmap_make(image);
}

const uint8_t *car_envmap_image(void)
{
    static uint8_t image[CAR_ENVMAP_SIZE * CAR_ENVMAP_SIZE];
    static uint32_t made;   /* port (dep59): the mods' generation it was made under (a mod's car_envmap.png may replace it: mod_set.h); 0 never */
    if (made != mod_set_generation()) car_envmap_build(image), made = mod_set_generation();
    return image;
}
