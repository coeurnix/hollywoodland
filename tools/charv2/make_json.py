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
LAYER = {'shirt': 1, 'trousers': 2, 'shoes': 3, 'coat': 4, 'gloves': 5, 'belt': 6, 'wrist': 6, 'armour': 7}
# hair styles per sex: (export piece name, label); filled out in the hair pass
HAIR = wardrobe.HAIR
# eight skin tones as a multiply on the base skin (01 lightest .. 08 darkest)
SKIN = [(1.0, 0.97, 0.95), (0.97, 0.88, 0.80), (0.92, 0.78, 0.66), (0.84, 0.66, 0.52), (0.72, 0.54, 0.40), (0.60, 0.43, 0.31),
        (0.47, 0.33, 0.24), (0.36, 0.25, 0.19)]


# the hat each look wears, (male, female); fit_hats.py makes them (the MakeHuman M1 and stunt helmets are too heavy for
# the web package: 60k-170k triangles, so the steel and racing looks use the Rodin Brodie helmet and flying cap)
HATS = {'C1': ('newsboy', 'cloche'), 'C2': ('fedora', 'cloche'), 'C3': ('fedora', 'fedora_tilted'), 'C4': ('top_hat', 'feather_headband'),
        'C5': ('fedora', 'feather_headband'), 'C6': ('fedora', 'chitin_tiara'), 'L1': ('flying_cap', 'flying_cap'), 'L2': ('flying_cap', 'flying_cap'),
        'L3': ('cowboy_hat', 'cowboy_hat'), 'L4': ('flying_cap', 'flying_cap'), 'L5': ('fedora', 'fedora_tilted'), 'L6': ('chitin_helmet', 'chitin_helmet'),
        'P1': ('newsboy', 'newsboy'), 'P2': ('newsboy', 'newsboy'), 'P3': ('football_helmet', 'football_helmet'), 'P4': ('flying_cap', 'flying_cap'),
        'P5': ('flying_cap', 'flying_cap'), 'P6': ('chitin_helmet', 'chitin_helmet'), 'S1': ('brodie_helmet', 'brodie_helmet'),
        'S2': ('brodie_helmet', 'brodie_helmet'), 'S3': ('brodie_helmet', 'brodie_helmet'), 'S4': ('warrior_helmet', 'warrior_helmet'),
        'S5': ('warrior_helmet', 'warrior_helmet'), 'S6': ('chitin_helmet', 'chitin_helmet'), 'E1': ('brodie_helmet', 'brodie_helmet'),
        'E2': ('flying_cap', 'flying_cap'), 'E3': ('cowboy_hat', 'cowboy_hat'), 'E4': ('miner_helmet', 'miner_helmet'),
        'E5': ('flying_cap', 'flying_cap'), 'E6': ('nurse_cap', 'nurse_cap'), 'E7': ('fedora', 'fedora_tilted'), 'E8': ('newsboy', 'newsboy')}
OPEN_HATS = {'feather_headband', 'chitin_tiara', 'nurse_cap'}  # the hair shows in full under these
CLOSED_HATS = {'football_helmet'}  # covers scalp and ears: no hair at all (the flying cap shows its hat-safe hair under the rim)


def mask(names):
    m = 0
    for z in names:
        m |= 1 << ZONES.index(z)
    return m


COVERAGE = json.loads((EXPORT / 'coverage.json').read_text()) if (EXPORT / 'coverage.json').exists() else {}


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
                # hide only what the garment really covers (coverage.py measures it on the exported meshes): a short
                # sleeve declared as covering the arm used to sink the bare part into a stick
                zones = covers.get(piece, [])
                if mesh in COVERAGE:
                    zones = [z for z in zones if COVERAGE[mesh].get(z, 0) >= (0.6 if z in ("hand", "foot") else 0.94)]  # rays slip between fingers
                looks.setdefault(look, {}).setdefault(sex, {})[slot] = {
                    'mesh': mesh, 'tex': 'T_' + tex, 'layer': LAYER[slot], 'covers': mask(zones)}
    # Rodin gear: belts, wrist pieces, the steel looks' cuirasses (fit_gear.py), painted per look (retex.py --gear)
    for look in wardrobe.LOOKS:
        for sex in ('Male', 'Female'):
            for slot, key in wardrobe.gear_for(look, sex).items():
                mesh = f'SKV2_{sex}_{key}_{slot}'
                if not (EXPORT / f'{mesh}.fbx').exists():
                    missing.append(mesh)
                    continue
                tex = f'SKV2_GEAR_{key}_{look}_D'
                if not (LOOKTEX / f'{tex}.png').exists():
                    tex = f'SKV2_{sex}_{key}_D'
                looks.setdefault(look, {}).setdefault(sex, {})[slot] = {'mesh': mesh, 'tex': 'T_' + tex, 'layer': LAYER[slot], 'covers': 0}
    # every style keeps its slot (an index is saved with the character); bald and not-yet-exported styles have no mesh
    hair = {sex: [{'mesh': f'SKV2_{sex}_{m}_hair', 'tex': f'T_SKV2_{sex}_{m}_D', 'name': n} if m and (EXPORT / f'SKV2_{sex}_{m}_hair.fbx').exists()
                  else {'mesh': '', 'tex': '', 'name': n} for m, n in lst] for sex, lst in HAIR.items()}
    hats = {}
    for look, per in HATS.items():
        for sex, hat in zip(('Male', 'Female'), per):
            if hat and (EXPORT / f'SKV2_{sex}_{hat}_hat.fbx').exists():
                hats.setdefault(look, {})[sex] = {'mesh': f'SKV2_{sex}_{hat}_hat', 'tex': f'T_SKV2_{sex}_{hat}_D', 'hides': hat not in OPEN_HATS, 'all_hair': hat in CLOSED_HATS}
    data = {'version': 2, 'zones': ZONES, 'looks': looks, 'hair': hair, 'skin': [list(c) for c in SKIN], 'hats': hats}
    OUT.write_text(json.dumps(data, indent=1), encoding='utf-8')
    print(f'wrote {OUT.name}: {len(looks)} looks, hair {[len(v) for v in hair.values()]}, missing meshes {sorted(set(missing))}')


if __name__ == '__main__':
    main()
