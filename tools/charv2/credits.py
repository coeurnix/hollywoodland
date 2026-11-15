"""Writes art/characters_v2/LICENSES.md: author, licence and pack of every MakeHuman asset the v2 characters use
(garment bases, hair, MakeHuman hats, skins, eyes, brows, lashes), read from each asset's .mhclo / .mhmat header.

Run: python -I tools/charv2/credits.py
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import wardrobe

ROOT = Path(__file__).resolve().parents[2]
PACKS = ROOT / 'tmp' / 'character-art-source' / 'mh_packs'
SYS = ROOT / 'tmp' / 'character-art-source' / 'mh_system'
MH_HATS = ['jujube_newsboy_cap', 'aethelraed_unraed_cloche_hat', 'fedora01', 'fedora_cocked', 'mrgreaterthan_m1_helmet',
           'culturalibre_motorcycle_helmet', 'culturalibre_warrior_helmet', 'javherre_casco_caballero_templario_templar_knight_helmet']
SYSTEM = ['young_caucasian_male.mhmat', 'young_caucasian_female2.mhmat', 'high-poly.mhclo', 'eyebrow001.mhclo', 'eyebrow010.mhclo',
          'eyelashes01.mhclo', 'eyelashes02.mhclo']


def find(name):
    for root in (PACKS, SYS):
        hits = list(root.rglob(name))
        if hits:
            return hits[0]
    return None


def header(p):
    meta = {}
    for line in p.read_text(encoding='utf-8', errors='replace').splitlines()[:60]:
        t = line.strip().lstrip('#').strip()
        for k in ('author', 'license', 'name'):
            if t.lower().startswith(k + ' ') and k not in meta:
                meta[k] = t[len(k):].strip()
    return meta


def pack_of(p):
    try:
        rel = p.relative_to(PACKS)
        return rel.parts[0]
    except ValueError:
        return 'MakeHuman system assets'


def main():
    stems = sorted({b[1] for b in wardrobe.BASES.values()} | {s for lst in wardrobe.HAIR.values() for s, _ in lst if s} | set(MH_HATS))
    rows = []
    for s in stems:
        p = find(f'{s}.mhclo')
        if not p:
            rows.append((s, '?', '?', '?'))
            continue
        m = header(p)
        rows.append((s, m.get('author', 'MakeHuman community'), m.get('license', 'CC0'), pack_of(p)))
    for n in SYSTEM:
        p = find(n)
        m = header(p) if p else {}
        rows.append((n.rsplit('.', 1)[0], m.get('author', 'MakeHuman team (Data Collection AB)'), m.get('license', 'CC0'), 'MakeHuman system assets'))
    out = ['# Characters v2: third-party assets', '',
           'The v2 bodies, garments, hair and some hats are built from MakeHuman (makehumancommunity.org) assets. CC0 assets need',
           'no attribution and are listed for provenance; CC-BY assets are used under CC BY 4.0 (or the version their author',
           'chose) with the credit below. The female skin is MakeHuman young_caucasian_female2 with painted makeup; textures were',
           'repainted per look with FLUX 3, and belts, wrist pieces, cuirasses and the remaining hats are original Rodin models.',
           '', '| Asset | Author | Licence | Pack |', '|---|---|---|---|']
    out += [f'| {a} | {b} | {c} | {d} |' for a, b, c, d in rows]
    dst = ROOT / 'art' / 'characters_v2' / 'LICENSES.md'
    dst.write_text('\n'.join(out) + '\n', encoding='utf-8')
    by = [r for r in rows if 'by' in r[2].lower()]
    print(f'{len(rows)} assets, {len(by)} CC-BY, unknown {sum(1 for r in rows if r[1] == "?")}')


if __name__ == '__main__':
    main()
