/* Front-end modal dialogs (EA's shared dialog code around 00465f4c). */
#ifndef TNFS_DIALOG_H
#define TNFS_DIALOG_H

/* 00465f4c: Yes/No box over the current screen. Returns 1 for Yes. */
int dialog_yes_no(const char *title);

/* 004667fc: "Please Enter Name" box with an 8-character field; edits name
 * (9 bytes) in place. Returns 1 when accepted. */
int dialog_enter_name(char *name);

#endif
