"""Generate PS5-style controller glyphs for the OPL PS5 theme.

Modern PlayStation face buttons are MONOCHROME white outlines -- that is the
single biggest visual difference from the PS2 icons OPL ships, which are filled
and colour-coded. Everything here is drawn as a stroke, no fills.

Drawn at 8x and downsampled with LANCZOS: OPL renders these at roughly 20 px
tall on a CRT, so clean antialiased edges matter far more than resolution.
"""
from PIL import Image, ImageDraw
import math
import os

OUT = os.path.dirname(__file__)  # writes straight into the theme folder
S = 8           # supersample factor
SIZE = 64       # final edge, px
C = SIZE * S    # canvas edge
WHITE = (255, 255, 255, 255)


def canvas():
    img = Image.new("RGBA", (C, C), (255, 255, 255, 0))
    return img, ImageDraw.Draw(img)


def save(img, name):
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    path = os.path.join(OUT, name + ".png")
    img.save(path)
    print("%-12s %s" % (name, img.size))


def circle():
    img, d = canvas()
    w = 5 * S
    pad = 12 * S
    d.ellipse([pad, pad, C - pad, C - pad], outline=WHITE, width=w)
    save(img, "circle")


def cross():
    img, d = canvas()
    w = 5 * S
    pad = 15 * S
    d.line([pad, pad, C - pad, C - pad], fill=WHITE, width=w)
    d.line([C - pad, pad, pad, C - pad], fill=WHITE, width=w)
    # round the four tips so the strokes do not end in hard chisels
    r = w // 2
    for x, y in ((pad, pad), (C - pad, pad), (pad, C - pad), (C - pad, C - pad)):
        d.ellipse([x - r, y - r, x + r, y + r], fill=WHITE)
    save(img, "cross")


def triangle():
    img, d = canvas()
    w = 5 * S
    pad = 12 * S
    # Equilateral, optically centred: the centroid of a triangle sits low, so
    # the shape is nudged down a touch to look centred in the box.
    h = (C - 2 * pad) * math.sqrt(3) / 2
    top = (C / 2, (C - h) / 2 + 2 * S)
    left = (pad, top[1] + h)
    right = (C - pad, top[1] + h)
    d.line([top, left, right, top], fill=WHITE, width=w, joint="curve")
    save(img, "triangle")


def square():
    img, d = canvas()
    w = 5 * S
    pad = 13 * S
    d.rectangle([pad, pad, C - pad, C - pad], outline=WHITE, width=w)
    save(img, "square")


def start():
    """OPL's START maps to the modern OPTIONS button: three stacked bars."""
    img, d = canvas()
    w = 5 * S
    pad = 14 * S
    gap = 9 * S
    mid = C / 2
    for y in (mid - gap, mid, mid + gap):
        d.line([pad, y, C - pad, y], fill=WHITE, width=w)
    save(img, "start")


def select():
    """OPL's SELECT maps to CREATE: a rounded pill split down the middle,
    echoing the touchpad the button sits beside."""
    img, d = canvas()
    w = 5 * S
    padx = 11 * S
    pady = 17 * S
    d.rounded_rectangle([padx, pady, C - padx, C - pady],
                        radius=6 * S, outline=WHITE, width=w)
    d.line([C / 2, pady, C / 2, C - pady], fill=WHITE, width=w)
    save(img, "select")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    circle()
    cross()
    triangle()
    square()
    start()
    select()
