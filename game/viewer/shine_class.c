/* port (body shine): see shine_class.h. */
#include "viewer/shine_class.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *payload;
    int slot;
    ShineClass cls;
} Entry;

static Entry *entries;
static int count, capacity;

static void release(Entry *e) { free((void *)(uintptr_t)e->cls.klass); }

void shine_class_clear(void)
{
    for (int i = 0; i < count; i++) release(&entries[i]);
    count = 0;
}

void shine_class_drop_slot(int slot)
{
    int kept = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].slot == slot) release(&entries[i]);
        else entries[kept++] = entries[i];
    }
    count = kept;
}

void shine_class_add(int slot, const uint8_t *payload, int width, int height, const uint8_t *keep)
{
    if (payload == NULL || keep == NULL || width <= 0 || height <= 0) return;
    if (count == capacity) {
        const int more = capacity != 0 ? capacity * 2 : 64;
        Entry *grown = realloc(entries, (size_t)more * sizeof *entries);
        if (grown == NULL) return;
        entries = grown, capacity = more;
    }
    const size_t size = (size_t)width * (size_t)height;
    uint8_t *copy = malloc(size);
    if (copy == NULL) return;
    memcpy(copy, keep, size);
    entries[count++] = (Entry){ payload, slot, { copy, width, height } };
}

const ShineClass *shine_class_find(const uint8_t *payload)
{
    if (payload == NULL) return NULL;
    for (int i = 0; i < count; i++)
        if (entries[i].payload == payload) return &entries[i].cls;
    return NULL;
}
