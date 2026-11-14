"""Turns the FLUX grass/flower images (art/foliage/src, tools/foliagegen.py) into alpha sprites and crossed-card GLBs.

Sprite: the white background is flood-filled from the image border (near-white pixels connected to the edge become
transparent, so white petals inside the clump survive), a one-pixel feather softens the cut, the clump is cropped to its
bounds and padded square to 512 px. Transparent pixels take the clump's mean colour, so mip levels do not halo white.
Card: tools/foliage_cards_blender.py builds three vertical quads crossed at 60 degrees, sized to the asset's height
(art/foliage/assets.json), pivot at the bottom centre, front +Z (art/props conventions), and exports a GLB.

Writes art/foliage/sprites/<id>.png and art/foliage/models/<id>.glb.
Run: python -I tools/foliage_cards.py
"""
import json, os, subprocess, sys
from collections import deque
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
F = os.path.join(ROOT, 'art', 'foliage')
BLENDER = os.environ.get('BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.3\blender.exe')
WHITE = 212  # light and colourless (spread under 20): background candidates


def key(path, max_hole=450):
    im = Image.open(path).convert('RGB')
    im.thumbnail((768, 768), Image.LANCZOS)
    w, h = im.size
    px = im.load()
    # near-white regions: background when they touch the border or are too big to be petals (gaps between blades)
    white = bytearray(1 if min(px[x, y]) >= WHITE and max(px[x, y]) - min(px[x, y]) < 20 else 0 for y in range(h) for x in range(w))
    bg = bytearray(w * h)
    seen = bytearray(w * h)
    for s in range(w * h):
        if not white[s] or seen[s]:
            continue
        comp, edge, q = [], False, deque([s])
        seen[s] = 1
        while q:
            i = q.popleft()
            comp.append(i)
            x, y = i % w, i // w
            if x == 0 or y == 0 or x == w - 1 or y == h - 1:
                edge = True
            for n in ((i - 1) if x > 0 else -1, (i + 1) if x < w - 1 else -1, (i - w) if y > 0 else -1, (i + w) if y < h - 1 else -1):
                if n >= 0 and white[n] and not seen[n]:
                    seen[n] = 1
                    q.append(n)
        if edge or len(comp) > max_hole:
            for i in comp:
                bg[i] = 1
    a = Image.new('L', (w, h))
    a.putdata([0 if v else 255 for v in bg])
    a = a.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.7))
    box = a.point(lambda v: 255 if v > 40 else 0).getbbox()
    rgba = im.convert('RGBA')
    rgba.putalpha(a)
    rgba = rgba.crop(box)
    # mean clump colour under the transparent parts
    data = list(rgba.getdata())
    op = [p for p in data if p[3] > 200]
    mean = tuple(sum(p[k] for p in op) // max(1, len(op)) for k in range(3))
    rgba.putdata([p if p[3] > 8 else mean + (p[3],) for p in data])
    # pad square, bottom-aligned (the card stands on its bottom edge)
    cw, ch = rgba.size
    s = max(cw, ch)
    sq = Image.new('RGBA', (s, s), mean + (0,))
    sq.paste(rgba, ((s - cw) // 2, s - ch))
    sq = sq.resize((512, 512), Image.LANCZOS)
    return sq, cw / ch


def main():
    assets = json.load(open(os.path.join(F, 'assets.json')))
    os.makedirs(os.path.join(F, 'sprites'), exist_ok=True)
    os.makedirs(os.path.join(F, 'models'), exist_ok=True)
    jobs = []
    for k, v in assets.items():
        if v['kind'] != 'card':
            continue
        sp, aspect = key(os.path.join(F, "src", f"{k}.png"), 450 if k == "flower_daisy" else 0)
        out = os.path.join(F, 'sprites', f'{k}.png')
        sp.save(out)
        # the square sprite spans height x height; the clump's own width is aspect * height inside it
        jobs.append({'id': k, 'sprite': out, 'height': v['height'], 'glb': os.path.join(F, 'models', f'{k}.glb')})
        print(f'sprite {k} aspect {aspect:.2f}')
    jf = os.path.join(F, 'cards_jobs.json')
    json.dump(jobs, open(jf, 'w'), indent=1)
    r = subprocess.run([BLENDER, '--background', '--factory-startup', '--python', os.path.join(ROOT, 'tools', 'foliage_cards_blender.py'), '--', jf], capture_output=True, text=True)
    lines = [l for l in r.stdout.splitlines() if l.startswith('HL_CARD')]
    print('\n'.join(lines))
    os.remove(jf)
    if r.returncode != 0 or len(lines) < len(jobs):
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit(1)


main()
