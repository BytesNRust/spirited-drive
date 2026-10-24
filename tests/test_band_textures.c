/* Render bands, phase 0: the car's wheel frames are bound during the frame (drive_draw.c:366, car_wheel_frames ->
 * bind_wheels -> object3d_set_texture), after earlier cars may already have been drawn. A recorded draw command keeps
 * the texture's FshEntry *pointer*; replaying it after the join is only the same picture if binding a wheel frame
 * changes which entry a slot points at and never the entry's own bytes or pixels. This pins that contract. */
#include "game/gfx/object3d.h"
#include "test.h"

#include <string.h>

enum { FRAMES = 4, TEX = 8 };

int main(void)
{
    static uint8_t pixels[FRAMES][TEX * TEX];
    FshEntry entries[FRAMES];
    Object3DTexture slots[FRAMES];
    for (int i = 0; i < FRAMES; i++) {
        memset(pixels[i], 0x10 + i, sizeof pixels[i]);
        entries[i] = (FshEntry){ { 'w', 'h', 'l', (char)('0' + i) }, FSH_TYPE_BITMAP, 0, TEX, TEX, { 0 }, pixels[i], sizeof pixels[i] };
        memset(&slots[i], 0, sizeof slots[i]);
        slots[i].texture = &entries[i];
    }
    FshEntry before[FRAMES];
    uint8_t pixels_before[FRAMES][TEX * TEX];
    memcpy(before, entries, sizeof before);
    memcpy(pixels_before, pixels, sizeof pixels);

    Object3D obj;
    memset(&obj, 0, sizeof obj);
    obj.textures = slots;
    obj.texture_count = FRAMES;

    const FshEntry *recorded = obj.textures[0].texture;   /* what a command recorded before the bind keeps */
    object3d_set_texture(&obj, 0, &entries[2]);
    CHECK(obj.textures[0].texture == &entries[2]);        /* the slot now draws frame 2 ... */
    CHECK(recorded == &entries[0]);                       /* ... the recorded pointer still names frame 0 */
    CHECK_MEM(entries, before, sizeof entries);           /* no entry was rewritten */
    CHECK_MEM(pixels, pixels_before, sizeof pixels);      /* no pixel was rewritten */
    CHECK(recorded->payload == pixels[0] && recorded->payload[0] == 0x10);

    object3d_set_texture(&obj, FRAMES, &entries[1]);      /* past the slots: nothing */
    object3d_set_texture(&obj, -1, &entries[1]);
    CHECK_MEM(entries, before, sizeof entries);
    for (int i = 1; i < FRAMES; i++) CHECK(obj.textures[i].texture == &entries[i]);
    return TEST_REPORT();
}
