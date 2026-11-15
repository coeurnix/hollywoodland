"""The character-select backdrop: the Hollywoodland sign's ridge at sunset (the reference is a dark scrub-covered ridge with
the letters across its brow, lower hills fading into haze in front, an orange cloud sky with the sun setting on the left).

- title_ridge, title_hills: a FLUX 3 reference image each, then a Hyper3D Rodin model (GLB) ->
  art/world/models/scene/<name>.glb (tools/propprep.py stages them as the props title_ridge / title_hills)
- T_SKY_Sunset: an equirectangular sunset sky for the title's sky dome -> art/textures/T_SKY_Sunset.png

Resumable; state in art/world/models/scene/state.json (prompts, task ids; no secrets).
Run: python -I tools/titlegen.py [--only title_ridge,sky] [--images-only]
"""
import argparse, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent / 'charv2'))
import genlib

OUT = genlib.ROOT / 'art' / 'world' / 'models' / 'scene'
TEX = genlib.ROOT / 'art' / 'textures'
REF = ('Product reference image of a 3D game terrain asset: {what}. A single isolated landform, the whole of it in frame with '
       'margin, seen from the front and slightly below, soft even overcast light, no cast shadow, plain pure white background, '
       'nothing under it. Painterly but realistic, no text, no letters, no sign, no buildings, no people.')
MODELS = {
    'title_ridge': 'a long, steep mountain ridge like Mount Lee in the Hollywood Hills, about four times as wide as it is tall, '
                   'its crest rising gently from the left to a rounded summit on the right, the slope facing us covered in dense '
                   'dark green chaparral and scrub oak with dusty tan rocky gullies and a few bare earth firebreaks, a flat '
                   'back and base like a cut-out landscape piece',
    'title_hills': 'a low range of three rounded foothills side by side, about five times as wide as they are tall, covered in '
                   'dark olive chaparral, scattered oak trees and dry golden grass, a flat back and base like a cut-out '
                   'landscape piece',
}
SKY = ('Equirectangular 360 degree panorama of a dramatic golden-hour sunset sky over Los Angeles, 2:1. The horizon is a perfectly '
       'straight line exactly across the vertical middle of the image. Above it: layers of glowing orange, coral and rose clouds, '
       'brightest and most golden low on the left third where the sun is just touching the horizon with a soft warm glare, '
       'fading to dusky violet and deep blue toward the top edge and toward the right. Below the horizon: plain dark warm brown '
       'haze, no land features, no hills, no buildings, no water. Photographic, cinematic, no text.')


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
        if key == 'sky':
            genlib.flux(key, SKY, TEX / 'T_SKY_Sunset.png', s, 2048, 1024)
            st.save()
            return 'sky image'
        ref = genlib.flux(key, REF.format(what=MODELS[key]), OUT / f'{key}.png', s, 1536, 1024)
        st.save()
        if a.images_only:
            return key + ' image'
        genlib.rodin(key, [ref], OUT / f'{key}.glb', s, st.save, tier='Gen-2', quality='high', material='PBR', mesh_mode='Raw',
                     extra={'quality_override': '30000'})
        return key + ' model'

    keys = [k for k in ['sky', *MODELS] if not only or k in only]
    with ThreadPoolExecutor(3) as ex:
        for r in ex.map(one, keys):
            print('ok', r, flush=True)
    st.save()


if __name__ == '__main__':
    main()
