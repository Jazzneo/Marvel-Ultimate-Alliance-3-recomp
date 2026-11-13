"""MUA3 Recomp's own icon, drawn here (no game art, no logos): the launcher's colours - a dark night-blue square, a red
diagonal band, and three gold chevrons stacked into a "3". The release's exes and windows use it (Makefile RELEASE=1);
a source build with your own dump still uses the game's icon (tools/make_icon.py).

Usage: make_original_icon.py <out dir>   ->  <out>/icon.png (512x512) and <out>/icon.ico (16..256)
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

NIGHT, NIGHT2 = (16, 19, 30, 255), (28, 33, 52, 255)
RED, RED_D = (186, 26, 26, 255), (120, 14, 18, 255)
GOLD, GOLD_D = (255, 200, 38, 255), (196, 128, 10, 255)
S = 1024  # drawn large, scaled down


def chevron(d, cx, cy, w, h, t, col):
    """A chevron pointing right: its tip at (cx + w / 2, cy), arms h tall, a stroke t thick."""
    x0, x1 = cx - w / 2, cx + w / 2
    d.line([(x0, cy - h / 2), (x1, cy), (x0, cy + h / 2)], fill=col, width=int(t), joint='curve')
    for p in ((x0, cy - h / 2), (x0, cy + h / 2)):  # (square-ish ends)
        d.ellipse([p[0] - t / 2, p[1] - t / 2, p[0] + t / 2, p[1] + t / 2], fill=col)


def main():
    out = sys.argv[1]
    im = Image.new('RGBA', (S, S), NIGHT)
    d = ImageDraw.Draw(im)
    for y in range(S):  # a soft vertical gradient
        k = y / S
        d.line([(0, y), (S, y)], fill=tuple(int(NIGHT2[i] * (1 - k) + NIGHT[i] * k) for i in range(3)) + (255,))
    d.polygon([(0, S * 0.58), (S, S * 0.20), (S, S * 0.44), (0, S * 0.82)], fill=RED_D)  # the red band, with a lip
    d.polygon([(0, S * 0.60), (S, S * 0.22), (S, S * 0.40), (0, S * 0.78)], fill=RED)
    # three gold chevrons, a shadow under each
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    for i, cy in enumerate((S * 0.28, S * 0.50, S * 0.72)):
        chevron(g, S * 0.52 + 22, cy + 22, S * 0.36, S * 0.115, 60, (0, 0, 0, 170))
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(14)))
    for cy in (S * 0.28, S * 0.50, S * 0.72):
        chevron(d, S * 0.52 + 6, cy + 6, S * 0.36, S * 0.115, 60, GOLD_D)
        chevron(d, S * 0.52, cy, S * 0.36, S * 0.115, 60, GOLD)
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
