"""Belts, wrist pieces and cuirasses: a FLUX 3 reference image, then a Hyper3D Rodin model (GLB) per piece.

Writes art/characters_v2/gear/<id>.png (reference) and <id>.glb (model); fit_gear.py fits them to the bodies.
Resumable; state in art/characters_v2/gear/state.json (prompts, task ids; no secrets).

Run: python -I tools/charv2/gear.py [--only belt_slim] [--images-only]
"""
import argparse, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import genlib

OUT = genlib.ROOT / 'art' / 'characters_v2' / 'gear'
REF = ('Product reference image of {what}, a single isolated piece with nobody wearing it, the whole piece in frame with margin, '
       'three-quarter view from slightly above and in front, soft even studio lighting, no shadow, plain pure white background. '
       'Stylized hand-painted 3D game asset for a 1930 Hollywood pulp adventure MMO, chunky readable shapes, rich painted '
       'materials, no text, no logos, no mannequin, no body.')
GEAR = {
    'belt_slim': 'a slim brown leather trouser belt with a polished brass buckle, closed into a ring as if worn around a waist, seen as an oval loop',
    'belt_wide': 'a wide brown leather adventurer utility belt with a big square brass buckle, two small pouches and rivets, closed into a ring as if worn around a hip, seen as an oval loop',
    'cuff': 'a single leather wrist cuff with a small round brass wristwatch on it, closed into a short cylinder as if worn around a wrist',
    'bracer': 'a single long leather forearm bracer with brass rivets and laces, closed into a long tapered tube as if worn around a forearm, longer than it is wide',
    'cuirass_m': 'a complete closed steel cuirass torso armour for a broad-shouldered man, breastplate and backplate joined at the sides with straps, riveted steel with art-deco stepped trim, open at the neck, arms and waist, standing upright as if worn',
    'cuirass_f': 'a complete closed steel cuirass torso armour shaped for a woman with a sculpted bust and narrow waist, breastplate and backplate joined at the sides with straps, riveted steel with art-deco stepped trim, open at the neck, arms and waist, standing upright as if worn',
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--images-only', action='store_true')
    a = ap.parse_args()
    genlib.load_env()
    st = genlib.State(OUT / 'state.json')
    only = set(x for x in a.only.split(',') if x)

    def one(key):
        s = st.get(key)
        ref = genlib.flux(key, REF.format(what=GEAR[key]), OUT / f'{key}.png', s, 1024, 1024)
        st.save()
        if a.images_only:
            return key + ' image'
        genlib.rodin(key, [ref], OUT / f'{key}.glb', s, st.save, tier='Gen-2', quality='medium', material='PBR', mesh_mode='Raw',
                     extra={'quality_override': '3000'})
        return key + ' model'

    with ThreadPoolExecutor(5) as ex:
        for r in ex.map(one, [k for k in GEAR if not only or k in only]):
            print('ok', r, flush=True)
    st.save()


if __name__ == '__main__':
    main()
