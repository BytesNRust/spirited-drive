/* The transport's queues (net_queue.h) */
#include "game/net/net_queue.h"

#include <stddef.h>
#include <string.h>

const char NET_QUEUE_FETCH_LOST[] = "qfetchpriority -- FOUND ELEMENT THAT COULD NOT BE REMOVED!";
const char NET_QUEUE_FETCH_TEST_LOST[] = "qfetch -- FOUND ELEMENT THAT COULD NOT BE REMOVED!";

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

int32_t net_queue_remove_locked(const NetMutexOps *ops, NetQueue *q, NetNode *n)
{
    int32_t removed = 0;
    ops->lock(ops->ctx, q->mutex);
    if (n != NULL) removed = net_queue_remove(q, n);
    ops->unlock(ops->ctx, q->mutex);
    return removed;
}

int32_t net_queue_place(const NetMutexOps *ops, NetQueue *q, NetNode *n)
{
    int32_t place = -1;
    ops->lock(ops->ctx, q->mutex);
    if (n != NULL) {
        const NetQueueKey key = q->key;
        NetNode *p = q->head;
        const uintptr_t own = key != NULL ? key(n, q->key_arg) : 0;
        place = 0;
        while (p != NULL && p != n && (key == NULL || key(p, q->key_arg) <= own)) p = p->next, place++;
        if (p != n) place = -1;
    }
    ops->unlock(ops->ctx, q->mutex);
    return place;
}

static uintptr_t head_key(const NetMutexOps *ops, NetQueue *q, uintptr_t none)
{
    uintptr_t key = none;
    ops->lock(ops->ctx, q->mutex);
    if (q->head != NULL) key = q->key(q->head, q->key_arg);
    ops->unlock(ops->ctx, q->mutex);
    return key;
}

uintptr_t net_queue_head_key(const NetMutexOps *ops, NetQueue *q) { return head_key(ops, q, (uintptr_t)-1); }

uintptr_t net_queue_head_key_or_zero(const NetMutexOps *ops, NetQueue *q) { return head_key(ops, q, 0); }

NetNode *net_queue_find(const NetMutexOps *ops, NetQueue *q, NetQueueTest test, uintptr_t arg)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *p = q->head;
    while (p != NULL && test(p, arg) == 0) p = p->next;
    ops->unlock(ops->ctx, q->mutex);
    return p;
}

NetNode *net_queue_find_key(const NetMutexOps *ops, NetQueue *q, uintptr_t key)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *p = q->head;
    while (p != NULL && q->key(p, q->key_arg) < key) p = p->next;
    if (p != NULL && q->key(p, q->key_arg) != key) p = NULL;
    ops->unlock(ops->ctx, q->mutex);
    return p;
}

NetNode *net_queue_fetch(const NetMutexOps *ops, const NetQueueMessage *message, NetQueue *q, uintptr_t key)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *p = q->head;
    while (p != NULL && q->key(p, q->key_arg) < key) p = p->next;
    if (p == NULL || q->key(p, q->key_arg) != key) {
        p = NULL;
    } else if (net_queue_remove(q, p) == 0) {
        message->fn(message->ctx, NET_QUEUE_FETCH_LOST);
        p = NULL;
    }
    ops->unlock(ops->ctx, q->mutex);
    return p;
}

NetNode *net_queue_fetch_test(const NetMutexOps *ops, const NetQueueMessage *message, NetQueue *q, NetQueueTest test,
                              uintptr_t arg)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *p = q->head;
    while (p != NULL && test(p, arg) == 0) p = p->next;
    if (p != NULL && net_queue_remove(q, p) == 0) {
        message->fn(message->ctx, NET_QUEUE_FETCH_TEST_LOST);
        p = NULL;
    }
    ops->unlock(ops->ctx, q->mutex);
    return p;
}

int32_t net_queue_walk(const NetMutexOps *ops, NetQueue *q, const NetQueueVisit *visit, uintptr_t arg)
{
    ops->lock(ops->ctx, q->mutex);
    const uint32_t kept = q->flags & NET_QUEUE_CHANGED;
    q->flags &= ~(uint32_t)NET_QUEUE_CHANGED;
    ops->unlock(ops->ctx, q->mutex);
    int32_t n = 0;
    for (NetNode *p = q->head; p != NULL && (q->flags & NET_QUEUE_CHANGED) == 0; p = p->next, n++)
        if (visit->fn(visit->ctx, p, arg) == 0) q->flags |= NET_QUEUE_CHANGED;
    ops->lock(ops->ctx, q->mutex);
    if ((q->flags & NET_QUEUE_CHANGED) == 0) n = -1;
    q->flags |= kept;
    ops->unlock(ops->ctx, q->mutex);
    return n;
}

NetNode *net_queue_nth(const NetMutexOps *ops, NetQueue *q, uint32_t n)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *p = q->head;
    for (uint32_t k = 0; p != NULL && k < n; k++) p = p->next;
    ops->unlock(ops->ctx, q->mutex);
    return p;
}

void net_queue_push_back(const NetMutexOps *ops, NetQueue *q, NetNode *n)
{
    ops->lock(ops->ctx, q->mutex);
    if (n != NULL) {
        NetNode *const last = q->tail;
        n->next = NULL;
        q->tail = n;
        q->count++;
        if (last == NULL) q->head = n;
        else last->next = n;
        q->flags |= NET_QUEUE_CHANGED;
    }
    ops->unlock(ops->ctx, q->mutex);
}

NetNode *net_queue_pop_back(const NetMutexOps *ops, NetQueue *q)
{
    ops->lock(ops->ctx, q->mutex);
    NetNode *const n = q->tail;
    if (n != NULL) {
        q->count--;
        NetNode *const first = q->head;
        if (n == first) {
            q->head = q->tail = NULL;
        } else {
            q->tail = first;
            while (q->tail->next != n) q->tail = q->tail->next;
            q->tail->next = NULL;
        }
        n->next = NULL;
    }
    q->flags |= NET_QUEUE_CHANGED;
    ops->unlock(ops->ctx, q->mutex);
    return n;
}

uint32_t net_queue_count(const NetQueue *q) { return q->count; }

void net_queue_close(const NetMutexOps *ops, const NetQueue *q) { ops->close(ops->ctx, q->mutex); }

void net_copy(const uint8_t *from, uint8_t *to, int32_t n)
{
    if (n <= 0) return;
    if (n < 4 || (to > from && to < from + n)) {
        for (int32_t i = n - 1; i >= 0; i--) to[i] = from[i];
        return;
    }
    memmove(to, from, (size_t)n);
}
