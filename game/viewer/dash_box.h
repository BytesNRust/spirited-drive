/* port: the dash's box where the exe puts the map (0x206, 0x130, 116 x 142): which of the map, the gear lever and the
 * plain dash it holds, by the mode's file's dash_box (port_extras.h) and the race's own state. */
#ifndef TNFS_DASH_BOX_H
#define TNFS_DASH_BOX_H

#include "game/sys/port_extras.h"

#include <stdint.h>

/* 004f9b3c as a race starts: the map is on only in the exe's mode. M (hud_keys' parts_key) then flips it either way,
 * so from Gear lever or Hidden it shows the map and goes back to the chosen mode. */
int dash_box_start_parts(DashBox mode);

/* 004b92c0: the gear gate drawn before any gear change, which is what keeps the lever in its box. With the map on the
 * gate is not drawn at all (cockpit_gate). */
int dash_box_gate_always(DashBox mode);

/* 1 when the wide dash lays the box as a piece at its anchor: the map; the gate, always in Gear lever and for a gear
 * change otherwise (`gate_state`, 0041f5bc's, not 0). Otherwise the dash runs on unbroken through it. */
int dash_box_shown(DashBox mode, int use_parts, int32_t gate_state, int in_car);

/* The gate state to carry on with after the dash was drawn afresh (a camera back into the cockpit, a reload): the lever
 * is drawn only when its state changes, over a dash the redraw has just painted clean, so in Gear lever the state starts
 * again from 0 and the next frame draws the lever. The other modes keep the exe's (the gate there is a moment's). */
int32_t dash_box_gate_state_after_redraw(DashBox mode, int32_t gate_state);

#endif
