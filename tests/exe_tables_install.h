/* What a test that reads one of the tables of NFS_WIN.EXE (game/exe/exe_tables.h) does first: in a build whose tables come from
 * the player's exe (TNFS_EXE_TABLES_FROM_EXE) it installs them from original/NFS_WIN.EXE, and without that file it ends cleanly
 * with a note (the tables are not there to test); in the other builds the tables are in the source and it does nothing.
 * With TNFS_EXE_TABLES_POISON=1 the install publishes zeros: the tests that fail then are the ones that depend on the exe. */
#ifndef TNFS_TESTS_EXE_TABLES_INSTALL_H
#define TNFS_TESTS_EXE_TABLES_INSTALL_H

#include "game/exe/exe_tables.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(TNFS_EXE_TABLES_FROM_EXE) && defined(TNFS_SOURCE_DIR)
static void exe_tables_for_test(void)
{
    char err[512] = "";
    if (exe_tables_install(TNFS_SOURCE_DIR "/original", err, sizeof err) != 0) {
        fprintf(stderr, "skipped: the tables come from original/NFS_WIN.EXE and it is not usable here (%s)\n", err);
        exit(0);
    }
}
#else
static void exe_tables_for_test(void) {}
#endif

#endif
