"""Check the painted maps against their layout sources and install them for the game.

    python tools/maps/install_maps.py [--force]

For each art/maps/painted/<name>.png: the registration check compares edge maps of the painting and art/maps/src/<name>.png
(streets and walls must not have moved; the game draws icons at exact coordinates on top). Passing maps are written as
Content/Data/UI/<name>.jpg plus Content/Data/UI/maps.json: {"<name>": {"uv": [u0, v0, u1, v1]}}, the part of the image that
covers the area's Min..Min+Size (zones and dungeons), or the whole image for the world map (zone MapRects are normalized).
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAPS = os.path.join(ROOT, 'art', 'maps')
UI = os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'UI')
MIN_SCORE = 0.25


def edges(img, w=384):
    g = img.convert('L').resize((w, round(w * img.height / img.width)), Image.LANCZOS).filter(ImageFilter.GaussianBlur(1.5))
    a = np.asarray(g.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.GaussianBlur(3)), dtype=np.float32)
    return (a - a.mean()) / (a.std() + 1e-6)


def score(a, b):
    """Edge correlation at the best small shift, and that shift in source pixels (should be about 0)."""
    best = (-1.0, (0, 0))
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            s = float((np.roll(a, (dy, dx), (0, 1)) * b).mean())
            if s > best[0]:
                best = (s, (dx, dy))
    return best


def main():
    force = '--force' in sys.argv
    reg = json.load(open(os.path.join(MAPS, 'maps.json'), encoding='utf-8'))
    out = {}
    ok = True
    for name, r in reg.items():
        p = os.path.join(MAPS, 'painted', name + '.png')
        if not os.path.exists(p):
            print(f'{name}: not painted yet')
            ok = False
            continue
        src = Image.open(os.path.join(MAPS, 'src', name + '.png')).convert('RGB')
        img = Image.open(p).convert('RGB')
        if img.size != src.size:
            img = img.resize(src.size, Image.LANCZOS)
        s, shift = score(edges(img), edges(src))
        scale = src.width / 384
        good = s >= MIN_SCORE and max(abs(shift[0]), abs(shift[1])) <= 2
        print(f'{name}: edge correlation {s:.2f}, best shift ({shift[0] * scale:.0f}, {shift[1] * scale:.0f}) px'
              f' -> {"ok" if good else "MISREGISTERED"}')
        if not good and not force:
            ok = False
            continue
        img.save(os.path.join(UI, name + '.jpg'), quality=88, optimize=True, progressive=False)
        x, y, w, h = r['rect']
        W, H = r['size']
        out[name] = {'uv': [round(x / W, 5), round(y / H, 5), round((x + w) / W, 5), round((y + h) / H, 5)]}
    with open(os.path.join(UI, 'maps.json'), 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=1)
    print(f'installed {len(out)} maps into {UI}')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
