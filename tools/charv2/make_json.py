"""Writes hollywoodland-ue/Content/Data/characters_v2.json, the runtime table for the v2 characters (HLCharacter.cpp):
per look, sex and slot the garment mesh, its painted texture, its layer and the zones it covers; the hair styles; the
skin tones. Only pieces whose mesh and texture exist in art/characters_v2 are listed.

Run: python -I tools/charv2/make_json.py
"""
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import wardrobe

ROOT = Path(__file__).resolve().parents[2]
EXPORT = ROOT / 'art' / 'characters_v2' / 'export'
LOOKTEX = ROOT / 'art' / 'characters_v2' / 'looks'
OUT = ROOT / 'hollywoodland-ue' / 'Content' / 'Data' / 'characters_v2.json'
ZONES = ['head', 'neck', 'chest', 'belly', 'hips', 'upperarm', 'forearm', 'hand', 'thigh', 'calf', 'foot']  # HLZone bits
LAYER = {'shirt': 1, 'trousers': 2, 'shoes': 3, 'coat': 4, 'gloves': 5, 'belt': 6, 'wrist': 6}
# hair styles per sex: (export piece name, label); filled out in the hair pass
HAIR = wardrobe.HAIR
# eight skin tones as a multiply on the base skin (01 lightest .. 08 darkest)
SKIN = [(1.0, 0.97, 0.95), (0.97, 0.88, 0.80), (0.92, 0.78, 0.66), (0.84, 0.66, 0.52), (0.72, 0.54, 0.40), (0.60, 0.43, 0.31),
        (0.47, 0.33, 0.24), (0.36, 0.25, 0.19)]


def mask(names):
    m = 0
    for z in names:
        m |= 1 << ZONES.index(z)
    return m


def main():
    looks = {}
    missing = []
    for look, (pal, sexes) in wardrobe.LOOKS.items():
        for sex, slots in sexes.items():
            for slot, base in slots.items():
                if not base:
                    continue
                bsex, stem, pieces, covers, _ = wardrobe.BASES[base]
                piece = next(p for p, s in pieces.items() if s == slot)
                mesh = f'SKV2_{sex}_{base}_{piece}'
                tex = f'SKV2_{sex}_{base}_{look}_D'
                if not (EXPORT / f'{mesh}.fbx').exists():
                    missing.append(mesh)
                    continue
                if not (LOOKTEX / f'{tex}.png').exists():
                    tex = f'SKV2_{sex}_{base}_D'  # the base's own texture until the look's paint exists
                looks.setdefault(look, {}).setdefault(sex, {})[slot] = {
                    'mesh': mesh, 'tex': 'T_' + tex, 'layer': LAYER[slot], 'covers': mask(covers.get(piece, []))}
    hair = {sex: [{'mesh': f'SKV2_{sex}_{m}_hair', 'tex': f'T_SKV2_{sex}_{m}_D', 'name': n} for m, n in lst
                  if (EXPORT / f'SKV2_{sex}_{m}_hair.fbx').exists()] for sex, lst in HAIR.items()}
    data = {'version': 2, 'zones': ZONES, 'looks': looks, 'hair': hair, 'skin': [list(c) for c in SKIN]}
    OUT.write_text(json.dumps(data, indent=1), encoding='utf-8')
    print(f'wrote {OUT.name}: {len(looks)} looks, hair {[len(v) for v in hair.values()]}, missing meshes {sorted(set(missing))}')


if __name__ == '__main__':
    main()
