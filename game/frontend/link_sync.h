/* The link game's glue between its files (link_game.c, link_messages.c,
 * link_session.c): the transport's calls on the entries and the handlers
 * the transport calls back. */
#ifndef TNFS_LINK_SYNC_H
#define TNFS_LINK_SYNC_H

#include "game/frontend/link_game.h"

#include <stdint.h>

/* the words read and written as the original's addresses say (menu_words for the shared ones) */
int16_t link_word(const uint8_t *at);                 /* a little-endian short */
const char *link_text(uint32_t at);                   /* 00467214's text by its exe address */
void link_message(uint32_t text, int32_t ticks);      /* 00467214 */
uint8_t link_esc(void);                               /* 004c693d: Esc down */
int32_t link_clock(void);                             /* 004bb8e0 */

uint32_t link_of(int i);                              /* entry i's link (its exe address) */
int link_index(uint32_t link);                        /* the entry of a link, -1 none */
void link_entries_sync(void);                         /* each entry's +0 from its handle */
int32_t link_join(const RaceNetLink *entry);          /* 00479a6c(entry, 0x20, 0, 0xfa, 6) */
void link_close_entry(uint32_t link);                 /* 0047b250 */
void link_forget(const uint8_t *name);                /* 004799a8(0050a260, name) */
void link_shut_down(void);                            /* 0047ab80 */
int32_t link_write(uint32_t link, const uint8_t *data, int32_t size);  /* 0047b17c */
int16_t link_slot_of(uint32_t link);                  /* 0044659c */

/* the transport's callbacks */
int32_t link_receive(uint32_t link, const uint8_t *data, int32_t size);  /* 00446058 */
int32_t link_lost_link(int32_t kind, uint32_t link);                      /* 004460cc */
int32_t link_join_request(const uint8_t *record);                         /* 00442ec0 */

void link_reset(void);                                /* 0044655c */
void link_release(void);                              /* 00442dec */
void link_deadline(int32_t ticks);                    /* 00410b7c */
int32_t link_wait_step(void);                         /* 00410bac */

#endif
