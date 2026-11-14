"""MUA3 Recomp's own icon, drawn here (no game art, no logos), in the launcher's colours: a cosmic night (indigo to
violet, nebula glows, stars), three gold chevrons stacked into a "3", and the six gem colours in a row. The release's
exes and windows use it (Makefile RELEASE=1); a source build with your own dump still uses the game's icon
(tools/make_icon.py).

Usage: make_original_icon.py <out dir>   ->  <out>/icon.png (512x512) and <out>/icon.ico (16..256)
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

GOLD, GOLD_D = (255, 204, 56, 255), (190, 120, 10, 255)
GEMS = [(235, 41, 69), (255, 140, 33), (56, 133, 255), (163, 82, 255), (51, 209, 117), (255, 204, 51)]  # (launcher.cpp)
S = 1024  # drawn large, scaled down


def chevron(d, cx, cy, w, h, t, col):
    """A chevron pointing right: its tip at (cx + w / 2, cy), arms h tall, a stroke t thick."""
    x0, x1 = cx - w / 2, cx + w / 2
    d.line([(x0, cy - h / 2), (x1, cy), (x0, cy + h / 2)], fill=col, width=int(t), joint='curve')
    for p in ((x0, cy - h / 2), (x0, cy + h / 2)):
        d.ellipse([p[0] - t / 2, p[1] - t / 2, p[0] + t / 2, p[1] + t / 2], fill=col)


def gem(d, cx, cy, r, rgb):
    pts = [(cx + r * math.cos(i * math.pi / 3 + math.pi / 6), cy + r * math.sin(i * math.pi / 3 + math.pi / 6))
           for i in range(6)]
    light = tuple(min(255, c + (255 - c) * 45 // 100) for c in rgb)
    dark = tuple(c * 65 // 100 for c in rgb)
    d.polygon(pts, fill=rgb + (255,))
    d.polygon([(cx, cy), pts[3], pts[4]], fill=light + (255,))
    d.polygon([(cx, cy), pts[0], pts[1]], fill=dark + (255,))
    d.ellipse([cx - r * 0.42, cy - r * 0.48, cx - r * 0.16, cy - r * 0.22], fill=(255, 255, 255, 230))


def main():
    out = sys.argv[1]
    im = Image.new('RGBA', (S, S))
    d = ImageDraw.Draw(im)
    for y in range(S):  # indigo at the top to violet at the bottom
        k = y / S
        d.line([(0, y), (S, y)], fill=(int(14 + 30 * k), int(10 + 2 * k), int(40 + 20 * k), 255))
    neb = Image.new('RGBA', (S, S), (0, 0, 0, 0))  # nebula glows
    nd = ImageDraw.Draw(neb)
    for (x, y, r, rgb, a) in ((0.18, 0.25, 0.45, GEMS[3], 150), (0.88, 0.12, 0.35, GEMS[0], 90),
                              (0.80, 0.90, 0.45, GEMS[2], 110)):
        nd.ellipse([S * (x - r), S * (y - r), S * (x + r), S * (y + r)], fill=rgb + (a,))
    im.alpha_composite(neb.filter(ImageFilter.GaussianBlur(120)))
    d = ImageDraw.Draw(im)
    rnd = random.Random(3)
    for _ in range(140):  # stars
        x, y, r = rnd.uniform(0, S), rnd.uniform(0, S), rnd.choice((1.5, 2, 2, 3, 4))
        d.ellipse([x - r, y - r, x + r, y + r], fill=(240, 236, 255, rnd.randint(120, 230)))
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))  # the chevrons: a shadow, a deep edge, then the gold
    sd = ImageDraw.Draw(shadow)
    rows = (S * 0.25, S * 0.45, S * 0.65)
    for cy in rows:
        chevron(sd, S * 0.52 + 20, cy + 20, S * 0.36, S * 0.115, 60, (0, 0, 0, 190))
    im.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(14)))
    d = ImageDraw.Draw(im)
    for cy in rows:
        chevron(d, S * 0.52 + 6, cy + 6, S * 0.36, S * 0.115, 60, GOLD_D)
        chevron(d, S * 0.52, cy, S * 0.36, S * 0.115, 60, GOLD)
    for i, rgb in enumerate(GEMS):  # the six gems
        gem(d, S * (0.22 + i * 0.112), S * 0.86, S * 0.042, rgb)
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), radius=150, fill=255)
    im.putalpha(mask)
    os.makedirs(out, exist_ok=True)
    im.resize((512, 512), Image.LANCZOS).save(os.path.join(out, 'icon.png'))
    im.resize((256, 256), Image.LANCZOS).save(
        os.path.join(out, 'icon.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print('icon:', os.path.join(out, 'icon.png'))


if __name__ == '__main__':
    main()
