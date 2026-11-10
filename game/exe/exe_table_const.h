/* port: the tables read from the exe (exe_tables.h). In a build that keeps them in the source (the generated tables) they are
 * const; in a build that reads them from the player's NFS_WIN.EXE at start-up (TNFS_EXE_TABLES_FROM_EXE) they are filled once,
 * before anything reads them, so they cannot be const. */
#ifndef TNFS_EXE_TABLE_CONST_H
#define TNFS_EXE_TABLE_CONST_H

#ifdef TNFS_EXE_TABLES_FROM_EXE
#define EXE_TABLE_CONST
#else
#define EXE_TABLE_CONST const
#endif

#endif
