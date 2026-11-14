# Writes hollywoodland-ue/Content/Data/enemies.json (what the game reads) from art/enemies/enemies.json (the art package).
# Run after Scripts/ImportEnemies.ps1:  python -I tools/enemyprep.py
#   models.<Model>: mesh, kind, roles (idle/walk/run/attack/hit/death -> clip), clips.<Clip>: anim asset, len s, loop, ev s
#   mobs.<id>: model, mat (base-colour variant instance), specials (ability or script trigger -> clip)
#   structures.<Kind>: destroyed static mesh (+ live skeletal mesh for the egg sac)
import json, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'art', 'enemies')
OUT = os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'enemies.json')
man = json.load(open(os.path.join(SRC, 'enemies.json'), encoding='utf-8'))
fps = float(man.get('fps', 30))


def obj(path):
    name = path.rsplit('/', 1)[-1]
    return f'{path}.{name}'


models = {}
for k, v in man['models'].items():
    clips = {}
    for c, d in v['clips'].items():
        ev = [x for x in d.get('events', {}).values() if isinstance(x, (int, float))]
        clips[c] = {'anim': obj(f'/Game/Enemies/{k}/{d["anim"]}'), 'len': round(d['seconds'], 3), 'loop': bool(d['loop'])}
        if ev:
            clips[c]['ev'] = round(ev[0] / fps, 3)
    models[k] = {'mesh': obj(f'/Game/Enemies/{k}/SK_{k}'), 'kind': v['kind'], 'roles': v.get('roles', {}), 'clips': clips}

mobs = {}
for mid, m in man['mobs'].items():
    k = m.get('model')
    if k not in models:
        continue
    var = re.match(r'.*/T_' + re.escape(k) + r'_(\w+)_BaseColor\.png$', m.get('base_color', ''))
    mat = f'/Game/Enemies/{k}/MI_{k}' + (f'_{var.group(1)}' if var else '')
    mobs[mid] = {'model': k, 'mat': obj(mat), 'specials': m.get('specials', {})}

structures = {}
for k, s in man.get('structures', {}).items():
    d = s['destroyed'].rsplit('/', 1)[-1][:-4]
    e = {'destroyed': obj(f'/Game/Enemies/HiveStructures/{d}'),
         'mat': obj(f'/Game/Enemies/HiveStructures/MI_{d[3:]}')}
    if s['live'].endswith('.fbx') and k in models:
        e['live'] = k
    structures[k] = e

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    json.dump({'models': models, 'mobs': mobs, 'structures': structures}, f, indent=1)
print(f'enemies.json: {len(models)} models, {len(mobs)} mobs, {len(structures)} structures')
