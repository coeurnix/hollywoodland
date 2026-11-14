"""Foliage and rocks for the open ground (art/foliage): palms, trees, shrubs, agave, rocks and flower/grass sprites.

Two kinds of asset:
  model   FLUX reference image (fal.ai blackforestlabs/flux-3/klein/text-to-image, white background), then a Hyper3D
          Rodin Gen-2 model (PBR, GLB). Trees, shrubs, succulents and rocks.
  card    FLUX image only, keyed off its white background into an RGBA sprite; tools/foliage_cards_blender.py turns it
          into crossed alpha cards (grass tufts, wildflowers). Thin blades and petals come out of Rodin as blobs.

Writes art/foliage/src/<id>.png (references / sprites), art/foliage/raw/<id>.glb (Rodin output), state in
art/foliage/state.json (request ids, seeds, prompts; no secrets). Resumable: finished steps are skipped.
Keys come from the repo-root .env (FAL_AI_API_KEY, RODIN_API_KEY) and are never written anywhere.

Run: python -I tools/foliagegen.py [--only id,id] [--images-only]
"""
import argparse, hashlib, json, os, sys, threading, time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import requests

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'art' / 'foliage'
STATE = OUT / 'state.json'
FAL_MODEL = 'blackforestlabs/flux-3/klein/text-to-image'
RODIN = 'https://api.hyper3d.com/api/v2'

REF = ('Product reference photo of {subject}, a single isolated object, entire object in frame with margin, three-quarter '
       'view from slightly above, even soft studio lighting, no shadow, plain pure white background, realistic natural '
       'colours, no text, no people, no pot, no ground.')
SPRITE = ('{subject}, seen from the side at eye level, a single isolated clump filling the frame, crisp edges, flat even '
          'lighting, plain pure white background, no shadow, no ground, no pot, realistic natural colours, no text.')

# id: (kind, subject, height in metres the art should read at in the game)
ASSETS = {
    'palm_fan': ('model', 'a very tall slender Mexican fan palm tree (Washingtonia robusta) with a thin straight grey trunk and a small round crown of bright green fan fronds, a skirt of dry brown fronds under the crown', 15.0),
    'palm_canary': ('model', 'a Canary Island date palm tree with a thick textured pineapple-patterned trunk and a large full crown of long arching feathery green fronds', 9.0),
    'tree_eucalyptus': ('model', 'a tall eucalyptus gum tree with a pale smooth peeling white and tan trunk and a loose airy crown of grey-green leaves', 13.0),
    'tree_pepper': ('model', 'a California pepper tree with a gnarled brown trunk and soft weeping drooping branches of fine bright green leaves and small pink berries', 7.0),
    'shrub_chaparral': ('model', 'a rounded dense chaparral shrub with small dusty grey-green leaves, wild California hillside bush', 1.6),
    'shrub_oleander': ('model', 'a full oleander bush with narrow dark green leaves covered in clusters of pink flowers', 2.2),
    'agave': ('model', 'a large blue-grey agave century plant rosette with thick pointed spiky leaves', 1.3),
    'yucca': ('model', 'a chaparral yucca plant: a round rosette of stiff narrow blue-green spiky leaves with one tall stalk of creamy white flowers', 2.6),
    'cactus_pear': ('model', 'a prickly pear cactus clump with flat green oval pads and a few yellow flowers', 1.4),
    'rock_boulder': ('model', 'a weathered tan sandstone boulder with rounded edges and natural cracks', 1.4),
    'rock_flat': ('model', 'a low cluster of three flat grey-brown granite rocks of different sizes', 0.6),
    'grass_dry': ('card', 'a tall clump of dry golden wild oat grass with thin blades and seed heads', 0.8),
    'grass_green': ('card', 'a dense clump of green wild grass with thin pointed blades', 0.55),
    'flower_poppy': ('card', 'a clump of orange California poppies, green feathery leaves and several open bright orange cup flowers on thin stems', 0.45),
    'flower_lupine': ('card', 'a clump of purple wild lupine flowers, tall spikes of violet-blue blossoms over palmate green leaves', 0.7),
    'flower_daisy': ('card', 'a clump of small white wild daisies with yellow centres on thin green stems', 0.4),
    'flower_mustard': ('card', 'a clump of wild yellow mustard flowers, thin stems topped with small bright yellow blossoms', 0.9),
}


def load_env():
    env = ROOT / '.env'
    if env.exists():
        for line in env.read_text(encoding='utf-8').splitlines():
            if '=' in line and not line.lstrip().startswith('#'):
                k, v = line.split('=', 1)
                os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))


_lock = threading.Lock()
STATE_REF = [None]


def save(state):
    with _lock:
        tmp = STATE.with_suffix('.part')
        tmp.write_text(json.dumps(state, indent=1), encoding='utf-8')
        os.replace(tmp, STATE)


def log(*a):
    print(*a, flush=True)


def fal_image(key, prompt, w, h, st, http):
    fh = {'Authorization': f"Key {os.environ['FAL_AI_API_KEY']}", 'Content-Type': 'application/json'}
    out = OUT / 'src' / f'{key}.png'
    if out.exists() and st.get('img') == 'done' and st.get('prompt') == prompt:
        return out
    seed = int(hashlib.sha256(f'{key}:{st.get("attempt", 0)}'.encode()).hexdigest()[:8], 16) & 0x7FFFFFFF
    body = {'prompt': prompt, 'image_size': {'width': w, 'height': h}, 'num_images': 1, 'seed': seed,
            'enable_prompt_expansion': False, 'output_format': 'png', 'enable_safety_checker': True}
    r = http.post(f'https://queue.fal.run/{FAL_MODEL}', headers=fh, json=body, timeout=60)
    r.raise_for_status()
    j = r.json()
    t0 = time.time()
    while True:
        s = http.get(j['status_url'], headers=fh, timeout=60)
        s.raise_for_status()
        if s.json().get('status') == 'COMPLETED':
            break
        if time.time() - t0 > 600:
            raise TimeoutError(f'{key}: fal queue timeout')
        time.sleep(2)
    res = http.get(j['response_url'], headers=fh, timeout=60).json()
    img = http.get(res['images'][0]['url'], timeout=120)
    img.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(img.content)
    st.update(img='done', prompt=prompt, seed=seed)
    return out


def rodin_model(key, ref, st, http):
    rh = {'Authorization': f"Bearer {os.environ['RODIN_API_KEY']}"}
    out = OUT / 'raw' / f'{key}.glb'
    if out.exists() and st.get('model') == 'done':
        return out
    if not st.get('sub'):
        with open(ref, 'rb') as f:
            r = http.post(f'{RODIN}/rodin', headers=rh, files={'images': (ref.name, f, 'image/png')},
                          data={'tier': 'Gen-2', 'geometry_file_format': 'glb', 'material': 'PBR', 'quality': 'medium',
                                'mesh_mode': 'Raw', 'texture_delight': 'true'}, timeout=120)
        if r.status_code >= 400:
            raise RuntimeError(f'{key}: rodin submit {r.status_code} {r.text[:300]}')
        j = r.json()
        st.update(task=j['uuid'], sub=j['jobs']['subscription_key'])
        save(STATE_REF[0])
        log(f'  {key}: rodin task {j["uuid"]}')
    t0 = time.time()
    while True:
        r = http.post(f'{RODIN}/status', headers=rh, json={'subscription_key': st['sub']}, timeout=60)
        r.raise_for_status()
        jobs = r.json().get('jobs', [])
        stat = [x.get('status') for x in jobs]
        if stat and all(s == 'Done' for s in stat):
            break
        if any(s == 'Failed' for s in stat):
            st.pop('sub', None); st['attempt'] = st.get('attempt', 0) + 1
            raise RuntimeError(f'{key}: rodin failed')
        if time.time() - t0 > 1500:
            raise TimeoutError(f'{key}: rodin still running; rerun to keep waiting')
        time.sleep(10)
    r = http.post(f'{RODIN}/download', headers=rh, json={'task_uuid': st['task']}, timeout=60)
    r.raise_for_status()
    items = r.json().get('list', [])
    glb = next((x for x in items if x['name'].lower().endswith('.glb')), None)
    if not glb:
        raise RuntimeError(f'{key}: no glb in {[x["name"] for x in items]}')
    d = http.get(glb['url'], timeout=600)
    d.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(d.content)
    st['model'] = 'done'
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--images-only', action='store_true')
    a = ap.parse_args()
    load_env()
    for k in ('FAL_AI_API_KEY', 'RODIN_API_KEY'):
        if not os.environ.get(k):
            sys.exit(f'missing {k} in .env')
    OUT.mkdir(parents=True, exist_ok=True)
    state = json.loads(STATE.read_text(encoding='utf-8')) if STATE.exists() else {}
    STATE_REF[0] = state
    only = set(x for x in a.only.split(',') if x)
    http = requests.Session()

    def one(key):
        kind, subj, _h = ASSETS[key]
        st = state.setdefault(key, {})
        if kind == 'model':
            ref = fal_image(key, REF.format(subject=subj), 1024, 1024, st, http)
            save(state)
            if not a.images_only:
                rodin_model(key, ref, st, http)
        else:
            fal_image(key, SPRITE.format(subject=subj), 1024, 1024, st, http)
        save(state)
        return key

    keys = [k for k in ASSETS if not only or k in only]
    with ThreadPoolExecutor(max_workers=6) as pool:
        futs = {pool.submit(one, k): k for k in keys}
        for f in as_completed(futs):
            try:
                log(f'done {f.result()}')
            except Exception as e:
                log(f'FAIL {futs[f]}: {e}')
    save(state)
    json.dump({k: {'kind': v[0], 'height': v[2]} for k, v in ASSETS.items()}, open(OUT / 'assets.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
