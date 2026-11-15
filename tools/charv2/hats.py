"""Hats the MakeHuman packs lack: a FLUX 3 reference image, then a Hyper3D Rodin model (GLB) per hat type.

Writes art/characters_v2/hats/<id>.png (reference) and <id>.glb (model); fit_hats.py places them on the heads.
Resumable; state in art/characters_v2/hats/state.json (prompts, task ids; no secrets).

Run: python -I tools/charv2/hats.py [--only top_hat,cowboy] [--images-only]
"""
import argparse, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import genlib

OUT = genlib.ROOT / 'art' / 'characters_v2' / 'hats'
REF = ('Product reference image of {what}, a single isolated hat with nobody wearing it, the whole hat in frame with margin, '
       'three-quarter view from slightly above and in front, soft even studio lighting, no shadow, plain pure white background. '
       'Stylized hand-painted 3D game asset for a 1930 Hollywood pulp adventure MMO, chunky readable shapes, rich painted '
       'materials, no text, no logos, no mannequin, no head.')
HATS = {
    'top_hat': 'a tall black silk opera top hat with a black satin band',
    'feather_headband': 'a 1920s flapper headband: a jewelled black satin band with a tall white ostrich feather plume and a deco brooch, shaped as a ring to sit around a head',
    'chitin_tiara': 'a tiara made of glossy green-black insect chitin plates with violet iridescent edges, shaped as a ring to sit on a head',
    'flying_cap': 'a brown leather aviator flying cap with ear flaps, a chin strap and round brass-rimmed goggles resting on the forehead',
    'cowboy_hat': 'a wide-brimmed tan felt western cowboy hat with a pinched crown and a leather band',
    'football_helmet': 'a 1930s brown leather football helmet with padded ribs and ear flaps, an open face with no face mask and no grille',
    'brodie_helmet': 'a WWI M1917 Brodie steel soldier helmet, olive drab, wide flat brim, with a leather chin strap',
    'chitin_helmet': 'a helmet made of glossy green-black insect chitin plates with amber rims and a tall ridged insect crest',
    'miner_helmet': 'a hard hat for a lighting electrician with a big brass carbide lamp on the front, scuffed khaki',
    'nurse_cap': 'a crisp folded white 1930s nurse cap with a small red cross on the front',
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
        ref = genlib.flux(key, REF.format(what=HATS[key]), OUT / f'{key}.png', s, 1024, 1024)
        st.save()
        if a.images_only:
            return key + ' image'
        genlib.rodin(key, [ref], OUT / f'{key}.glb', s, st.save, tier='Gen-2', quality='medium', material='PBR', mesh_mode='Raw',
                     extra={'quality_override': '3000'})
        return key + ' model'

    with ThreadPoolExecutor(5) as ex:
        for r in ex.map(one, [k for k in HATS if not only or k in only]):
            print('ok', r, flush=True)
    st.save()


if __name__ == '__main__':
    main()
