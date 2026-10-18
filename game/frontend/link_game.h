/* The link game over the network (glue, no oracle): the race_net functions
 * (game/race/race_net*.c) and the network menus' calls run on the transport
 * (game/net/net_runtime.c) with the original's globals kept here. The words
 * the menus already share stay in menu_words: 0052b312 link_game (playing a
 * link game), 0052b314 link_lost (dropped), 00524e7b mode_menu (this machine
 * hosts), 0052b31a redraw, 004fbb0c intro_seen (this machine's side: 1 the
 * host), 004fbb08 single (the slot the host gave this machine). */
#ifndef TNFS_LINK_GAME_H
#define TNFS_LINK_GAME_H

#include "game/net/net_runtime.h"
#include "game/race/race_net.h"
#include "game/race/race_net_games.h"
#include "game/race/race_net_players.h"
#include "game/race/race_net_services.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum { LINK_INFOS = 8 * RACE_NET_INFO };

typedef struct LinkGame {
    NetRuntime rt;
    NetSessionRecord session;              /* 0050a260 */
    int16_t view;                          /* 0052b316: only watching */
    int16_t unknown_52b302, unknown_52b318;
    int16_t unknown_4bc6ac, unknown_4bc6dc;
    int32_t unknown_4bcb94, joined;        /* 004bcb94, 0050a5a8 */
    RaceNetCounts counts;                  /* 004bc6d8 / 004bc6da */
    int16_t flags_a[RACE_NET_FLAGS_A], flags_b[RACE_NET_FLAGS_B];  /* 0050a5ac, 0050a5c2 */
    uint8_t state[RACE_NET_STATE];         /* 004f8164 */
    uint8_t incoming[RACE_NET_STATE];      /* 004f81d8 */
    uint8_t settings[RACE_NET_SETTINGS];   /* 004f8250 */
    int16_t stand_ins[1 + RACE_NET_PLAYERS];  /* 004f829e: the kind word, then 004f82a0's eight */
    uint8_t session_name[RACE_NET_SESSION_NAME];  /* 0050a320 */
    int32_t unknown_50a360;
    RaceNetQueue queue;                    /* 0050a368..: its words past the asks copied in for each call */
    uint32_t links[RACE_NET_LINK_ROOM];    /* 0050a578 */
    RaceNetLink entries[RACE_NET_ENTRIES]; /* 0050a5dc */
    NetPacketHandle handles[RACE_NET_ENTRIES];    /* the transport's view of each entry */
    uint8_t peers[RACE_NET_ENTRIES][NET_ADDRESS];
    uint8_t tags[RACE_NET_TAGS];           /* 0050a978 */
    int32_t pings[RACE_NET_PINGS_CLEARED]; /* 004f8108 */
    int32_t unknown_4f8138[RACE_NET_WORDS_4F8138];
    int32_t deadline;                      /* 004f8158 */
    int16_t unknown_52b2b4[RACE_NET_SET_WORDS];
    uint8_t infos[LINK_INFOS];             /* 004fb7f8 */
    uint8_t setup[RACE_NET_SETUP];         /* 004fb7bc */
    uint8_t chat[RACE_NET_CHAT_LINES][RACE_NET_CHAT_LINE];  /* 004b7a30 */
    int16_t chat_line, chat_arrived;       /* 004b905f, 004b9062 */
    RaceNetServices services;              /* 004bc6c0.. */
    RaceNetGamesState games;               /* 004bc6c4.. */
    RaceNetPlayers players;                /* 004bc6cc.. */
    uint8_t game_guid[NET_GUID];           /* 004bc6b0 */
    InputQueue *queues;                    /* 004faa7c: the race's, else spare ones */
    PlayerRecord *records;                 /* 0051cc54: the race's, else spare ones */
    int32_t racers;                        /* 004fb7dc */
    void (*stand_in)(int16_t player);      /* 0041019c, NULL outside the race */
    /* the race's kinds 2 (004395cc) and 3 (004109e4), NULL outside the race */
    void (*race_message)(int16_t kind, uint32_t link, const uint8_t *data, int32_t size);
    int ready;                             /* link_game_init ran */
} LinkGame;

extern LinkGame link_game;

/* Before the menus (main.c): the UDP port to bind (0: the default) and the
 * direct hosts ("a.b.c.d:port", NULL-terminated, or NULL) a search also asks */
void link_game_init(uint16_t port, const char *const *hosts);

/* 00475c14(0) while the network runs: the OS events, then the transport's services */
void link_service(void);

/* The network menus' calls */
int32_t link_services_found(void);                 /* 00442514 not NULL */
const char *link_service_first(void);              /* 00442514 */
const char *link_service_next(void);               /* 004425a0 */
RaceNetGames *link_games(int32_t timeout);         /* 00442684 */
RaceNetPlayers *link_players(const uint8_t *address);  /* 004428fc */
int32_t link_host(const char *name);               /* 00442970 */
int32_t link_connect(const uint8_t *address);      /* 00442ad0 */
void link_give_up(void);                           /* 00446220 */
void link_close(void);                             /* 004373c4's 00446220 when the network game ends */

/* The race's messages to the others (00445e00) and a slot dropped (004463ec) */
void link_send(uint32_t link, const uint8_t *data, int32_t size);
void link_drop(int16_t slot);

#endif
