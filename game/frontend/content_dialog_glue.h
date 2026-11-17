/* port (dep59 step 4b, T6): the CONTENT screen's dialogs (edition_name_box.h) drawn over the screen as it is: the screen kept as the
 * dialog opens, shown darkened 45% under the panel, put back as it closes. For the CONTENT screen's glue (T7) to call. */
#ifndef TNFS_CONTENT_DIALOG_GLUE_H
#define TNFS_CONTENT_DIALOG_GLUE_H

#include <stdint.h>

/* content_install_prompt's: 1 Yes (or OK in the standard edition, `edition` NULL), 0 No or Escape */
int32_t glue_content_install_prompt(const char *name, const char *version, const char *edition);

/* edition_name_box's: 1 with `name` (EDITION_NAME_MAX + 1) the new name, 0 left as it was */
int32_t glue_edition_name_box(const char *title, char *name);

#endif
