"""Paints every (base, look) texture of the v2 wardrobe with FLUX 3 edit-image.

The input is the base garment's own texture map (export/SKV2_<Sex>_<base>_D.png, written by build_wardrobe.py); the edit
keeps its UV layout and repaints the materials from the base's piece template filled with the look's palette
(wardrobe.py). Output: art/characters_v2/looks/SKV2_<Sex>_<base>_<look>_D.png (2K master). Resumable; state (prompts,
no secrets) in art/characters_v2/looks/state.json.

Run: python -I tools/charv2/retex.py [--only C3,S1] [--sex Male] [--attempt N]
"""
import argparse, sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import genlib, wardrobe

EXPORT = genlib.ROOT / 'art' / 'characters_v2' / 'export'
OUT = genlib.ROOT / 'art' / 'characters_v2' / 'looks'
PROMPT = ('This image is a texture map (UV atlas) for a 3D garment: its pieces are laid out flat. Repaint it keeping the exact same '
          'layout, every piece in the same place and with the same outline, and every seam, edge, button, pocket and fold where it is. '
          'Only change the materials so that it shows {what}. Hollywood 1930 pulp adventure, hand-painted stylized game texture with '
          'soft painted shading in the folds, crisp stitched seams and visible but restrained brush texture. Keep the background '
          'outside the pieces as it is. No text, no logos, no people.')


def jobs(only_looks, only_sex):
    seen = set()
    for look, (pal, sexes) in wardrobe.LOOKS.items():
        if only_looks and look not in only_looks:
            continue
        for sex, slots in sexes.items():
            if only_sex and sex != only_sex:
                continue
            for slot, base in slots.items():
                if not base or (base, look, sex) in seen:
                    continue
                seen.add((base, look, sex))
                tmpl = wardrobe.BASES[base][4]
                yield sex, base, look, PROMPT.format(what=tmpl.format(**pal))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--sex', default='')
    ap.add_argument('--attempt', type=int, default=0)
    a = ap.parse_args()
    genlib.load_env()
    st = genlib.State(OUT / 'state.json')
    todo = list(jobs(set(x for x in a.only.split(',') if x), a.sex))
    print(f'{len(todo)} textures', flush=True)

    def run(job):
        sex, base, look, prompt = job
        src = EXPORT / f'SKV2_{sex}_{base}_D.png'
        if not src.exists():
            return f'missing source {src.name}'
        key = f'{sex}_{base}_{look}'
        genlib.flux_edit(key, prompt, [src], OUT / f'SKV2_{sex}_{base}_{look}_D.png', st.get(key), attempt=a.attempt)
        return 'ok ' + key

    with ThreadPoolExecutor(6) as ex:
        futs = [ex.submit(run, j) for j in todo]
        for n, f in enumerate(as_completed(futs)):
            try:
                print(n, f.result(), flush=True)
            except Exception as e:
                print(n, 'FAILED', e, flush=True)
            if n % 10 == 0:
                st.save()
    st.save()


if __name__ == '__main__':
    main()
