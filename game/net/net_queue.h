/* The transport's queues (0049c6a8..0049cbb4): a singly linked list, each
 * node's first word its next, with a count, the head and tail, the order's
 * compare and a mutex. */
#ifndef TNFS_NET_QUEUE_H
#define TNFS_NET_QUEUE_H

#include <stdint.h>

typedef struct NetNode {
    struct NetNode *next;  /* +0 */
} NetNode;

/* The order's key of a node (0049c954 keeps them rising) */
typedef uintptr_t (*NetQueueKey)(const NetNode *n, uintptr_t arg);

typedef struct {
    uint32_t count;        /* +0 */
    uint32_t flags;        /* +4: bit 0 set by every change */
    NetNode *head;         /* +8 */
    NetNode *tail;         /* +0xc */
    NetQueueKey key;       /* +0x10 */
    uintptr_t key_arg;     /* +0x14: the key's second argument */
    void *mutex;           /* +0x18 */
} NetQueue;

typedef struct {
    void *ctx;
    void *(*create)(void *ctx);            /* 00476c68: CreateMutexA(0, 0, 0) */
    void (*lock)(void *ctx, void *mutex);    /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, void *mutex);  /* 00476cfc (cdecl) */
    void (*close)(void *ctx, void *mutex);   /* 00476c58 (cdecl): CloseHandle */
} NetMutexOps;

enum { NET_QUEUE_CHANGED = 1 };

/* 0049c714 (cdecl (queue, node), from 0049c9d8, 0049cc10, 0049cc74 and
 * 0049cd48, under the queue's mutex): the node taken out. Nothing (0) with
 * no node or the count 0. The head: count - 1, head and tail NULL when it
 * was the tail, else head its next. Else the node after the head whose
 * next it is (the walk stops at a NULL next): none, 0; else count - 1, that
 * one's next the node's, and the tail it when the node was the tail. Found:
 * the node's next NULL, the flags' bit 0 set, 1. */
int32_t net_queue_remove(NetQueue *q, NetNode *n);

/* 0049c6f0 (cdecl (node, arg)): the default key, the node's own address */
uintptr_t net_queue_key_self(const NetNode *n, uintptr_t arg);

/* 0049c7a8 (cdecl (queue, key, arg), from 00478f0c, 0047ab10, 0047ac18 and
 * 0048161c): a new mutex (00476c68) to +0x18, count, flags, head and tail
 * 0, the key (0049c6f0 for NULL) and its argument. */
void net_queue_init(const NetMutexOps *ops, NetQueue *q, NetQueueKey key, uintptr_t arg);

/* 0049c954 (cdecl (queue, node), from 0047a50c, 0047a82c, 0047ac18 and
 * 0047b17c): under the mutex (+0x18, read again for the unlock), a node (not
 * NULL) put before the first whose key is not below its own (the key and
 * its argument read again for every call), count + 1 (read after the
 * node's key), the tail when none is, the flags' bit 0 set. */
void net_queue_insert(const NetMutexOps *ops, NetQueue *q, NetNode *n);

/* 0049c804 (cdecl (queue, node), from 00478f0c, 0047a308, 0047a434,
 * 0047ac18, 0047b250 and 0048161c): under the mutex (+0x18, read again for
 * the unlock), a node (not NULL) put first: its next the head, count + 1,
 * the tail when its next is NULL, the flags' bit 0 set. */
void net_queue_push_front(const NetMutexOps *ops, NetQueue *q, NetNode *n);

/* 0049c848 (cdecl (queue, node); no entry in functions.tsv): under the
 * mutex (+0x18, read again for the unlock), a node (not NULL) put last: the
 * tail read, the node's next NULL, the tail it, count + 1, the old tail's
 * next it (or the head with none), the flags' bit 0 set. */
void net_queue_push_back(const NetMutexOps *ops, NetQueue *q, NetNode *n);

/* 0049c894 (cdecl (queue), from 0047a308 and 0047b17c): under the mutex
 * (+0x18, read again for the unlock), the head taken: it the tail, head and
 * tail NULL, else the head its next; count - 1; its next NULL. The flags'
 * bit 0 set even with none. The node, or NULL. */
NetNode *net_queue_pop_front(const NetMutexOps *ops, NetQueue *q);

/* 0049c8e8 (cdecl (queue), no direct callers): under the mutex (+0x18, read
 * again for the unlock), the tail taken: count - 1; it the head, head and
 * tail NULL; else the tail walked from the head (the tail word itself the
 * walk's place) to the node before it, whose next is NULLed; its next NULL.
 * The flags' bit 0 set even with none. The node, or NULL. The tail must be
 * on the chain from the head (the original faults past its end). */
NetNode *net_queue_pop_back(const NetMutexOps *ops, NetQueue *q);

/* 0049c9d8 (cdecl (queue, node), from 004790b0, 0047a434, 0047ac18 and
 * 0047b250): 0049c714 under the mutex (+0x18, read again for the unlock),
 * its answer. Port guard: a NULL node answers 0 (the original returns ebx as
 * it came; its callers pass nodes). */
int32_t net_queue_remove_locked(const NetMutexOps *ops, NetQueue *q, NetNode *n);

/* 0049ca1c (cdecl (queue, node), from 004790b0, 004795fc and 0047a068):
 * under the mutex (+0x18, read again for the unlock), the node's place from
 * the head (0 first), -1 for none or NULL. The head is read, then the key
 * (+0x10, read once; NULL none) of the node; the walk stops at the node, a
 * NULL next, or a node whose key (+0x14 read again) is above the node's. */
int32_t net_queue_place(const NetMutexOps *ops, NetQueue *q, NetNode *n);

/* 0049caa0 (cdecl (queue), from 004790b0, 0047a50c, 0047a82c and 0047b17c):
 * under the mutex (+0x18, read again for the unlock), the head's key (+0x10
 * (head, +0x14), never NULL there), or -1 with no head. */
uintptr_t net_queue_head_key(const NetMutexOps *ops, NetQueue *q);

/* A node's test (0049cb1c stops at the first not 0) */
typedef int32_t (*NetQueueTest)(NetNode *n, uintptr_t arg);

/* 0049cb1c (cdecl (queue, test, arg); no direct callers): under the mutex
 * (+0x18, read again for the unlock), the first node from the head (read
 * after the lock; each next read after the test) the test accepts, or NULL. */
NetNode *net_queue_find(const NetMutexOps *ops, NetQueue *q, NetQueueTest test, uintptr_t arg);

/* 0049cb60 (cdecl (queue, n), no direct callers): under the mutex (+0x18,
 * read again for the unlock), the node n places from the head (read after
 * the lock), or NULL past the end. */
NetNode *net_queue_nth(const NetMutexOps *ops, NetQueue *q, uint32_t n);

/* 0049c6a8 (cdecl (to, value, n), 10 callers: the packets' words in network
 * order): value's low n bytes to to[0..n-1], the last byte the lowest (above
 * 4, zeros first); n at most 0 writes nothing. */
void net_put_big(uint8_t *to, uint32_t value, int32_t n);

/* 0049c6cb (cdecl (to, value, n), from 0048f7a0, 004926d4 and 0049f3a0):
 * value's low n bytes to to[0..n-1], the first the lowest (above 4, zeros
 * last); n at most 0 writes nothing. */
void net_put_little(uint8_t *to, uint32_t value, int32_t n);

#endif
