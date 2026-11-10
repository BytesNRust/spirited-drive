/* The SHA-256 of each family of the tables read from NFS_WIN.EXE (see family_sha in exe_tables_family.h): taken from the exe
 * whose size and SHA-256 are in game/sys/data_manifest.c, each when the extractor was proved equal to the table the port held. */
#ifndef TNFS_TESTS_EXE_TABLES_FIXTURE_H
#define TNFS_TESTS_EXE_TABLES_FIXTURE_H

static const struct { const char *family, *sha256; } EXE_FAMILY_SHA[] = {
    {"ai",     "776b7f32e93ad94c0b4c5130769fef4a1db7caf476628d955e0bdddcb867df81"},
    {"split",  "dc18d7d1101210db488e4feb68bbc52a361af285d72d76c3c24f6300bad207f0"},
    {"view",   "3a0a948420978810efc633912c110df4957d9080aa41839940260f73e5b5f5e6"},
    {"track",  "7900ebd4419316969c2e20ad14105bcbe6d7d5734f34305aec8d0fd439733c0f"},
    {"menu",   "f658162ce9e512d3d51ed52d352658732928fb44c4ea1aa86952c0d750faad44"},
    {"config", "b4e7b2d0ace9f330d1a3225b895678a2306f16606e1f18ae7343f8b2b3901cbc"},
};

#endif
