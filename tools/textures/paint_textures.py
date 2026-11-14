"""Paint the world's ground tiles and sky panoramas (FLUX 3 Klein on fal.ai), then make them tile.

    python tools/textures/paint_textures.py [names...]   # art/textures/raw/<name>.png, in parallel; skips existing
    python tools/textures/tile_textures.py               # seamless wrap + sizes -> art/textures/<name>.png
    hollywoodland-ue\\Scripts\\MakeAssets.ps1              # imports them into /Game/HL/Tex and builds the materials

Ground tiles are square, seen from straight above, and must repeat with no visible seam; skies are equirectangular
panoramas (the horizon on the middle line) that wrap left to right. The fal key comes from the environment or the
repository's .env (FAL_AI_API_KEY); it is never printed or written anywhere.
"""
import os
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import requests

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'art' / 'textures' / 'raw'
FAL_MODEL = 'blackforestlabs/flux-3/klein/text-to-image'

TILE = ("Seamless tileable game texture, viewed from directly above, orthographic, flat even lighting, no horizon, no "
        "perspective, fills the whole frame edge to edge, uniform density with no single feature that stands out when "
        "repeated. Stylised hand-painted texture for a warm 1930s Hollywood fantasy MMORPG, painterly, gently saturated. ")

JOBS = {
    'T_GND_Asphalt': (1024, 1024, TILE + "1930 city street asphalt: dark charcoal grey tarmac, fine aggregate, faint tyre "
                                         "polish, a few hairline cracks and small tar patches, no road markings."),
    'T_GND_Sidewalk': (1024, 1024, TILE + "Pale warm grey poured concrete sidewalk slabs in a regular 2 by 2 grid of "
                                          "square slabs, scored joints, light weathering, a few hairline cracks."),
    'T_GND_Lot': (1024, 1024, TILE + "Southern California vacant lot ground: patchy dry grass in sunny greens and straw "
                                     "with packed tan dirt showing through, small pebbles, tiny weeds, evenly spread."),
    'T_GND_Grass': (1024, 1024, TILE + "Well kept lawn: lush grass in warm greens with gentle tonal variation and a "
                                       "little clover, no flowers."),
    'T_GND_Blight': (1024, 1024, TILE + "Alien hive blight over the ground: dark violet organic resin with glossy veins, "
                                        "crusted chitin flakes, faint glowing magenta pores, dirt showing through."),
    'T_GND_Theatre': (1024, 1024, TILE + "Worn movie palace carpet, deep red with a small repeating gold Egyptian lotus "
                                         "motif, a few violet resin stains."),
    'T_SKY_Day': (2048, 1024, "Equirectangular 360 degree sky panorama for a game sky dome, hand-painted 1930s Hollywood "
                              "studio matte painting. The horizon is a straight line across the exact middle of the image. "
                              "Upper half: clear warm California blue sky overhead fading to hazy golden cream near the "
                              "horizon, a few soft painted cumulus clouds, very faint huge violet hexagonal lattice of an "
                              "alien dome high in the sky. Along the horizon a low distant silhouette of hills in dusty "
                              "blue-violet haze with palm trees. Lower half: plain soft violet grey haze. No text, no sun."),
    'T_SKY_Night': (2048, 1024, "Equirectangular 360 degree night sky panorama for a game sky dome, hand-painted 1930s "
                                "Hollywood studio matte painting. The horizon is a straight line across the exact middle of "
                                "the image. Upper half: deep indigo night sky with many small stars and a faint milky band, a "
                                "softly glowing violet and teal hexagonal alien dome lattice overhead. Along the horizon dark "
                                "hill silhouettes, a warm amber city glow, two pale searchlight beams fanning up. Lower half: "
                                "plain dark indigo haze. No text, no moon."),
}


def load_env():
    env = ROOT / '.env'
    if env.exists():
        for line in env.read_text(encoding='utf-8').splitlines():
            m = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$', line)
            if m and not line.lstrip().startswith('#'):
                os.environ.setdefault(m.group(1), m.group(2).strip().strip('"').strip("'"))
    key = os.environ.get('FAL_KEY') or os.environ.get('FAL_AI_API_KEY')
    if not key: sys.exit('FAL_AI_API_KEY is not set (environment or .env).')
    return {'Authorization': f'Key {key}', 'Content-Type': 'application/json'}


def run(name, H):
    out = OUT / f'{name}.png'
    if out.exists(): print(f'{name}: exists'); return True
    w, h, prompt = JOBS[name]
    body = {'prompt': prompt, 'image_size': {'width': w, 'height': h}, 'num_images': 1, 'seed': abs(hash(name)) % 2**31,
            'enable_prompt_expansion': False, 'output_format': 'png', 'enable_safety_checker': True}
    try:
        j = requests.post(f'https://queue.fal.run/{FAL_MODEL}', headers=H, json=body, timeout=60).json()
        t0 = time.time()
        while True:
            s = requests.get(j['status_url'], headers=H, timeout=60).json()
            if s.get('status') == 'COMPLETED': break
            if time.time() - t0 > 600: raise TimeoutError('queued 10 minutes')
            time.sleep(2)
        res = requests.get(j['response_url'], headers=H, timeout=60).json()
        img = requests.get(res['images'][0]['url'], timeout=120)
        img.raise_for_status()
        OUT.mkdir(parents=True, exist_ok=True)
        out.write_bytes(img.content)
        print(f'{name}: saved', flush=True)
        return True
    except Exception as e:  # noqa: BLE001 - report and carry on with the others
        print(f'{name}: FAILED {type(e).__name__}: {str(e)[:200]}', flush=True)
        return False


if __name__ == '__main__':
    H = load_env()
    names = sys.argv[1:] or list(JOBS)
    with ThreadPoolExecutor(len(names)) as pool:
        ok = list(pool.map(lambda n: run(n, H), names))
    sys.exit(0 if all(ok) else 1)
