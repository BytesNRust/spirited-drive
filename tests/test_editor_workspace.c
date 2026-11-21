#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
/* The workspace (editor/workspace.h): cars/ and tracks/ made in a folder, the projects in them listed, where a folder dialog starts, the choice kept in a file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "workspace.h"

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; fprintf(stderr, "FAIL line %d: ", __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static char g_root[256];

static void make(const char *rel)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", g_root, rel);
    mkdir(path, 0777);
}

static void touch(const char *rel)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", g_root, rel);
    FILE *f = fopen(path, "w");
    if (f != NULL) fclose(f), (void)0;
}

/* a project is a folder with a file `project` in it; the fake knows no kinds apart */
static int present(const char *dir, int kind)
{
    char path[1024];
    (void)kind;
    snprintf(path, sizeof path, "%s/project", dir);
    struct stat st;
    return stat(path, &st) == 0;
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(g_root, sizeof g_root, "%s/ws-test-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(g_root) == NULL) return fprintf(stderr, "no scratch folder\n"), 1;
    char why[128] = "", sub[512];

    CHECK(!workspace_prepare("", why, sizeof why) && why[0] != '\0', "an empty path is refused: %s", why);
    snprintf(sub, sizeof sub, "%s/nothing-here", g_root);
    CHECK(!workspace_prepare(sub, why, sizeof why), "a folder that does not exist is refused");
    CHECK(workspace_prepare(g_root, why, sizeof why), "prepare: %s", why);
    CHECK(workspace_prepare(g_root, why, sizeof why), "prepare twice: %s", why);
    CHECK(workspace_path(g_root, WORKSPACE_KIND_CAR, NULL, sub, sizeof sub) && strstr(sub, "/cars") != NULL, "cars path");
    struct stat st;
    CHECK(stat(sub, &st) == 0 && S_ISDIR(st.st_mode), "cars/ was made");
    CHECK(workspace_path(g_root, WORKSPACE_KIND_TRACK, "b", sub, sizeof sub) && strstr(sub, "/tracks/b") != NULL, "track path");
    CHECK(!workspace_path(g_root, WORKSPACE_KIND_TRACK, "b", sub, 8), "a path that does not fit");

    make("cars/Zed"); touch("cars/Zed/project");
    make("cars/alpha"); touch("cars/alpha/project");
    make("cars/Beta"); touch("cars/Beta/project");
    make("cars/not-a-project");           /* no project file */
    make("cars/.hidden"); touch("cars/.hidden/project");
    touch("cars/loose-file");
    make("tracks/road"); touch("tracks/road/project");
    WorkspaceList list;
    workspace_scan(g_root, present, &list);
    CHECK(list.cars == 3 && strcmp(list.car[0], "alpha") == 0 && strcmp(list.car[1], "Beta") == 0 && strcmp(list.car[2], "Zed") == 0, "cars: %d, %s %s %s", list.cars, list.car[0], list.car[1], list.car[2]);
    CHECK(list.tracks == 1 && strcmp(list.track[0], "road") == 0, "tracks: %d", list.tracks);
    workspace_scan("", present, &list);
    CHECK(list.cars == 0 && list.tracks == 0, "no workspace lists nothing");
    snprintf(sub, sizeof sub, "%s/missing", g_root);
    workspace_scan(sub, present, &list);
    CHECK(list.cars == 0 && list.tracks == 0, "a missing folder lists nothing");

    char out[512];
    workspace_start_dir(g_root, WORKSPACE_KIND_CAR, "/a/b/proj", out, sizeof out);
    CHECK(strstr(out, "/cars") != NULL && strncmp(out, g_root, strlen(g_root)) == 0, "with a workspace, a car dialog starts in cars/: %s", out);
    workspace_start_dir(g_root, WORKSPACE_KIND_TRACK, "", out, sizeof out);
    CHECK(strlen(out) > 7 && strcmp(out + strlen(out) - 7, "/tracks") == 0, "a track dialog starts in tracks/: %s", out);
    workspace_start_dir("", WORKSPACE_KIND_TRACK, "/a/b/proj", out, sizeof out);
    CHECK(strcmp(out, "/a/b") == 0, "no workspace: the open project's parent: %s", out);
    workspace_start_dir("", WORKSPACE_KIND_TRACK, "", out, sizeof out);
    CHECK(out[0] == '\0', "nothing: the system's place");
    workspace_start_dir("", WORKSPACE_KIND_TRACK, "proj", out, sizeof out);
    CHECK(out[0] == '\0', "a bare name has no parent");

    char id[64];
    CHECK(workspace_id_from_name("  My New Track!! ", id, sizeof id, 32) && strcmp(id, "my-new-track") == 0, "id from a name: %s", id);
    CHECK(workspace_id_from_name("Zeta_9 / Road", id, sizeof id, 32) && strcmp(id, "zeta-9-road") == 0, "id with separators: %s", id);
    CHECK(workspace_id_from_name("A very long track name that goes on and on", id, sizeof id, 32) && strlen(id) <= 32 && id[strlen(id) - 1] != '-', "id is cut at 32: %s", id);
    CHECK(!workspace_id_from_name("!!!", id, sizeof id, 32) && id[0] == '\0', "a name with nothing usable makes no id");
    CHECK(workspace_id_from_name("Ünï 7", id, sizeof id, 32) && strcmp(id, "n-7") == 0, "non-ASCII letters are separators: %s", id);

    char file[512], dir[512] = "x";
    int aside = 1;
    snprintf(file, sizeof file, "%s/workspace.txt", g_root);
    workspace_load(file, dir, sizeof dir, &aside);
    CHECK(dir[0] == '\0' && aside == 0, "no file: no workspace, not put aside");
    CHECK(workspace_save(file, g_root, 0), "save");
    workspace_load(file, dir, sizeof dir, &aside);
    CHECK(strcmp(dir, g_root) == 0 && aside == 0, "the folder comes back: %s", dir);
    CHECK(workspace_save(file, "", 1), "save put aside");
    workspace_load(file, dir, sizeof dir, &aside);
    CHECK(dir[0] == '\0' && aside == 1, "put aside comes back with no folder");
    CHECK(!workspace_save("", g_root, 0), "no file name: not kept");

    {   /* the game repo's data/: a third setting in the same file, kept by the other saves, changed by its own */
        char rfile[512], repo[512], rdir[512];
        int raside = 0;
        snprintf(rfile, sizeof rfile, "%s/repo-settings.txt", g_root);
        workspace_load_repo(rfile, repo, sizeof repo);
        CHECK(repo[0] == '\0', "no file, no repo");
        CHECK(workspace_save(rfile, "/w/space", 1), "save");
        CHECK(workspace_save_repo(rfile, "/path/to/repo/data"), "save the repo");
        workspace_load_repo(rfile, repo, sizeof repo);
        workspace_load(rfile, rdir, sizeof rdir, &raside);
        CHECK(strcmp(repo, "/path/to/repo/data") == 0 && strcmp(rdir, "/w/space") == 0 && raside == 1, "the repo is set and the others kept: %s %s %d", repo, rdir, raside);
        CHECK(workspace_save(rfile, "/other", 0), "save again");
        workspace_load_repo(rfile, repo, sizeof repo);
        workspace_load(rfile, rdir, sizeof rdir, &raside);
        CHECK(strcmp(repo, "/path/to/repo/data") == 0 && strcmp(rdir, "/other") == 0 && raside == 0, "the workspace saves keep the repo: %s %s", repo, rdir);
        CHECK(workspace_save_repo(rfile, ""), "clear the repo");
        workspace_load_repo(rfile, repo, sizeof repo);
        workspace_load(rfile, rdir, sizeof rdir, &raside);
        CHECK(repo[0] == '\0' && strcmp(rdir, "/other") == 0, "cleared, the workspace stays");
    }

    if (g_fail != 0) return fprintf(stderr, "%d failed (scratch %s)\n", g_fail, g_root), 1;
    puts("workspace: ok");
    return 0;
}
