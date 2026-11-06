/* The dash box's three modes (Map, Gear lever, Hidden): what a race starts with, when the gate may draw, when the box is
 * a piece of the wide dash, M's flip, and the key in extras.cfg. */
#include "game/viewer/dash_box.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

static char stored[512];
static size_t stored_size;

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    if (strcmp(name, "port.cfg") == 0) {   /* the mode that leaves everything free; port_profile's test has the others */
        static const char MODE[] = "profile=restomod\n";
        uint8_t *mode = malloc(sizeof MODE);
        memcpy(mode, MODE, sizeof MODE);
        *size = sizeof MODE - 1;
        return mode;
    }
    if (stored_size == 0) return NULL;
    uint8_t *copy = malloc(stored_size);
    memcpy(copy, stored, stored_size);
    *size = stored_size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (strcmp(name, "port.cfg") == 0) return true;   /* the mode stays */
    memcpy(stored, data, size);
    stored_size = size;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static void test_start_shows_the_map_only_in_the_exes_mode(void)
{
    CHECK_EQ_INT(dash_box_start_parts(DASH_BOX_MAP), 1);
    CHECK_EQ_INT(dash_box_start_parts(DASH_BOX_LEVER), 0);
    CHECK_EQ_INT(dash_box_start_parts(DASH_BOX_HIDDEN), 0);
}

/* box present or absent per state, with and without a gear change (gate_state 0 = none showing) */
static void test_box_per_state(void)
{
    for (int mode = DASH_BOX_MAP; mode <= DASH_BOX_HIDDEN; mode++) {
        CHECK_EQ_INT(dash_box_shown((DashBox)mode, 1, 0, 1), 1);   /* the map is on: the box, gear change or not */
        CHECK_EQ_INT(dash_box_shown((DashBox)mode, 1, 3, 1), 1);
    }
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_MAP, 0, 0, 1), 0);      /* the exe's, map hidden: the plain dash ... */
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_MAP, 0, 3, 1), 1);      /* ... and the lever for a gear change */
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_HIDDEN, 0, 0, 1), 0);
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_HIDDEN, 0, 3, 1), 1);
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_LEVER, 0, 0, 1), 1);    /* Gear lever: always, gear change or not */
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_LEVER, 0, 3, 1), 1);
    CHECK_EQ_INT(dash_box_shown(DASH_BOX_LEVER, 0, 3, 0), 0);    /* outside the car there is no dash */
}

static void test_gate_always_only_in_gear_lever(void)
{
    CHECK_EQ_INT(dash_box_gate_always(DASH_BOX_MAP), 0);
    CHECK_EQ_INT(dash_box_gate_always(DASH_BOX_LEVER), 1);
    CHECK_EQ_INT(dash_box_gate_always(DASH_BOX_HIDDEN), 0);
}

/* M is the exe's 1 - use_parts: from Gear lever or Hidden it shows the map, and again it is back to the chosen mode */
static void test_m_toggles_map_and_chosen_mode(void)
{
    for (int mode = DASH_BOX_LEVER; mode <= DASH_BOX_HIDDEN; mode++) {
        const int plain = dash_box_shown((DashBox)mode, dash_box_start_parts((DashBox)mode), 0, 1);
        int use_parts = dash_box_start_parts((DashBox)mode);
        CHECK_EQ_INT(plain, mode == DASH_BOX_LEVER);
        use_parts = 1 - use_parts;   /* M: the map */
        CHECK_EQ_INT(dash_box_shown((DashBox)mode, use_parts, 0, 1), 1);
        use_parts = 1 - use_parts;   /* M again: the chosen mode */
        CHECK_EQ_INT(use_parts, dash_box_start_parts((DashBox)mode));
        CHECK_EQ_INT(dash_box_shown((DashBox)mode, use_parts, 0, 1), plain);
    }
}

static void test_extras_key_round_trip(void)
{
    stored_size = 0;
    CHECK_EQ_INT(port_extras_dash_box(), DASH_BOX_MAP);   /* no file: the exe's */
    for (int mode = DASH_BOX_MAP; mode <= DASH_BOX_HIDDEN; mode++) {
        PortExtras e = port_extras_defaults();
        e.dash_box = (DashBox)mode;
        CHECK(port_extras_save(&e));
        CHECK_EQ_INT(port_extras_load().dash_box, mode);
        CHECK_EQ_INT(port_extras_dash_box(), mode);
    }
    {
        const char *junk = "dash_box=sideways\n";
        PortExtras e = port_extras_parse(junk, strlen(junk));
        CHECK_EQ_INT(e.dash_box, DASH_BOX_MAP);   /* a damaged value leaves the default */
    }
}

/* the lever is drawn only on a change of state, so a dash redrawn under it (the cockpit again after an outside camera)
 * must start the state over in Gear lever, and leave the exe's modes as they are */
static void test_redraw_restarts_the_lever_only_in_gear_lever(void)
{
    for (int32_t state = 0; state <= 5; state++) {
        CHECK_EQ_INT(dash_box_gate_state_after_redraw(DASH_BOX_LEVER, state), 0);
        CHECK_EQ_INT(dash_box_gate_state_after_redraw(DASH_BOX_MAP, state), state);
        CHECK_EQ_INT(dash_box_gate_state_after_redraw(DASH_BOX_HIDDEN, state), state);
    }
}

int main(void)
{
    test_start_shows_the_map_only_in_the_exes_mode();
    test_box_per_state();
    test_gate_always_only_in_gear_lever();
    test_redraw_restarts_the_lever_only_in_gear_lever();
    test_m_toggles_map_and_chosen_mode();
    test_extras_key_round_trip();
    return TEST_REPORT();
}
