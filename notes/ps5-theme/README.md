# PS5 theme — reference captures

Framebuffer captures from PCSX2, taken with its own screenshot function, so
they are the exact 640x480 output rather than a photo of a window.

The art is a real OPL art pack (Tekken 4 / Tekken 5): per-game `_BG` backdrop,
`_LGO` logo and `_COV` covers.

| File | What it shows |
|---|---|
| `main-screen.png` | The theme as it ships. Title centred at the top in Titillium, the focused game's logo below it, and the card shelf centred on the focus — the focused cover grows, rises and stays at full brightness while the others dim. Everything sits on the game's own backdrop, behind full-screen glass. |
| `sharp-backdrop-rejected.png` | An alternative that was tried and **rejected**: the glass as a band from y=190 down, to leave the backdrop sharp in the upper half. It leaves a hard horizontal seam across the art, which reads worse than blurring the whole thing. Kept as a record so nobody re-proposes it. |

Both were captured with `make DEBUG=1`, which is why the VRAM and fps overlay is
visible in the corner. `1632 KiB TEXMAN` is the pool after the blur chain has
claimed its 224 KiB.
