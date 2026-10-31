#include "game/viewer/dash_box.h"

int dash_box_start_parts(DashBox mode)
{
    return mode == DASH_BOX_MAP;
}

int dash_box_gate_always(DashBox mode)
{
    return mode == DASH_BOX_LEVER;
}

int dash_box_shown(DashBox mode, int use_parts, int32_t gate_state, int in_car)
{
    return use_parts != 0 || (in_car && (dash_box_gate_always(mode) || gate_state != 0));
}

int32_t dash_box_gate_state_after_redraw(DashBox mode, int32_t gate_state)
{
    return dash_box_gate_always(mode) ? 0 : gate_state;
}
