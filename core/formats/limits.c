#include "formats/limits.h"

#include <string.h>

/* Order: by area. Every id is used by validate*.c and has a test in tests/test_validate.c. */
const Limit limit_table[] = {
    /* files that cannot be read at all */
    { "FILE.READABLE", "the file parses as the format its name promises", 0, "0043e4e0 (TRI), 004511a4 (FAM, CFM), 004a2520 (QFS), 004400f4 (PBS)", 1, 0 },
    { "FILE.NAME_LENGTH", "resource file name including its extension", 127, "00476268 (copied unchecked into a 128-byte stack buffer)", 1, 0 },
    { "FILE.FIXED_SIZE", "files the EXE reads or writes with a fixed byte count (PATHS.DAT 0x5f0, CONFIG.DAT 24402, SAV 0x5b6)", 0, "0042a188 (PATHS.DAT), 0042a010 (CONFIG.DAT), 0042a484 (SAV)", 1, 0 },

    /* QFS containers */
    { "QFS.PACK_CODE", "a stream whose second byte is 0xfb or 0x32 must carry a known codec in its first byte", 0, "004a261c, 00477a40 (abort)", 1, 0 },
    { "QFS.INPLACE_SIZE", "packed size must not exceed unpacked size + 0x400 (the loader unpacks in place)", 0x400, "004a2520", 1, 0 },
    { "QFS.INPLACE_ORDER", "RefPack output written never passes the input still unread, from the offset unpacked + 0x400 - packed", 0, "004a2520", 1, 0 },

    /* bitmaps */
    { "SHPI.SHAPE_TYPE", "entry type 0x7a, 0x7b, 0x78, 0x7e, 0x7f, 0x7d (0x68..0x6f alias 0x78..0x7f) or a palette 0x22, 0x24", 0, "00496f38, 004c7d70 (convertshapedepth aborts)", 1, 0 },

    /* tracks */
    { "TRI.SEGMENTS", "segments per track: 4 per segment plus one closing node must fit the 2400-node array", 599, "0043e4e0", 1, 0 },
    { "TRI.CIRCUIT_POW2", "a circuit's node count (4 x circuit segments) is a power of two (the loader derives a mask)", 0, "0043e4e0", 1, 1 },
    { "TRI.SCENERY_RECORDS", "scenery placements counted up to the -1 terminator; beyond 1000 (16000 bytes) the loader ignores them silently", 1000, "0043e4e0", 0, 0 },
    { "TRI.DESCRIPTOR_INDEX", "a placement's descriptor byte must be below the descriptor count (the loader does not check)", 0, "0043eca4", 1, 0 },
    { "TRI.SCENERY_SLOTS", "scenery_slots (header +0x16218) must be at least the placement count: per-record state is written unchecked", 0, "0040d6a0, 0040fc64, 0043eca4", 1, 0 },
    { "FAM.FILE_SIZE", "track FAM file size (a larger file is cut silently at the buffer)", 0x162400, "004510e8, 00451040", 1, 0 },
    { "FAM.CHILDREN", "a track FAM needs at least 3 children", 3, "0044ca3c (fatal handler 004c6f88)", 1, 0 },
    { "FAM.OBJECTS", "scenery objects (children of child 3): object i is written at 005150f0 + i x 0x138, 30 fit before the PDN table", 30, "004576f0, 004585e4", 1, 1 },
    { "FAM.BANKS_BACKGROUND", "bitmap banks of child 0", 48, "0044c36c", 1, 0 },
    { "FAM.BANKS_FOREGROUND", "bitmap banks of child 1", 63, "0044c36c", 1, 0 },
    { "FAM.BANKS_TOTAL", "child 0 banks + child 1 banks (the shape-bank registry holds 100 per race, 4 are taken)", 96, "00446f58, 0044c36c", 1, 0 },

    /* meshes */
    { "MESH.TEXTURE_NAME", "every texture reference of a mesh names an entry of a bitmap directory the mesh can see (a miss leaves a null texture; the CD ships 7 such references, so the original tolerates it)", 0, "00490b9a, 004732e4", 0, 0 },
    { "MESH.POLYGON_0_FLAG", "with part -1 the loader tests polygon 0's byte +1 bit 0x4 for every polygon", 0, "004917f7", 0, 0 },
    { "CFM.ELEMENT_INDEX", "the car mesh elements 'inside' and 'surface' sit at an index below 8 (else the loader hangs)", 7, "00457d40", 1, 0 },
    { "CFM.REQUIRED_NAME", "the car loader looks up fixed element names in the mesh and hangs when one is missing", 0, "00457d40", 1, 0 },

    /* car data */
    { "PBS.SIZE", "a PBS unpacks to 0x778 bytes", 0x778, "004400f4", 1, 0 },
    { "PBS.CHECKSUM", "the sum of the first 0x758 bytes must equal the word at +0x774, else the car's power is zeroed", 0, "004400f4", 1, 0 },
    { "PDN.SIZE", "a PDN unpacks to 0x1cc bytes", 0x1cc, "00457d40", 1, 0 },

    /* sound */
    { "BNK.DATA_BOUNDS", "a sample's data (offset + length x frame) lies inside the file (checked for banks the EXE opens)", 0, "0048ed30", 1, 0 },
    { "BNK.SOUND_NUMBERS", "banks loaded together use disjoint sound numbers (slots)", 0, "0048ed30", 1, 0 },

    /* text files */
    { "INFO.LINE_LENGTH", "a line of a track INFO file is read with fgets 0x7f: 126 characters at most", 126, "00429020", 1, 0 },
    { "INFO.DATA_LINES", "an INFO file has 29 data lines holding 33 values (a shorter file repeats its last line)", 29, "0043a370, 00429020", 0, 0 },
    { "INFO.COLOUR", "the two sky colour lines (data lines 9 and 10) hold three integers a b c packed a<<10 | b<<5 | c without masking: each 0..31", 31, "0043a370", 1, 0 },
    { "INFO.REQUIRED", "a track stem outside the built-in list needs its INFO file (else the loader reads a NULL row)", 0, "0043a370", 1, 0 },

    /* installation */
    { "MOD.CARSPECS_WRITABLE", "when the CARSPECS directory is writable the EXE overwrites By_R&T and zeroes every car's power", 0, "004400f4", 1, 0 },
};

const size_t limit_count = sizeof limit_table / sizeof limit_table[0];

const Limit *limit_find(const char *id)
{
    for (size_t i = 0; i < limit_count; ++i)
        if (strcmp(limit_table[i].id, id) == 0) return &limit_table[i];
    return NULL;
}
