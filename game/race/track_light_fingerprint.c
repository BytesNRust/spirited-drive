/* port: see track_light_fingerprint.h. */
#include "game/race/track_light_fingerprint.h"
#include "game/sys/sha256.h"

#include <string.h>

/* a field's bytes, with its length first so that two arrays cannot trade bytes */
static void part(Sha256 *s, const void *bytes, size_t n)
{
    const uint64_t len = bytes != NULL ? (uint64_t)n : 0;
    sha256_update(s, &len, sizeof len);
    if (bytes != NULL) sha256_update(s, bytes, n);
}

void track_light_fingerprint(const TrackNodeLight *light, char out[65])
{
    Sha256 s;
    sha256_init(&s);
    const size_t nodes = light->nodes;
    part(&s, &light->nodes, sizeof light->nodes);
    part(&s, light->data, nodes * TRACK_NODE_LIGHT_POINTS);
    part(&s, light->tunnel, nodes);
    part(&s, light->hall, nodes);
    part(&s, light->lamp_lift, nodes * TRACK_NODE_LIGHT_POINTS);
    for (int i = 0; i < light->lamp_n; i++) {   /* a lamp's node, colour and kind: its place in the world is the road's, not the file's */
        const TrackNodeLightLamp *const l = &light->lamps[i];
        part(&s, &l->node, sizeof l->node);
        part(&s, l->rgb, sizeof l->rgb);
        part(&s, &l->tunnel, sizeof l->tunnel);
    }
    const int look = light->look_set;
    part(&s, &look, sizeof look);
    if (look) {
        part(&s, &light->look_light, sizeof light->look_light);
        part(&s, light->look_cast, sizeof light->look_cast);
    }
    sha256_hex(&s, out);
}
