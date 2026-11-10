/* what a wet road does round the cars and how much weather there is at a node (view_rain.c); ported_view.c and view_spray.c call these */
#ifndef TNFS_VIEW_RAIN_H
#define TNFS_VIEW_RAIN_H

#include "game/race/weather.h"
#include "viewer/weather_sky.h"

#include <stdint.h>


enum { RAIN_MOOD_SHARE = 7, RAIN_MOOD_SHARE_NIGHT = 10 };   /* of 16 (a 4x4 ordered dither): how much of the picture is moved toward the rain's grey; at night the grey lifts the dark picture, so more of it keeps the rain no darker than the dry night */
enum { RAIN_V2_RAISE_DIV = 8 };   /* port (rain_v2): the streaks' vanishing point is raised by this fraction of the view's height */

int rain_open_at(int32_t node);   /* how much weather there is at a node, 0..16: none inside a closed zone */
uint32_t rain_hash(uint32_t a, uint32_t b);
int rain_wet_off(void);   /* dev, env only: TNFS_RAIN_WET_OFF=1 */
void rain_vanishing_point(int32_t out[2]);   /* the screen point the rain streams from */
void rain_cars_paint(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WeatherTables *wt);   /* the tail lights' reflections, over the finished 3D */

#endif
