"""Compose a 2048 atlas from four 1024 quadrant textures written by fit_rodin.py (work/quadtex/<atlas>_<q>.png).

    python -I art/buildings/src/pack_quads.py ATL_BD1 [ATL_BD2 ...]
Quadrant q: column q % 2, row q // 2 (row 0 at the top of the image).
"""
import sys
from pathlib import Path
from PIL import Image

R = Path(__file__).resolve().parents[1]
for atl in sys.argv[1:]:
    out = Image.new('RGB', (2048, 2048), (128, 118, 100))
    for q in range(4):
        p = R / 'work' / 'quadtex' / f'{atl}_{q}.png'
        if p.exists(): out.paste(Image.open(p).convert('RGB').resize((1024, 1024)), ((q % 2) * 1024, (q // 2) * 1024))
    out.save(R / 'textures' / f'T_{atl}.png'); print('packed', atl)
