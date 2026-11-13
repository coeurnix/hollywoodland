"""Prepares the character art package (art/characters, see its INTEGRATION.md) for the Unreal import.

Runtime textures are cut down for the web download: every overlay layer (underwear, face, scalp, clothing slot) is cropped
to the rectangle it actually paints, and the character's atlas is composed at run time on the GPU (HLCharacter.cpp), so
only those rectangles ship. Sizes (atlas-relative): skins 1K, faces and scalps 1K, clothing layers 512, hair atlas 1K,
hats and held items 256. Identical outputs (several scalps are the same paint) are written once.

Writes:
  hollywoodland-ue/Saved/CharStage/<Kind>/<Name>.png   staged textures for Scripts/import_characters.py
  hollywoodland-ue/Saved/CharStage/stage.json          what to import (textures, meshes, animations)
  hollywoodland-ue/Content/Data/characters.json         the runtime table (looks, slots, rects, geosets, hats, hair, held items)

Run: python -I tools/charprep.py
"""
import hashlib, json, os, sys
from PIL import Image
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'art', 'characters')
STAGE = os.path.join(ROOT, 'hollywoodland-ue', 'Saved', 'CharStage')
DATA = os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'characters.json')
SEXES = ['Male', 'Female']
SLOTS = ['shirt', 'trousers', 'shoes', 'coat', 'wrist', 'gloves', 'belt']  # manifest composition order after the scalp

man = json.load(open(os.path.join(ART, 'manifest.json'), encoding='utf-8'))
assets = man['assets']
stage = {'textures': [], 'meshes': [], 'anims': []}
written = {}  # content hash -> staged asset name


def src(name):
    return os.path.join(ART, assets[name]['file'].replace('/', os.sep))


def save(img, kind, name, group):
    """Writes img once per distinct content; returns the asset name to reference."""
    h = hashlib.sha1(img.tobytes() + str(img.size).encode() + img.mode.encode()).hexdigest()
    if h in written:
        return written[h]
    d = os.path.join(STAGE, kind)
    os.makedirs(d, exist_ok=True)
    img.save(os.path.join(d, name + '.png'), optimize=True)
    written[h] = f'{kind}/{name}'
    stage['textures'].append({'name': name, 'file': os.path.join(d, name + '.png'), 'folder': kind, 'group': group})
    return written[h]


def layer(name, kind, scale_to, group='layer'):
    """Crops an RGBA overlay to its painted rectangle; scale_to = atlas size the crop is resampled for."""
    im = Image.open(src(name)).convert('RGBA')
    W, H = im.size
    a = np.array(im)[:, :, 3]
    ys, xs = np.nonzero(a > 0)
    if not len(xs):
        return None
    pad = max(2, W // 256)
    x0, y0 = max(0, xs.min() - pad), max(0, ys.min() - pad)
    x1, y1 = min(W, xs.max() + 1 + pad), min(H, ys.max() + 1 + pad)
    f = scale_to / W
    cw, ch = max(4, round((x1 - x0) * f / 4) * 4), max(4, round((y1 - y0) * f / 4) * 4)  # BC blocks: multiples of 4
    crop = im.crop((x0, y0, x1, y1)).resize((cw, ch), Image.LANCZOS)
    # straight alpha: bleed colour into transparent texels so bilinear filtering at the edges does not darken
    arr = np.array(crop).astype(np.float32)
    al = arr[:, :, 3:4] / 255.0
    if (al < 1).any():
        blur = np.array(crop.convert('RGBa').resize((max(1, cw // 8), max(1, ch // 8)), Image.BILINEAR).resize((cw, ch), Image.BILINEAR).convert('RGBA')).astype(np.float32)
        arr[:, :, :3] = arr[:, :, :3] * al + blur[:, :, :3] * (1 - al)
        crop = Image.fromarray(arr.clip(0, 255).astype(np.uint8), 'RGBA')
    asset = save(crop, kind, name, group)
    return {'tex': asset, 'rect': [round(x0 / W, 6), round(y0 / H, 6), round(x1 / W, 6), round(y1 / H, 6)]}


def mask(name, rect, scale_to):
    im = Image.open(src(name)).convert('RGB')
    W, H = im.size
    box = (round(rect[0] * W), round(rect[1] * H), round(rect[2] * W), round(rect[3] * H))
    f = scale_to / W
    crop = im.crop(box).resize((max(4, round((box[2] - box[0]) * f / 4) * 4), max(4, round((box[3] - box[1]) * f / 4) * 4)), Image.BILINEAR)
    return save(crop, 'Looks', name, 'mask')


def full(name, kind, size, group, mode='RGB'):
    im = Image.open(src(name)).convert(mode)
    if im.size[0] != size:
        im = im.resize((size, size), Image.LANCZOS)
    return save(im, kind, name, group)


out = {'version': 1, 'sexes': {}, 'looks': {}, 'hats': {}, 'held': {}, 'slots': SLOTS,
       'starter_colours': man['starter_colours'], 'hat_anchor': {}, 'height': {}}
for sex in SEXES:
    b = man['bodies'][sex]
    out['height'][sex] = b['height_cm']
    a = man['hat_anchors'][sex]['crown_head_local_cm']
    out['hat_anchor'][sex] = a
    s = {'skins': [full(t, 'Skin', 1024, 'skin') for t in b['skin']],
         'underwear': layer(b['underwear'], 'Skin', 1024),
         'faces': [layer(t, 'Skin', 1024) for t in b['faces']],
         'defaults': b['default_geosets'],
         'hair_tex': full(f'T_{sex}_Hair', 'Hair', 1024, 'hair'),
         'hair': []}
    for h in man['hair'][sex]:
        s['hair'].append({'name': h['name'], 'mesh': h['mesh'], 'top': h['top'], 'scalp': layer(h['scalp'], 'Hair', 1024)})
        for m in (h['mesh'], h['top']):
            if m:
                stage['meshes'].append({'name': m, 'file': src(m), 'kind': 'skeletal', 'sex': sex, 'folder': 'Hair'})
    out['sexes'][sex] = s
    # body: default welded mesh creates the skeleton; parts import against it
    stage['meshes'].append({'name': b['default_merged'], 'file': src(b['default_merged']), 'kind': 'skeleton', 'sex': sex, 'folder': 'Body'})
    for f in sorted(os.listdir(os.path.join(ART, 'bodies', 'parts'))):
        if f.startswith(f'SK_{sex}_') and f.endswith('.fbx'):
            stage['meshes'].append({'name': f[:-4], 'file': os.path.join(ART, 'bodies', 'parts', f), 'kind': 'skeletal', 'sex': sex, 'folder': 'Body'})
    stage['anims'].append({'name': b['rom'], 'file': src(b['rom']), 'sex': sex})

for lk in man['looks']:
    L = {'name': lk['name'], 'type': lk['type'], 'tier': lk['tier'], 'slots': {}, 'hat': {}}
    for slot in SLOTS:
        per = lk['slots'].get(slot)
        if not per:
            continue
        for sex in SEXES:
            e = per.get(sex)
            if not e:
                continue
            ent = {'geosets': e.get('geosets', [])}
            if e.get('layer') and e['layer'] in assets:
                ly = layer(e['layer'], 'Looks', 512)
                if ly:
                    ent.update(ly)
                    # dye masks only for the starter outfit (the creator's colour choice)
                    if lk['id'] == 'Starter' and e.get('mask') in assets:
                        ent['mask'] = mask(e['mask'], ly['rect'], 512)
            L['slots'].setdefault(slot, {})[sex] = ent
    hat = lk.get('hat')
    if hat:
        for sex in SEXES:
            hs = hat.get(sex, hat) if sex == 'Female' else hat
            if hs.get('mesh') in assets:
                L['hat'][sex] = {'mesh': hs['mesh'], 'tex': full(hs['texture'], 'Hats', 256, 'prop', 'RGB'), 'hides_top': bool(hs.get('hides_hair_top'))}
                if not any(m['name'] == hs['mesh'] for m in stage['meshes']):
                    stage['meshes'].append({'name': hs['mesh'], 'file': src(hs['mesh']), 'kind': 'static', 'folder': 'Hats'})
    out['looks'][lk['id']] = L

# source assets that break the package convention (+X barrel/blade, +Z top): roll in degrees about +X, applied at attach time
HELD_ROLL = {'ThompsonM1928': 180}  # modelled upside down: grips and magazine on +Z
for w in man['weapons']:
    out['held'][w['id']] = {'name': w['name'], 'type': w['type'], 'mesh': w['mesh'], 'tex': full(w['texture'], 'Held', 256, 'prop', 'RGB'),
                            'hand': w['hand'], 'grip_l': w.get('grip_l_position')}
    if w['id'] in HELD_ROLL:
        out['held'][w['id']]['roll'] = HELD_ROLL[w['id']]
    stage['meshes'].append({'name': w['mesh'], 'file': src(w['mesh']), 'kind': 'static', 'folder': 'Held'})
    if f"{w['texture']}_Mask" in assets:
        out['held'][w['id']]['glow'] = True

anim_dir = os.path.join(ART, 'anim')
if os.path.isdir(anim_dir):
    for f in sorted(os.listdir(anim_dir)):
        if f.endswith('.fbx') and f.startswith('A_'):
            sex = f.split('_')[1]
            if sex in SEXES:
                # retargeted mocap (tools/danceprep_blender.py -> art/mocap) replaces the authored clip of the same name
                mocap = os.path.join(ROOT, 'art', 'mocap', f)
                stage['anims'].append({'name': f[:-4], 'file': mocap if os.path.exists(mocap) else os.path.join(anim_dir, f), 'sex': sex})

os.makedirs(STAGE, exist_ok=True)
json.dump(stage, open(os.path.join(STAGE, 'stage.json'), 'w'), indent=1)
json.dump(out, open(DATA, 'w'), indent=1)
tot = sum(os.path.getsize(t['file']) for t in stage['textures'])
px = 0
for t in stage['textures']:
    with Image.open(t['file']) as im:
        px += im.size[0] * im.size[1]
print(f"textures {len(stage['textures'])} ({tot / 1e6:.1f} MB png, {px / 1e6:.1f} Mpx), meshes {len(stage['meshes'])}, anims {len(stage['anims'])}")
