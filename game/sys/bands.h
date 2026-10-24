/* Render bands (re/notes/render-bands-design.md): a stage of the frame that is a function of rows alone (the sky, the
 * horizon, the layer's composite) is run over horizontal bands of its rows on several threads. Each band's draw is
 * exactly what the whole draw paints on those rows (tests/test_band_exact.c), so the frame is byte for byte the same
 * with any number of threads; TNFS_BANDS=0 (the default) runs the stage whole on the caller, as before bands. */
#ifndef TNFS_BANDS_H
#define TNFS_BANDS_H

/* A static of its own per band thread (C99 has no _Thread_local). */
#if defined(_MSC_VER)
#define BANDS_THREAD_LOCAL __declspec(thread)
#else
#define BANDS_THREAD_LOCAL __thread
#endif

/* The threads the stages use: TNFS_BANDS = 0 none (the plain path), n >= 1 that many (1: the band code on one
 * thread), "auto" the logical cores (at most 8). Read once. */
int bands_threads(void);
/* Tests and the settings: set it by hand (< 0 back to the environment's). */
void bands_set_threads(int threads);

/* Calls fn(ctx, top, bottom) over [top, bottom) split into bands on bands_threads() threads, or once over the whole
 * range when that is 0. Bands cover the range exactly, in no defined order; fn must write only its rows. */
void bands_rows(int top, int bottom, void (*fn)(void *ctx, int top, int bottom), void *ctx);

#endif
