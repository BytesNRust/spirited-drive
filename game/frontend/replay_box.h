/* The "Save replay as" box. */
#ifndef TNFS_REPLAY_BOX_H
#define TNFS_REPLAY_BOX_H

#include "game/frontend/message_banner.h"
#include "game/frontend/save_box.h"

#include <stdint.h>

/* the original's path buffer (esp + 0x100, below the two pictures): dir + name + ".rpl" must fit */
enum { REPLAY_BOX_PATH = 0x50 };

/* 0042a9f8: as 0042a680 (save_box_show) with a 0x1e0 x 0x64 box at 0x50,
 * 0xbe, "Save replay as" ("Wiederholung speichern als") centred at 0xf0,
 * 0xa, and no default picture selected before the screen is put back. The
 * name is cut at its first '.', or at 15 characters, before the field.
 * Taken: dir + name + ".rpl" (the dir 00524418, paths.dat's slot 8), the
 * name into config.dat's 00524e2b (sprintf'd with the name as the format,
 * as 0042a680 does); a file that exists named REPLAY or REPLAY1..3
 * (A..Z folded) gets "Cannot replace system replay" for 100 ticks and is
 * not saved, any other asks "Replace replay ?" (no removal); ops->save is
 * 0042a5e8 (game_file_copy_replay) here; "Replay saved" for 100 ticks when
 * it worked. Sound 0x48 when the name is given up or the file is new. The
 * name needs 16 bytes. */
void replay_box_show(char *name, int german, const MessageBannerInputs *in, uint8_t *config, const char *dir,
                     const SaveBoxOps *ops);

#endif
