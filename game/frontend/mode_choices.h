/* port (dep59 step 4): the choices of Game Mode's selector (notes/dep59-step4-editions-plan.md, section 6, Task 9): 0 Stock '96,
 * 1 Restored '96, 2 Restomod '26 (the standard edition), then the editions in editions/ by id (edition_file.h). An edition that cannot be
 * used (its file damaged, no car or no track there, a name too wide for the selector) keeps its place with the reason as its line, and
 * cannot be applied. */
#ifndef TNFS_MODE_CHOICES_H
#define TNFS_MODE_CHOICES_H

#include "formats/edition_ini.h"

enum {
    MODE_CHOICES_MODES = 3,                     /* Stock, Restored, Restomod */
    MODE_CHOICES_EDITIONS = 64,                 /* the most editions listed */
    MODE_CHOICES_MAX = MODE_CHOICES_MODES + MODE_CHOICES_EDITIONS,
    MODE_CHOICES_TITLE_NUM = 33, MODE_CHOICES_TITLE_DEN = 20,   /* the title's scale (the selector's letters) */
    MODE_CHOICES_TITLE_MAX_PX = 320,            /* the widest name: with the arrows, 428 px of the 448 the row has */
    MODE_CHOICES_LINE_CAP = 96
};

typedef struct {
    char id[EDITION_ID_MAX + 1];                /* an edition's id; "" for the three modes */
    char title[EDITION_ID_MAX + 1];             /* the name the selector shows */
    char line1[MODE_CHOICES_LINE_CAP], line2[MODE_CHOICES_LINE_CAP];   /* the lines under it ("" none) */
    int profile;                                /* the mode it runs in (port_profile.h's Profile) */
    int usable;                                 /* 0: shown with its reason, not applied */
} ModeChoice;

/* how many choices there are: the three modes and the editions in editions/ */
int mode_choices_count(void);
/* choice `index` into `out`: 1; 0 when there is none */
int mode_choices_get(int index, ModeChoice *out);
/* the choice in use: the mode's index, or in Restomod the edition in use's (2, the standard, when edition= is none or cannot be used) */
int mode_choices_in_use(void);
/* choice `index` made the one in use: its mode selected, and Restomod's edition= set to its id ("" for Restomod '26); 1 done, 0 when it
 * cannot be applied (none, or not usable) */
int mode_choices_apply(int index);
/* how a title is measured: text_width_scaled (game/sys/text.h, in the game itself: Game Mode's glue sets it). NULL, the default: not
 * measured, the characters alone */
void mode_choices_measure(int (*width)(const char *text, int num, int den));
/* 1 when `name` may be an edition's title in the selector: 1..EDITION_NAME_MAX characters and under MODE_CHOICES_TITLE_MAX_PX at the
 * title's scale (mode_choices_measure's) */
int mode_choices_name_fits(const char *name);
/* the rule itself, for a name that is `width_px` wide at the title's scale */
int mode_choices_name_fits_width(const char *name, int width_px);

#endif
