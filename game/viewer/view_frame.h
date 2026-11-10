/* the frame's set-up steps: the weather sky, the track stream and road style, the transform, the cars' list and the recorder start (view_frame.c); ported_view.c calls these */
#ifndef TNFS_VIEW_FRAME_H
#define TNFS_VIEW_FRAME_H

#include "viewer/view_shared.h"

#include <stdint.h>

void cars_frame_begin(const TrackArt *art, const WeatherTables *wt, uint8_t *pixels);
void cars_stream_style(const Tri *tri, const TrackArt *art, const PortedCamera *pc, const WeatherTables *wt);
void cars_take_list(const TrackArt *art, const PortedCar *list, int count, QuadTarget *quad, View *view);
void cars_transform(TrackCamera *cam, DrawContext *ctx, View *view);
void cars_weather_sky(const WeatherTables *wt, uint8_t *pixels, const int32_t box[4], View *view, const TrackCamera *cam);
#endif
