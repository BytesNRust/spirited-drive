#include "formats/slots.h"

#include <ctype.h>
#include <string.h>

/* 004af2fc..004af3dc as the CD has them (the other track names are variants of these). */
static const char *const track_stems[] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7", NULL };

/* 004c5660 (race cars, then cops) and 004c56a0 (traffic). */
static const char *const car_stems[] = {
    "TSUPRA", "LDIABL", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7", "TRAFFC",
    "COPMUST", "COPCAMARO", "COPCAPRICE", "COPVIC",
    "WAGON", "CRX", "VANDURA", "BMW", "SUNBIRD", "JETTA", "RODEO", "AXXESS", "PROBE", "JEEP", "LEMANS", "PICKUP", NULL
};

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    return *a == *b;
}

/* Whether `s` starts with `prefix`, ignoring case; *rest is what follows. */
static int ibegins(const char *s, const char *prefix, const char **rest)
{
    for (; *prefix; ++s, ++prefix)
        if (toupper((unsigned char)*s) != toupper((unsigned char)*prefix)) return 0;
    *rest = s;
    return 1;
}

const char *const *slot_stems(SlotKind kind)
{
    return kind == SLOT_TRACK ? track_stems : car_stems;
}

int slot_find(SlotKind kind, const char *name, char *stem, size_t cap)
{
    for (const char *const *s = slot_stems(kind); *s; ++s) {
        if (!ieq(*s, name)) continue;
        if (strlen(*s) + 1 > cap) return 0;
        strcpy(stem, *s);
        return 1;
    }
    return 0;
}

int slot_owns(SlotKind kind, const char *stem, const char *rel)
{
    const char *rest;
    if (!ibegins(rel, "SIMDATA/", &rest)) return 0;
    if (kind == SLOT_TRACK) {
        const char *after;
        if (ibegins(rest, "MISC/", &after) && ibegins(after, stem, &after))
            return ieq(after, ".TRI") || ieq(after, "INFO");
        static const char *const dirs[] = { "NTRACKFM/", "ETRACKFM/", "GTRACKFM/" };
        for (size_t d = 0; d < 3; ++d) {
            if (!ibegins(rest, dirs[d], &after) || !ibegins(after, stem, &after) || after[0] != '_') continue;
            const size_t n = strlen(after);
            return n > 5 && ieq(after + n - 4, ".FAM") && strchr(after, '/') == NULL;
        }
        return 0;
    }
    const char *after;
    if (ibegins(rest, "CARFAMS/", &after)) {
        if (after[0] == 'M' || after[0] == 'm') {
            const char *restricted;
            if (ibegins(after + 1, stem, &restricted) && ieq(restricted, ".CFM")) return 1;
        }
        if (ibegins(after, stem, &after)) return ieq(after, ".CFM") || ieq(after, ".PDN");
        return 0;
    }
    if (ibegins(rest, "CARSPECS/", &after) && ibegins(after, stem, &after)) return ieq(after, ".PBS");
    return 0;
}
