/* The order of the Studio's edits across its undo stacks (a car's card and its mask): each step to undo is numbered when its edit is made, from
 * one counter, so that one Undo can take back the newest of them whichever stack it is on (owner 2026-11-20: one history). Pure C. */
#ifndef TNFS_TRACK_EDITOR_EDIT_SEQ_H
#define TNFS_TRACK_EDITOR_EDIT_SEQ_H

#ifdef __cplusplus
extern "C" {
#endif

/* the next step's number: above every one given before, never 0 (0 is "no step") */
unsigned long edit_seq_next(void);

#ifdef __cplusplus
}
#endif

#endif
