/*
  Copyright 2025, Open-PS2-Loader Team
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __UIANIM_H
#define __UIANIM_H

/*
 * Time-based animation.
 *
 * Everything here is driven by elapsed seconds, never by frame counts. OPL
 * runs at 60 Hz in NTSC and 50 Hz in PAL, and drops frames under load: an
 * animation indexed by frames would run 20% slow in Europe and stutter
 * everywhere. Indexed by time, a 200 ms move takes 200 ms on both.
 *
 * All floats, deliberately. The R5900 has no double precision in hardware, so
 * every double is emulated in software through libgcc -- one to two orders of
 * magnitude more expensive. This module is compiled with -Wdouble-promotion to
 * keep one from sneaking in.
 */

/** Samples the frame clock. Call exactly once per frame, from guiStartFrame(). */
void uiAnimTick(void);

/** Seconds elapsed since the previous frame, clamped. Zero on the first frame. */
float uiAnimDelta(void);

/** Exponential smoothing: eases `cur` toward `target`, framerate independent.
 *
 * `rate` is roughly "how many e-foldings per second" -- 10 is a brisk UI
 * follow, 20 is snappy. Asymptotic: it never quite arrives, which is what you
 * want for a value that chases a moving target (a focus highlight). For a move
 * with a definite duration, use uiAdvance() instead. */
float uiApproach(float cur, float target, float rate, float dt);

/** Advances a normalized 0..1 progress by dt across `duration` seconds, and
 * clamps at 1. This is the one to use when "200 ms" has to mean 200 ms. */
float uiAdvance(float progress, float duration, float dt);

/** Easing curves over a normalized 0..1 progress. */
float uiEaseOutCubic(float t);
float uiEaseInOutCubic(float t);

#endif
