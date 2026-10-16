/* The transport's queues (net_queue.h) */
#include "game/net/net_queue.h"

#include <stddef.h>

int32_t net_queue_remove(NetQueue *q, NetNode *n)
{
    if (n == NULL || q->count == 0) return 0;
    NetNode *p = q->head;
    if (n == p) {
        q->count--;
        if (p == q->tail) q->head = q->tail = NULL;
        else q->head = p->next;
    } else {
        while (p->next != NULL && n != p->next) p = p->next;
        if (p->next == NULL || n != p->next) return 0;
        q->count--;
        p->next = n->next;
        if (n == q->tail) q->tail = p;
    }
    n->next = NULL;
    q->flags |= NET_QUEUE_CHANGED;
    return 1;
}

void net_put_big(uint8_t *to, uint32_t value, int32_t n)
{
    for (int32_t i = n - 1; i >= 0; i--, value >>= 8) to[i] = (uint8_t)value;
}

void net_put_little(uint8_t *to, uint32_t value, int32_t n)
{
    for (int32_t i = 0; i < n; i++, value >>= 8) to[i] = (uint8_t)value;
}

uintptr_t net_queue_key_self(const NetNode *n, uintptr_t arg) { return (void)arg, (uintptr_t)n; }

void net_queue_init(const NetMutexOps *ops, NetQueue *q, NetQueueKey key, uintptr_t arg)
{
    void *const mutex = ops->create(ops->ctx);
    q->count = q->flags = 0;
    q->head = q->tail = NULL;
    q->mutex = mutex;
    q->key = key != NULL ? key : net_queue_key_self;
    q->key_arg = arg;
}

void net_queue_insert(const NetMutexOps *ops, NetQueue *q, NetNode *n)
{
    ops->lock(ops->ctx, q->mutex);
    if (n != NULL) {
        const uintptr_t key = q->key(n, q->key_arg);
        q->count++;
        NetNode *prev = NULL, *p = q->head;
        while (p != NULL && q->key(p, q->key_arg) < key) prev = p, p = p->next;
        n->next = p;
        if (prev == NULL) q->head = n;
        else prev->next = n;
        if (p == NULL) q->tail = n;
        q->flags |= NET_QUEUE_CHANGED;
    }
    ops->unlock(ops->ctx, q->mutex);
}

void net_queue_push_front(const NetMutexOps *ops, NetQueue *q, NetNode *n)
{
    ops->lock(ops->ctx, q->mutex);
    if (n != NULL) {
        n->next = q->head;
        q->head = n;
        q->count++;
        if (n->next == NULL) q->tail = n;
        q->flags |= NET_QUEUE_CHANGED;
    }
    ops->unlock(ops->ctx, q->mutex);
}

NetNode *net_queue_pop_front(const NetMutexOps *ops, NetQueue *q)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *const n = q->head;
    if (n != NULL) {
        if (n == q->tail) q->head = q->tail = NULL;
        else q->head = n->next;
        q->count--;
        n->next = NULL;
    }
    q->flags |= NET_QUEUE_CHANGED;
    ops->unlock(ops->ctx, q->mutex);
    return n;
}
