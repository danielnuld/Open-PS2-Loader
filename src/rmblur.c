/*
  Copyright 2025, Open-PS2-Loader Team
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <stdio.h>
#include <kernel.h>

#include "include/opl.h"
#include "include/renderman.h"
#include "include/rmblur.h"
#include "include/ioman.h"

extern GSGLOBAL *gsGlobal;

/*
 * Render target widths MUST be exact multiples of 64.
 *
 * gsKit writes and reads the same buffer through two different roundings:
 *
 *   gsKit_setactive()  FRAME.FBW = Width / 64            (truncates)
 *   gsKit_setup_tbw()  TEX0.TBW  = ceil(Width / 64)      (rounds up)
 *
 * A 160-wide buffer is therefore rendered with a 128-texel stride and sampled
 * back with a 192-texel stride, which tears it. 320 (=5*64) and 128 (=2*64)
 * agree under both. Heights are unconstrained: FRAME carries no height.
 */
#define BLUR_W_HALF  320
#define BLUR_W_SMALL 128

/* Iterating a box blur converges on a gaussian. Each pass is one bilinear tap
   over 128xN texels on a 2.4 Gpixel/s chip, so the cost is noise. The offsets
   alternate sign: a fractional half-texel shift is what makes the tap average
   a 2x2 neighbourhood, and alternating keeps the image from walking away from
   where it started. They sum to zero. */
static const float blurOffsets[] = {0.5f, -0.5f, 1.5f, -1.5f};
#define BLUR_PASSES (sizeof(blurOffsets) / sizeof(blurOffsets[0]))

static int blurReady = 0;   /* chain allocated and usable */
static int blurHires = 0;   /* mode we cannot blur in at all */
static int blurTried = 0;   /* allocation attempted for this video mode */

static GSTEXTURE rtHalf;      /* framebuffer reduced by 2  */
static GSTEXTURE rtA, rtB;    /* ping-pong pair            */
static GSTEXTURE fbTex;       /* aliases the live framebuffer */
static GSTEXTURE *blurResult; /* whichever of rtA/rtB the last pass wrote */

/* Screen state parked while a render target is bound. */
static u32 savedBuffer;
static int savedWidth, savedHeight, savedPSM;

static void rmBlurSetupRT(GSTEXTURE *rt, int w, int h)
{
    rt->Width = w;
    rt->Height = h;
    rt->PSM = GS_PSM_CT16S;
    rt->Mem = NULL; /* VRAM-resident: there is no EE-side copy */
    rt->Clut = NULL;
    rt->VramClut = 0;
    rt->Filter = GS_FILTER_LINEAR;
    rt->Delayed = 0;
    gsKit_setup_tbw(rt);
}

void rmBlurInit(int hires)
{
    /* Deliberately does NOT allocate. The video mode is set before the theme is
       loaded (opl.c, applyConfig), so at this point we cannot know whether any
       glass panel will ever be drawn -- and a theme that never draws one must
       not pay 224 KiB out of the TexManager pool. The chain is claimed on first
       use instead, from rmBlurBackdrop(). */
    blurReady = 0;
    blurTried = 0;
    blurHires = hires;
    blurResult = NULL;
}

/* Claim the chain. Called on the first glass panel of the first frame that has
   one; a theme with no glass panel never gets here and never pays for it. */
static int rmBlurAlloc(void)
{
    blurTried = 1;

    /* The hires path swaps framebuffers per pass; there is nothing stable to
       sample, and at 720p/1080i the chain would not fit anyway. */
    if (blurHires)
        return 0;

    const int h0 = gsGlobal->Height / 2;
    const int h1 = gsGlobal->Height / 4;

    if (h1 < 1)
        return 0;

    rmBlurSetupRT(&rtHalf, BLUR_W_HALF, h0);
    rmBlurSetupRT(&rtA, BLUR_W_SMALL, h1);
    rmBlurSetupRT(&rtB, BLUR_W_SMALL, h1);

    /* SYSBUFFER, not USERBUFFER: FRAME.FBP addresses VRAM in units of 8192
       bytes, so a render target base that is only 256-byte aligned would have
       its low bits truncated and the GS would draw somewhere else. SYSBUFFER
       is the allocation type that rounds up to 8 KiB. */
    GSTEXTURE *rts[3] = {&rtHalf, &rtA, &rtB};
    for (int i = 0; i < 3; i++) {
        rts[i]->Vram = gsKit_vram_alloc(gsGlobal,
                                        gsKit_texture_size(rts[i]->Width, rts[i]->Height, rts[i]->PSM),
                                        GSKIT_ALLOC_SYSBUFFER);
        if (rts[i]->Vram == GSKIT_ALLOC_ERROR) {
            /* Decision 4 says this cannot happen -- gsKit_vram_alloc only
               shrinks the streaming pool -- but the pool is finite and a
               future mode could exhaust it. Stay disabled rather than draw
               into address 0. */
            LOG("RMBLUR out of VRAM, blur disabled\n");
            return 0;
        }
    }

    blurReady = 1;
    LOG("RMBLUR chain ready: %dx%d + 2x %dx%d, %d KiB\n",
        BLUR_W_HALF, h0, BLUR_W_SMALL, h1,
        (gsKit_texture_size(BLUR_W_HALF, h0, GS_PSM_CT16S) +
         2 * gsKit_texture_size(BLUR_W_SMALL, h1, GS_PSM_CT16S)) /
            1024);

    return blurReady;
}

void rmBlurEnd(void)
{
    /* The VRAM goes back wholesale with gsKit_deinit_global(); all we do is
       forget, so the next video mode claims the chain again on first use. */
    blurReady = 0;
    blurTried = 0;
    blurResult = NULL;
}

int rmBlurAvailable(void)
{
    return blurReady;
}

/* Point the GS at a render target.
 *
 * gsKit_setactive() emits FRAME immediately instead of queueing it, so any
 * primitives still in the queue would land in the NEW target. The queue has to
 * be drained against the old one first. This is the whole trick. */
static void rmBlurBindRT(GSTEXTURE *rt)
{
    gsKit_queue_exec(gsGlobal);

    gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] = rt->Vram;
    gsGlobal->Width = rt->Width;
    gsGlobal->Height = rt->Height;
    gsGlobal->PSM = rt->PSM;

    gsKit_setactive(gsGlobal);
}

static void rmBlurRestoreScreen(void)
{
    gsKit_queue_exec(gsGlobal);

    gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] = savedBuffer;
    gsGlobal->Width = savedWidth;
    gsGlobal->Height = savedHeight;
    gsGlobal->PSM = savedPSM;

    gsKit_setactive(gsGlobal);
}

/* Stretch the whole of src over the whole of dst, offset by `off` texels on
   both axes. Downscaling by 2 with a linear filter lands each sample exactly
   between four source texels, so the 2x2 box average comes for free. */
static void rmBlurBlit(GSTEXTURE *src, GSTEXTURE *dst, float off)
{
    gsKit_prim_sprite_texture(gsGlobal, src,
                              0.0f, 0.0f,
                              off, off,
                              (float)dst->Width, (float)dst->Height,
                              (float)src->Width + off, (float)src->Height + off,
                              0, gDefaultCol);
}

void rmBlurBackdrop(void)
{
    /* First glass panel ever drawn in this video mode: claim the chain now.
       gsKit_vram_alloc() re-inits the TexManager, so the textures already
       uploaded get re-streamed over the next frame -- a one-off hiccup, and
       the price of not charging 224 KiB to themes that never blur. */
    if (!blurTried && !rmBlurAlloc())
        return;

    if (!blurReady)
        return;

    savedBuffer = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1];
    savedWidth = gsGlobal->Width;
    savedHeight = gsGlobal->Height;
    savedPSM = gsGlobal->PSM;

    /* Alias the framebuffer we have been drawing into, so it can be sampled. */
    fbTex.Width = savedWidth;
    fbTex.Height = savedHeight;
    fbTex.PSM = savedPSM;
    fbTex.Vram = savedBuffer;
    fbTex.Mem = NULL;
    fbTex.Clut = NULL;
    fbTex.VramClut = 0;
    fbTex.Filter = GS_FILTER_LINEAR;
    fbTex.Delayed = 0;
    gsKit_setup_tbw(&fbTex);

    /* Every pass is a copy, not a composite. With blending left on, the
       framebuffer's alpha would enter the equation -- and in CT16S that
       channel is a single bit whose value we do not control. Off it goes,
       along with the alpha test, which would otherwise reject texels. */
    const int savedAlpha = gsGlobal->PrimAlphaEnable;
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
    gsKit_set_test(gsGlobal, GS_ATEST_OFF);
    gsKit_set_clamp(gsGlobal, GS_CMODE_CLAMP); /* offsets must not wrap the edges */

    /* Reduce: framebuffer -> 320xH/2 -> 128xH/4. */
    rmBlurBindRT(&rtHalf);
    rmBlurBlit(&fbTex, &rtHalf, 0.0f);

    rmBlurBindRT(&rtA);
    rmBlurBlit(&rtHalf, &rtA, 0.0f);

    /* Widen: ping-pong, one bilinear tap each way. */
    GSTEXTURE *src = &rtA;
    GSTEXTURE *dst = &rtB;
    for (unsigned int i = 0; i < BLUR_PASSES; i++) {
        rmBlurBindRT(dst);
        rmBlurBlit(src, dst, blurOffsets[i]);

        GSTEXTURE *swap = src;
        src = dst;
        dst = swap;
    }
    blurResult = src;

    rmBlurRestoreScreen();

    gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);
    gsGlobal->PrimAlphaEnable = savedAlpha;
}

GSTEXTURE *rmBlurTexture(void)
{
    return blurReady ? blurResult : NULL;
}
