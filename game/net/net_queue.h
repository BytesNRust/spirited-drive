/* The transport's queues (0049c6a8..0049cbb4): a singly linked list, each
 * node's first word its next, with a count, the head and tail, the order's
 * compare and a mutex. */
#ifndef TNFS_NET_QUEUE_H
#define TNFS_NET_QUEUE_H

#include <stdint.h>

typedef struct NetNode {
    struct NetNode *next;  /* +0 */
} NetNode;

typedef struct {
    uint32_t count;        /* +0 */
    uint32_t flags;        /* +4: bit 0 set by every change */
    NetNode *head;         /* +8 */
    NetNode *tail;         /* +0xc */
} NetQueue;

enum { NET_QUEUE_CHANGED = 1 };

/* 0049c714 (cdecl (queue, node), from 0049c9d8, 0049cc10, 0049cc74 and
 * 0049cd48, under the queue's mutex): the node taken out. Nothing (0) with
 * no node or the count 0. The head: count - 1, head and tail NULL when it
 * was the tail, else head its next. Else the node after the head whose
 * next it is (the walk stops at a NULL next): none, 0; else count - 1, that
 * one's next the node's, and the tail it when the node was the tail. Found:
 * the node's next NULL, the flags' bit 0 set, 1. */
int32_t net_queue_remove(NetQueue *q, NetNode *n);

/* 0049c6a8 (cdecl (to, value, n), 10 callers: the packets' words in network
 * order): value's low n bytes to to[0..n-1], the last byte the lowest (above
 * 4, zeros first); n at most 0 writes nothing. */
void net_put_big(uint8_t *to, uint32_t value, int32_t n);

#endif
