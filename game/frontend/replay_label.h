/* port (dep59 step 3): what the menus' replay list says of a Replay.rpx, a race with an added car (race_replay_image.h's TNFSCAR1 block):
 * its name when the cars it needs are here as they were; which mod it needs, or which one changed since, when not (such a one is not
 * played). */
#ifndef TNFS_REPLAY_LABEL_H
#define TNFS_REPLAY_LABEL_H

#include "game/race/race_replay_image.h"

#include <stddef.h>

/* `name` without its extension: "NAME" for REPLAY_CARS_OK, "NAME - needs <id>" for MISSING, "NAME - <id> changed" for CHANGED,
 * "NAME - not a replay" for BAD; cut to `cap` with its NUL */
void replay_label(const char *name, ReplayCars status, const char *id, char *out, size_t cap);
/* 1 when a replay of that status plays */
int replay_label_plays(ReplayCars status);
/* 1 when `name` is a Replay.rpx's kind of file: it ends in .RPX, in any case */
int replay_label_added(const char *name);

#endif
