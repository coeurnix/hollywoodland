"""Make the painted textures wrap: art/textures/raw/<name>.png -> art/textures/<name>.png.

Ground tiles (T_GND_*) wrap both ways, skies (T_SKY_*) left to right only. A texture whose wrap seam is already small is
left alone; otherwise the half-offset copy (continuous across the edges) is blended in near the borders.
"""
import os
import sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, 'art', 'textures', 'raw')
OUT = os.path.join(ROOT, 'art', 'textures')
SIZES = {'T_GND': (1024, 1024), 'T_SKY': (2048, 1024)}
# where the painted horizon actually sits (fraction of the height); skies are shifted so it lands on the middle line
# mean colour (sRGB) a tile is graded to, so the painted texture sits at the brightness of the world around it
TARGET = {'T_GND_Lot': (118, 126, 86)}
HORIZON = {'T_SKY_Day': 0.53, 'T_SKY_Night': 0.58}


def seam(a, axis):
    # mean step across the wrap edge relative to the mean step between neighbouring pixels inside the image
    edge = np.abs(a.take(0, axis) - a.take(-1, axis)).mean()
    inner = np.abs(np.diff(a, axis=axis)).mean() + 1e-6
    return edge / inner


def wrap(a, both):
    h, w = a.shape[:2]
    s = np.roll(a, w // 2, axis=1)
    if both: s = np.roll(s, h // 2, axis=0)
    x = np.abs(np.linspace(-1, 1, w))[None, :]
    m = x
    if both:
        y = np.abs(np.linspace(-1, 1, h))[:, None]
        m = np.maximum(x, y)
    m = np.clip((m - 0.55) / 0.4, 0, 1)
    m = m * m * (3 - 2 * m)
    return a * (1 - m[..., None]) + s * m[..., None]


def main(names):
    os.makedirs(OUT, exist_ok=True)
    for f in sorted(os.listdir(RAW)):
        name = f[:-4]
        if not f.endswith('.png') or (names and name not in names): continue
        size = SIZES[name[:5]]
        im = Image.open(os.path.join(RAW, f)).convert('RGB').resize(size, Image.LANCZOS)
        a = np.asarray(im, dtype=np.float32)
        both = name.startswith('T_GND')
        sx = seam(a, 1); sy = seam(a, 0) if both else 0
        fixed = sx > 1.6 or sy > 1.6
        if fixed: a = wrap(a, both)
        if name in TARGET:
            mean = a.reshape(-1, 3).mean(0)
            g = a.mean(2, keepdims=True)
            a = g + (a - g) * 0.6  # calmer saturation
            a = a * (np.array(TARGET[name], dtype=np.float32) / a.reshape(-1, 3).mean(0))
        if name in HORIZON:
            d = int(round((HORIZON[name] - 0.5) * a.shape[0]))
            if d > 0: a = np.concatenate([a[d:], np.repeat(a[-1:], d, axis=0)], axis=0)
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(OUT, name + '.png'))
        print(f'{name}: seam x {sx:.2f} y {sy:.2f} -> {"blended" if fixed else "as painted"}')


if __name__ == '__main__':
    main(sys.argv[1:])
