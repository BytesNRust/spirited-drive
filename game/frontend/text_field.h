/* One-line text entry of the front end. */
#ifndef TNFS_TEXT_FIELD_H
#define TNFS_TEXT_FIELD_H

/* 004626d0: edits text (at most max_length characters, buffer max_length+1)
 * centred on cx at y. free_text allows spaces and punctuation and keeps
 * lower case. Returns 1 when accepted with Enter, 0 when cancelled. */
int text_field_edit(char *text, int max_length, int cx, int y, int free_text);

#endif
