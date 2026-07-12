/*
  Copyright 2025, Open-PS2-Loader Team
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <time.h>
#include <math.h>

#include "include/uianim.h"

/* A hitch -- scanning a device, spinning up the DVD -- must not teleport every
   animation to its target. Anything longer than this counts as one slow frame,
   not as elapsed time to catch up on. Three PAL frames. */
#define UI_DT_MAX 0.06f

static clock_t uiPrev = 0;
static float uiDelta = 0.0f;

void uiAnimTick(void)
{
    const clock_t now = clock();

    if (uiPrev == 0) {
        /* First frame: no interval to animate across. */
        uiDelta = 0.0f;
    } else {
        const clock_t elapsed = now - uiPrev;

        uiDelta = (float)elapsed / (float)CLOCKS_PER_SEC;

        if (uiDelta > UI_DT_MAX)
            uiDelta = UI_DT_MAX;
        else if (uiDelta < 0.0f)
            uiDelta = 0.0f; /* the clock wrapped */
    }

    uiPrev = now;
}

float uiAnimDelta(void)
{
    return uiDelta;
}

float uiApproach(float cur, float target, float rate, float dt)
{
    if (rate <= 0.0f || dt <= 0.0f)
        return cur;

    /* The 1 - exp(-rate*dt) form is the whole point: it makes the step depend
       on elapsed time rather than on being called. Two 8 ms steps land exactly
       where one 16 ms step does. The naive cur += (target - cur) * k does not
       have that property -- it would ease 20% slower at PAL's 50 Hz. */
    const float t = 1.0f - expf(-rate * dt);

    return cur + (target - cur) * t;
}

float uiAdvance(float progress, float duration, float dt)
{
    if (duration <= 0.0f)
        return 1.0f;

    progress += dt / duration;

    if (progress > 1.0f)
        progress = 1.0f;
    else if (progress < 0.0f)
        progress = 0.0f;

    return progress;
}

float uiEaseOutCubic(float t)
{
    const float inv = 1.0f - t;

    return 1.0f - inv * inv * inv;
}

float uiEaseInOutCubic(float t)
{
    if (t < 0.5f)
        return 4.0f * t * t * t;

    const float inv = -2.0f * t + 2.0f;

    return 1.0f - (inv * inv * inv) / 2.0f;
}
