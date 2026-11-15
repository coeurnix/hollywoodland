"""Painted makeup for the female skin: rose lips, a dark lash line with a soft smoky lid, and blush, onto MakeHuman's
young_caucasian_female2 diffuse (CC0). Writes art/characters_v2/src/skins/hl_female/ (mhmat + diffuse), which
build_body.py uses for the female body. Coordinates are in the 2048 atlas, where the face lies on its side (forehead to
the left, chin to the right).

Run: python -I tools/charv2/makeup.py
"""
from pathlib import Path
import shutil
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'tmp' / 'character-art-source' / 'mh_system' / 'skins' / 'young_caucasian_female2'
OUT = ROOT / 'art' / 'characters_v2' / 'src' / 'skins' / 'hl_female'

EYES = [(1712, 975), (1712, 1125)]  # (x, y) pupil centres; the eye runs along y, the upper lid is towards smaller x
LIPS = (1866, 1052)
CHEEKS = [(1782, 902), (1782, 1201)]


def blob(shape, cx, cy, rx, ry, power=2.0):
    h, w = shape
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
    return np.clip(1.0 - d, 0.0, 1.0) ** power


def tint(img, mask, colour, strength):
    """Multiply-blend towards colour (0..1 rgb) by mask * strength."""
    m = (mask * strength)[..., None]
    return img * (1 - m) + img * np.array(colour, np.float32) * m


def overlay(img, mask, colour, strength):
    m = (mask * strength)[..., None]
    return img * (1 - m) + np.array(colour, np.float32) * m


def main():
    im = np.asarray(Image.open(SRC / 'young_lightskinned_female_diffuse2.png').convert('RGB')).astype(np.float32) / 255.0
    sh = im.shape[:2]
    # lips: deepen and warm the existing lip colour only where the paint is already red (keeps the lip outline)
    lx, ly = LIPS
    region = blob(sh, lx, ly, 34, 52, 1.0)
    red = np.clip((im[..., 0] - im[..., 1] - 0.10) * 5.0, 0, 1)
    lipm = np.asarray(Image.fromarray((region * red * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2))).astype(np.float32) / 255
    im = tint(im, lipm, (0.88, 0.46, 0.52), 0.7)
    im = overlay(im, lipm * blob(sh, lx - 6, ly, 10, 26, 1.0), (1.0, 0.86, 0.88), 0.18)  # a little gloss on the lower lip
    for ex, ey in EYES:
        # smoky lid: a soft mauve-brown gradient above the eye, fading towards the brow
        lid = blob(sh, ex - 16, ey, 22, 44, 1.5)
        lid *= np.clip((np.mgrid[0:sh[0], 0:sh[1]][1] - (ex - 40)) / 26.0, 0, 1)  # not past the brow line
        lid *= np.clip((ex - 4 - np.mgrid[0:sh[0], 0:sh[1]][1]) / 6.0, 0, 1)  # stop at the lash line
        im = tint(im, lid, (0.62, 0.48, 0.52), 0.55)
        # lash line: a thin dark arc along the upper lid, a winged tip towards the outer corner
        ring = blob(sh, ex - 2, ey, 15, 34, 1.0) - blob(sh, ex - 1, ey, 11, 29, 1.0)
        upper = np.clip((ex + 2 - np.mgrid[0:sh[0], 0:sh[1]][1]) / 4.0, 0, 1)
        liner = np.clip(ring * 3.0, 0, 1) * upper
        im = tint(im, liner, (0.18, 0.12, 0.13), 0.8)
        # lower lash line, lighter
        lower = np.clip((np.mgrid[0:sh[0], 0:sh[1]][1] - (ex + 6)) / 4.0, 0, 1)
        im = tint(im, np.clip(ring * 3.0, 0, 1) * lower, (0.35, 0.26, 0.28), 0.35)
    for cx, cy in CHEEKS:
        im = tint(im, blob(sh, cx, cy, 40, 58, 2.5), (1.0, 0.74, 0.74), 0.3)
    OUT.mkdir(parents=True, exist_ok=True)
    Image.fromarray((np.clip(im, 0, 1) * 255).astype(np.uint8)).save(OUT / 'hl_female_diffuse.png')
    mat = (SRC / 'young_caucasian_female2.mhmat').read_text(encoding='utf-8')
    mat = mat.replace('name young_caucasian_female2', 'name hl_female').replace('young_lightskinned_female_diffuse2.png', 'hl_female_diffuse.png')
    (OUT / 'hl_female.mhmat').write_text(mat, encoding='utf-8')
    face = Image.open(OUT / 'hl_female_diffuse.png').crop((1600, 850, 1950, 1250)).rotate(-90, expand=True).resize((800, 700))
    face.save(ROOT / 'art' / 'characters_v2' / 'review' / 'makeup_face.png')
    print('makeup ok')


if __name__ == '__main__':
    main()
