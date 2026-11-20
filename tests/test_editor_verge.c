/* The verge material of a track project (editor/project.h): project.ini's edge= is read (grass when it is not there, refused when it is not one of the
 * engine's six words), written back by project_set_edge without touching the other lines, and the card the build writes says it. */
#include "build.h"
#include "project.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[256];

static void put(const char *name, const char *text)
{
    char path[400];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(text, 1, strlen(text), f), fclose(f);
}

static void get(const char *name, char *out, size_t cap)
{
    char path[400];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *f = fopen(path, "rb");
    size_t n = f != NULL ? fread(out, 1, cap - 1, f) : 0;
    if (f != NULL) fclose(f);
    out[n] = '\0';
}

static const char TRACE[] = "{\n  \"version\": 1,\n  \"width\": 40.0,\n  \"points\": [\n    [0.000, 0.000],\n    [0.000, 100.000]\n  ]\n}\n";

static int open_edge(const char *ini, char *edge, size_t cap, char *why)
{
    put("project.ini", ini);
    put("trace.json", TRACE);
    Project p;
    Trace t = { NULL, 0, 0 };
    const int ok = project_open(root, &p, &t, why, 160);
    if (ok) snprintf(edge, cap, "%s", p.edge), free(t.points);
    return ok;
}

int main(void)
{
    snprintf(root, sizeof root, "/tmp/tnfs_verge_XXXXXX");
    if (mkdtemp(root) == NULL) return fprintf(stderr, "no temp folder\n"), 2;
    char edge[16], why[160] = "", text[512];

    /* read: grass when edge= is not there, the word when it is, a refusal naming the line otherwise */
    CHECK(open_edge("id=t\nname=t\nstem=EDIT\nart=GENERIC.FAM\n", edge, sizeof edge, why));
    CHECK(strcmp(edge, "grass") == 0);
    for (int i = 0; PROJECT_EDGES[i] != NULL; ++i) {
        snprintf(text, sizeof text, "id=t\nname=t\nstem=EDIT\nedge=%s\n", PROJECT_EDGES[i]);
        CHECK(open_edge(text, edge, sizeof edge, why));
        CHECK(strcmp(edge, PROJECT_EDGES[i]) == 0);
    }
    CHECK(!open_edge("id=t\nname=t\nstem=EDIT\nedge=lava\n", edge, sizeof edge, why));
    CHECK(strstr(why, "edge") != NULL);

    /* the six words of the engine, in the engine's order (core/formats/track_piece.c) */
    CHECK(strcmp(PROJECT_EDGES[0], "grass") == 0 && strcmp(PROJECT_EDGES[5], "water") == 0 && PROJECT_EDGES[6] == NULL);

    /* write: the line is replaced in place or added, nothing else changes, and doing it again changes nothing */
    put("project.ini", "# note\nid=t\nname=t\nauthor=me\nstem=EDIT\nart=GENERIC.FAM\nredistributable=1\n");
    CHECK(project_set_edge(root, "snow", why, sizeof why));
    get("project.ini", text, sizeof text);
    CHECK(strcmp(text, "# note\nid=t\nname=t\nauthor=me\nstem=EDIT\nart=GENERIC.FAM\nredistributable=1\nedge=snow\n") == 0);
    CHECK(project_set_edge(root, "snow", why, sizeof why));
    char again[512];
    get("project.ini", again, sizeof again);
    CHECK(strcmp(text, again) == 0);
    CHECK(project_set_edge(root, "sand", why, sizeof why));
    get("project.ini", text, sizeof text);
    CHECK(strcmp(text, "# note\nid=t\nname=t\nauthor=me\nstem=EDIT\nart=GENERIC.FAM\nredistributable=1\nedge=sand\n") == 0);
    put("project.ini", "id=t\nedge=gravel\nstem=EDIT");   /* the line in the middle, the file without a last newline */
    CHECK(project_set_edge(root, "water", why, sizeof why));
    get("project.ini", text, sizeof text);
    CHECK(strcmp(text, "id=t\nedge=water\nstem=EDIT") == 0);
    CHECK(!project_set_edge(root, "lava", why, sizeof why));   /* refused, the file as it was */
    get("project.ini", text, sizeof text);
    CHECK(strcmp(text, "id=t\nedge=water\nstem=EDIT") == 0);

    /* the card of the build carries the word, and the game's parser accepts it */
    Project p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "t"), snprintf(p.stem, sizeof p.stem, "EDIT"), snprintf(p.edge, sizeof p.edge, "sand");
    uint8_t card[2048];
    size_t len = 0;
    ModInfo info;
    CHECK(build_card(&p, card, sizeof card, &len, &info, why, sizeof why));
    CHECK(strstr((const char *)card, "\nedge=sand\n") != NULL && strstr((const char *)card, "\nedge=grass\n") == NULL);
    snprintf(p.edge, sizeof p.edge, "lava");
    CHECK(!build_card(&p, card, sizeof card, &len, &info, why, sizeof why));

    return TEST_REPORT();
}
