// Colour by's colours (dep95): the colour of a class in a mode (formats/track_trace_stretch.h). The Viewport's road line and the height profile both
// read stretch_colour, so a stretch is the same colour in both; a mode has a row of colours here and nowhere else.
#ifndef TNFS_TRACK_EDITOR_STRETCH_COLOURS_H
#define TNFS_TRACK_EDITOR_STRETCH_COLOURS_H

#include "imgui.h"

extern "C" {
#include "formats/track_trace_stretch.h"
}

namespace stretch_colours {

const int MAX_CLASSES = 8;

#define SC(r, g, b) IM_COL32(r, g, b, 255)
// each mode's classes in order, none red, orange or yellow (they read as an error or a warning; tests/test_stretch_colours.cpp) (track_trace_stretch.h); Limits is the road's own grey
const ImU32 TABLE[TRACE_COLOUR_MODES][MAX_CLASSES] = {
    { SC(200, 204, 212) },                                                                                   // Limits
    { SC(200, 204, 212), SC(80, 170, 255), SC(200, 110, 240) },                                              // Curve: straight, left, right
    { SC(200, 204, 212), SC(150, 230, 140), SC(90, 200, 110), SC(40, 160, 90), SC(120, 220, 230), SC(80, 160, 240), SC(110, 90, 220) },   // Bank
    { SC(90, 120, 140), SC(80, 150, 170), SC(80, 180, 170), SC(110, 205, 160), SC(150, 225, 175), SC(190, 240, 200) },                    // Width
    { SC(200, 204, 212), SC(80, 200, 190), SC(170, 110, 240) },                                              // Lanes
    { SC(200, 204, 212), SC(80, 210, 230), SC(200, 120, 240), SC(150, 230, 90) },                            // Shoulder
    { SC(200, 204, 212), SC(100, 200, 130), SC(160, 230, 170), SC(110, 170, 230), SC(155, 210, 255), SC(180, 130, 230), SC(225, 190, 250) },   // Fence
    { SC(200, 204, 212), SC(150, 230, 140), SC(90, 200, 110), SC(40, 160, 90), SC(120, 220, 230), SC(80, 160, 240), SC(110, 90, 220) },   // Grade: level, up in three steps, down in three
};
#undef SC

}   // namespace stretch_colours

// the colour of `klass` in `mode`; the road's grey for anything out of range
inline ImU32 stretch_colour(int mode, int klass)
{
    if (mode < 0 || mode >= TRACE_COLOUR_MODES || klass < 0 || klass >= track_trace_class_count((TraceColourBy)mode) || klass >= stretch_colours::MAX_CLASSES)
        return stretch_colours::TABLE[TRACE_COLOUR_LIMITS][0];
    return stretch_colours::TABLE[mode][klass];
}

#endif
