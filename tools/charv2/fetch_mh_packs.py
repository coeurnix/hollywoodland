"""Downloads the MakeHuman community asset packs the v2 characters draw on (clothes, hair, hats, shoes, skins, facial hair).

CC0 packs need no attribution; CC-BY packs are credited in art/characters_v2/LICENSES.md. Everything lands in
tmp/character-art-source/mh_packs/<pack>/ (zip + extracted), which is a local cache and never committed.

Run: python -I tools/charv2/fetch_mh_packs.py
"""
import os, sys, zipfile, time
from pathlib import Path
import requests

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tmp' / 'character-art-source' / 'mh_packs'
MIRRORS = ['https://files.makehumancommunity.org/asset_packs', 'https://files2.makehumancommunity.org/asset_packs']
CC0 = ['system_clothes_materials01', 'system_hair_materials01', 'suits01', 'suits02', 'shirts01', 'pants01', 'shoes01', 'hats01', 'hats02',
       'dress01', 'skirts01', 'gloves01', 'hair01', 'bodyparts05', 'eyebrows01', 'eyelashes01', 'skins01', 'skins02',
       'underwear04', 'glasses01', 'equipment01', 'hands01', 'nose01', 'cheek01', 'ears01']
CCBY = ['hair02', 'hair03', 'hats03', 'hats04', 'shirts02', 'shirts03', 'shoes02', 'shoes03', 'suits03', 'pants02',
        'dress02', 'dress03', 'skirts02', 'bodyparts06', 'glasses02', 'equipment02', 'equipment03']


def fetch(pack, lic, http):
    d = OUT / pack
    z = d / f'{pack}_{lic}.zip'
    if (d / 'extracted').exists():
        return 'cached'
    d.mkdir(parents=True, exist_ok=True)
    last = None
    for m in MIRRORS:
        url = f'{m}/{pack}/{pack}_{lic}.zip'
        try:
            r = http.get(url, timeout=600, stream=True)
            if r.status_code != 200:
                last = f'{r.status_code} {url}'
                continue
            with open(z, 'wb') as f:
                for chunk in r.iter_content(1 << 20):
                    f.write(chunk)
            with zipfile.ZipFile(z) as zf:
                zf.extractall(d / 'extracted')
            return f'{z.stat().st_size >> 20} MB'
        except Exception as e:  # try the other mirror
            last = f'{type(e).__name__}: {e}'
    return f'FAILED ({last})'


def main():
    http = requests.Session()
    http.headers['User-Agent'] = 'asset-fetch/1.0'
    for pack in CC0 + CCBY:
        lic = 'cc0' if pack in CC0 else 'ccby'
        t = time.time()
        print(f'{pack:28s} {lic:5s} {fetch(pack, lic, http)}  ({time.time() - t:.0f}s)', flush=True)


if __name__ == '__main__':
    main()
