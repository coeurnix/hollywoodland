"""Makes the exported hair textures tintable: greyscale luminance (opaque pixels normalised to a mean of 0.55, contrast
kept), alpha untouched. The game multiplies them by the chosen hair colour (M_HLCharV2Masked's Mul).

Run: python -I tools/charv2/grey_hair.py
"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent))
import wardrobe

EXPORT = Path(__file__).resolve().parents[2] / 'art' / 'characters_v2' / 'export'
for sex, lst in wardrobe.HAIR.items():
    for stem, _ in lst:
        if not stem:
            continue
        p = EXPORT / f'SKV2_{sex}_{stem}_D.png'
        if not p.exists():
            print('missing', p.name)
            continue
        im = Image.open(p).convert('RGBA')
        a = np.asarray(im).astype(np.float32) / 255.0
        if np.abs(a[..., 0] - a[..., 1]).max() < 1e-3 and np.abs(a[..., 1] - a[..., 2]).max() < 1e-3:
            continue  # already converted (the contrast stretch must not compound)
        lum = a[..., 0] * 0.2126 + a[..., 1] * 0.7152 + a[..., 2] * 0.0722
        opaque = a[..., 3] > 0.5
        m = lum[opaque].mean() if opaque.any() else lum.mean()
        g = np.clip(0.55 + (lum - m) * 1.25 * (0.55 / max(m, 0.08)) ** 0.5, 0.0, 1.0)
        out = np.dstack([g, g, g, a[..., 3]])
        Image.fromarray((out * 255).astype(np.uint8), 'RGBA').save(p)
        print('grey', p.name, f'mean {m:.2f}')
