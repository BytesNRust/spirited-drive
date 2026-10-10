/* Slot names and the files a slot is made of. */
#include "formats/slots.h"
#include "test.h"

#include <string.h>

int main(void)
{
    char stem[16];
    CHECK(slot_find(SLOT_TRACK, "tr3", stem, sizeof stem) && strcmp(stem, "TR3") == 0);
    CHECK(slot_find(SLOT_TRACK, "Al1", stem, sizeof stem) && strcmp(stem, "AL1") == 0);
    CHECK(!slot_find(SLOT_TRACK, "TR8", stem, sizeof stem));            /* TR8_R01.FAM is a leftover */
    CHECK(!slot_find(SLOT_TRACK, "AL22", stem, sizeof stem));
    CHECK(slot_find(SLOT_CAR, "dviper", stem, sizeof stem) && strcmp(stem, "DVIPER") == 0);
    CHECK(slot_find(SLOT_CAR, "Wagon", stem, sizeof stem) && strcmp(stem, "WAGON") == 0);
    CHECK(!slot_find(SLOT_CAR, "LDIABLO", stem, sizeof stem));          /* a leftover model */
    CHECK(!slot_find(SLOT_CAR, "DVIPER", stem, 4));                     /* too small a buffer */
    size_t tracks = 0, cars = 0;
    for (const char *const *s = slot_stems(SLOT_TRACK); *s; ++s) ++tracks;
    for (const char *const *s = slot_stems(SLOT_CAR); *s; ++s) ++cars;
    CHECK_EQ_INT(tracks, 16);
    CHECK_EQ_INT(cars, 25);

    CHECK(slot_owns(SLOT_TRACK, "TR3", "SIMDATA/MISC/TR3.TRI"));
    CHECK(slot_owns(SLOT_TRACK, "TR3", "simdata/misc/tr3.tri"));
    CHECK(slot_owns(SLOT_TRACK, "TR3", "SIMDATA/MISC/TR3INFO"));
    CHECK(slot_owns(SLOT_TRACK, "TR3", "SIMDATA/NTRACKFM/TR3_001.FAM"));
    CHECK(slot_owns(SLOT_TRACK, "TR3", "SIMDATA/ETRACKFM/TR3_T01.FAM"));
    CHECK(slot_owns(SLOT_TRACK, "TR3", "SIMDATA/GTRACKFM/TR3_M01.FAM"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "SIMDATA/MISC/TR4.TRI"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "SIMDATA/NTRACKFM/TR33_001.FAM"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "SIMDATA/NTRACKFM/TR3_001.TXT"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "SIMDATA/NTRACKFM/sub/TR3_001.FAM"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "SIMDATA/CARFAMS/TR3.TRI"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "../SIMDATA/MISC/TR3.TRI"));
    CHECK(!slot_owns(SLOT_TRACK, "TR3", "SIMDATA/MISC/TR3.TRI.bak"));

    CHECK(slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/CARFAMS/DVIPER.CFM"));
    CHECK(slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/CARFAMS/MDVIPER.CFM"));
    CHECK(slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/CARFAMS/DVIPER.PDN"));
    CHECK(slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/CARSPECS/DVIPER.PBS"));
    CHECK(!slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/CARFAMS/MDVIPER.PDN"));      /* only the CFM has an m copy */
    CHECK(!slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/CARSPECS/CZR1.PBS"));
    CHECK(!slot_owns(SLOT_CAR, "DVIPER", "SIMDATA/SOUNDBNK/Vipersw.BNK"));
    CHECK(!slot_owns(SLOT_CAR, "MRX7", "SIMDATA/CARFAMS/MMRX7.CFM.old"));
    CHECK(slot_owns(SLOT_CAR, "MRX7", "SIMDATA/CARFAMS/MMRX7.CFM"));
    return TEST_REPORT();
}
