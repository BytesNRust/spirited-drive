/* port (wrong_way): see wrong_way.h. */
#include "game/race/wrong_way.h"

enum { ANGLE_MASK = 0xffffff, HALF_TURN = 0x800000, QUARTER_TURN = 0x400000 };

/* the nose more than a quarter turn from the road, either side */
static int faces_back(int32_t road_yaw, int32_t nose_yaw)
{
    int32_t apart = (int32_t)(((uint32_t)nose_yaw - (uint32_t)road_yaw) & ANGLE_MASK);
    if (apart > HALF_TURN) apart = (ANGLE_MASK + 1) - apart;
    return apart > QUARTER_TURN;
}

static int wrong(const WrongWayInput *in)
{
    const int forwards = !in->reverse_gear && in->forward_speed > WRONG_WAY_MIN_SPEED;
    const int spinning = in->yaw_rate > WRONG_WAY_SPIN_RATE || in->yaw_rate < -WRONG_WAY_SPIN_RATE;
    return forwards && !spinning && faces_back(in->road_yaw, in->nose_yaw);
}

int wrong_way_step(WrongWay *w, const WrongWayInput *in, int dt_ms)
{
    if (!in->running) {
        *w = (WrongWay){ 0 };
        return 0;
    }
    const int now_wrong = wrong(in);
    if (!w->on) {
        w->wrong_ms = now_wrong ? w->wrong_ms + dt_ms : 0;
        if (w->wrong_ms >= WRONG_WAY_ON_MS) *w = (WrongWay){ .on = 1 };
        return w->on;
    }
    w->shown_ms += dt_ms;
    w->right_ms = now_wrong ? 0 : w->right_ms + dt_ms;
    if (w->right_ms >= WRONG_WAY_OFF_MS) *w = (WrongWay){ 0 };
    return w->on;
}

int wrong_way_visible(const WrongWay *w)
{
    return w->on && (w->shown_ms / WRONG_WAY_BLINK_MS) % 2 == 0;
}

const char *wrong_way_text(int german)
{
    return german ? "FALSCHE RICHTUNG" : "WRONG WAY";
}
