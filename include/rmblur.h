/*
  Copyright 2025, Open-PS2-Loader Team
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __RMBLUR_H
#define __RMBLUR_H

#include <gsKit.h>

/*
 * Backdrop blur: render-to-texture on the GS, no shaders.
 *
 * The framebuffer is reduced into a small pyramid and box-blurred by
 * iterating cheap bilinear taps. This is the one place in OPL that repoints
 * the FRAME register mid-frame, so it is kept out of renderman.c and behind
 * the narrowest surface that works. Themes never see it: they reach the
 * result through rmDrawFrosted().
 */

/** Arms the blur for the active video mode. Call from rmSetMode().
 *
 * Deliberately allocates nothing: the video mode is set before the theme is
 * loaded, so here we cannot yet know whether any glass panel will be drawn --
 * and a theme that draws none must not pay 224 KiB out of the TexManager pool.
 * The chain is claimed on the first rmBlurBackdrop() instead.
 *
 * Blur stays off entirely when hires is set: those modes drive the framebuffer
 * through the multi-pass gsKit_hires_* path, which has no single stable render
 * target to sample. */
void rmBlurInit(int hires);

/** Forgets the chain. The VRAM itself goes back with gsKit_deinit_global(). */
void rmBlurEnd(void);

/** Non-zero when the chain is allocated and may be used. */
int rmBlurAvailable(void);

/** Captures the framebuffer as drawn so far and blurs it into the chain.
 *
 * Call once per frame, after the backdrop is queued and before anything that
 * wants to sample it. Flushes the draw queue (the queued backdrop is the
 * input) and leaves the screen bound as the render target again. */
void rmBlurBackdrop(void);

/** The blurred backdrop, or NULL when unavailable.
 *
 * The texture lives in VRAM and has no EE-side copy: never hand it to the
 * TexManager (gsKit_TexManager_bind would try to upload from Mem == NULL). */
GSTEXTURE *rmBlurTexture(void);

#endif
