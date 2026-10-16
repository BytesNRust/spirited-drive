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
