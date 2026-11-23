/* The window at rest (owner's complaint 2026-11-23: 10-15 % CPU and fans on an Intel Mac with nothing happening; every frame was drawn and presented through Metal): the main loop asks
 * idle_draw each turn whether to draw and present. Drawing goes on at the frame rate while something moves: an event in the last IDLE_AFTER seconds, a job running, an animation. At
 * rest the window is drawn again only every IDLE_BEAT seconds (a result that arrived, a cursor that blinks), and the loop waits for an event instead of sleeping a frame. */
#ifndef TNFS_TRACK_EDITOR_IDLE_H
#define TNFS_TRACK_EDITOR_IDLE_H

#define IDLE_AFTER 2.0   /* seconds with no event and nothing moving before the window is at rest (a tooltip's delay and a click's reaction are inside it) */
#define IDLE_BEAT 0.5    /* at rest: the longest the picture stays undrawn */
#define IDLE_WAIT_MS 100 /* at rest: how long the loop waits for an event before it looks at the jobs again */

typedef struct {
    double last_event;   /* the clock of the last event or busy turn */
    double last_draw;    /* the clock of the last drawn frame */
    int started;
} Idle;

/* `now` in seconds; `events` 1 when this turn had an event; `busy` 1 when a job or an animation is going. 1 when this turn is to be drawn and presented. */
int idle_draw(Idle *s, double now, int events, int busy);
/* 1 when the window is at rest as of the last call to idle_draw: the loop then waits for an event (IDLE_WAIT_MS) instead of a frame's sleep */
int idle_resting(const Idle *s, double now);

#endif
