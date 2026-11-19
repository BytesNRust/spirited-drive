/* A project's copy of the generic art (editor/project.h project_art_refresh): the copy of an older Studio is replaced by the current one, silently, when it is
 * byte for byte one of the known old ones; one the author edited is left alone, with a note; an art= of another name is never looked at. */
#include "project.h"
#include "game/sys/sha256.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[256];

static void write_file(const char *name, const char *text, size_t size)
{
    char path[400];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(text, 1, size, f), fclose(f);
}

static int file_is(const char *name, const char *text, size_t size)
{
    char path[400], got[64];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(got, 1, sizeof got, f);
    fclose(f);
    return n == size && memcmp(got, text, size) == 0;
}

int main(void)
{
    snprintf(root, sizeof root, "/tmp/tnfs_art_refresh_XXXXXX");
    if (mkdtemp(root) == NULL) return fprintf(stderr, "no temp folder\n"), 2;
    char hex[65], note[160], generic[400], dir[400];
    Sha256 s;
    sha256_init(&s), sha256_update(&s, "OLD-ART", 7), sha256_hex(&s, hex);
    const char *const old[] = { hex, NULL };
    snprintf(generic, sizeof generic, "%s/bundled.fam", root);
    write_file("bundled.fam", "NEW-ART", 7);
    snprintf(dir, sizeof dir, "%s/p", root);
    mkdir(dir, 0777);
    char ini_path[400];
    snprintf(ini_path, sizeof ini_path, "p/project.ini");

    /* the copy of an older Studio: replaced, and the note says so */
    write_file(ini_path, "id=t\nname=t\nauthor=\nstem=EDIT\nart=GENERIC.FAM\nredistributable=1\n", 65);
    write_file("p/GENERIC.FAM", "OLD-ART", 7);
    CHECK_EQ_INT(project_art_refresh(dir, generic, old, note, sizeof note), 1);
    CHECK(file_is("p/GENERIC.FAM", "NEW-ART", 7));
    CHECK(strstr(note, "older Studio") != NULL);
    /* again: it is the current one now, nothing to do or say */
    CHECK_EQ_INT(project_art_refresh(dir, generic, old, note, sizeof note), 0);
    CHECK_EQ_INT(note[0], 0);

    /* one the author edited: left as it is, with a note */
    write_file("p/GENERIC.FAM", "MY-ART", 6);
    CHECK_EQ_INT(project_art_refresh(dir, generic, old, note, sizeof note), 0);
    CHECK(file_is("p/GENERIC.FAM", "MY-ART", 6));
    CHECK(strstr(note, "left as it is") != NULL);

    /* an art= that is not the project's copy of the generic: not looked at */
    write_file(ini_path, "id=t\nname=t\nauthor=\nstem=EDIT\nart=mine.fam\nredistributable=1\n", 62);
    write_file("p/GENERIC.FAM", "OLD-ART", 7);
    CHECK_EQ_INT(project_art_refresh(dir, generic, old, note, sizeof note), 0);
    CHECK(file_is("p/GENERIC.FAM", "OLD-ART", 7));
    CHECK_EQ_INT(note[0], 0);

    /* no bundled art (the build cannot read it), or no copy in the project: nothing */
    write_file(ini_path, "id=t\nname=t\nauthor=\nstem=EDIT\nart=GENERIC.FAM\nredistributable=1\n", 65);
    CHECK_EQ_INT(project_art_refresh(dir, NULL, old, note, sizeof note), 0);
    char gone[400];
    snprintf(gone, sizeof gone, "%s/p/GENERIC.FAM", root);
    remove(gone);
    CHECK_EQ_INT(project_art_refresh(dir, generic, old, note, sizeof note), 0);

    /* the list the editor carries holds the first Studio's art */
    CHECK(PROJECT_OLD_ARTS[0] != NULL && strlen(PROJECT_OLD_ARTS[0]) == 64 && PROJECT_OLD_ARTS[1] == NULL);
    char path[400];   /* the scratch folder goes away */
    const char *const names[] = { "p/project.ini", "p/GENERIC.FAM", "bundled.fam" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) snprintf(path, sizeof path, "%s/%s", root, names[i]), remove(path);
    rmdir(dir);
    rmdir(root);
    return TEST_REPORT();
}
