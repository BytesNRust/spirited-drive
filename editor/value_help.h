/* One line on what each road value of a key is for (owner feedback T9), shown on hover over its row's label in the profile strip and in Road profile. */
#ifndef EDITOR_VALUE_HELP_H
#define EDITOR_VALUE_HELP_H

#ifdef __cplusplus
extern "C" {
#endif

/* the help of a TRACE_KEY_ bit (both sides of a value read the same); NULL for a bit that is not a road value */
const char *road_value_help(unsigned bit);

#ifdef __cplusplus
}
#endif

#endif
