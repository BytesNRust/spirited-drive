#include "edit_seq.h"

static unsigned long last;   /* the window's one thread makes every edit */

unsigned long edit_seq_next(void)
{
    return ++last;
}
