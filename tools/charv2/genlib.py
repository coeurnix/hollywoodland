"""Shared FLUX 3 (fal.ai) and Hyper3D Rodin calls for the v2 characters.

Keys come from the repo-root .env (FAL_AI_API_KEY, RODIN_API_KEY) and are never printed or written. Every request is
recorded in a JSON state file (prompt, seed, task ids; no secrets) so a rerun never silently pays for a duplicate.
"""
import hashlib, json, os, threading, time
from pathlib import Path
import requests

ROOT = Path(__file__).resolve().parents[2]
FAL_T2I = 'blackforestlabs/flux-3/klein/text-to-image'
RODIN = 'https://api.hyper3d.com/api/v2'
_http = requests.Session()
_http.headers['User-Agent'] = 'asset-gen/1.0'


def load_env():
    p = ROOT / '.env'
    for line in p.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#') or '=' not in line:
            continue
        k, v = line.split('=', 1)
        os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))


class State:
    def __init__(self, path):
        self.path = Path(path)
        self.d = json.loads(self.path.read_text(encoding='utf-8')) if self.path.exists() else {}

    def get(self, key):
        return self.d.setdefault(key, {})

    _lock = threading.Lock()

    def save(self):
        with State._lock:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.path.with_suffix('.tmp')
            tmp.write_text(json.dumps(self.d, indent=1), encoding='utf-8')
            tmp.replace(self.path)


def flux(key, prompt, out, st, w=1024, h=1024, attempt=0):
    """Text-to-image; skips when out exists for the same prompt/attempt. Returns out."""
    out = Path(out)
    if out.exists() and st.get('prompt') == prompt and st.get('attempt', 0) == attempt:
        return out
    fh = {'Authorization': f"Key {os.environ['FAL_AI_API_KEY']}", 'Content-Type': 'application/json'}
    seed = int(hashlib.sha256(f'{key}:{attempt}'.encode()).hexdigest()[:8], 16) & 0x7FFFFFFF
    body = {'prompt': prompt, 'image_size': {'width': w, 'height': h}, 'num_images': 1, 'seed': seed,
            'enable_prompt_expansion': False, 'output_format': 'png', 'enable_safety_checker': True}
    r = _http.post(f'https://queue.fal.run/{FAL_T2I}', headers=fh, json=body, timeout=60)
    r.raise_for_status()
    j = r.json()
    t0 = time.time()
    while True:
        s = _http.get(j['status_url'], headers=fh, timeout=60)
        s.raise_for_status()
        if s.json().get('status') == 'COMPLETED':
            break
        if time.time() - t0 > 600:
            raise TimeoutError(f'{key}: fal timeout')
        time.sleep(2)
    res = _http.get(j['response_url'], headers=fh, timeout=60).json()
    img = _http.get(res['images'][0]['url'], timeout=120)
    img.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(img.content)
    st.update(prompt=prompt, seed=seed, attempt=attempt, w=w, h=h)
    return out


def _data_uri(p):
    import base64, mimetypes
    mt = mimetypes.guess_type(str(p))[0] or 'image/png'
    return f'data:{mt};base64,' + base64.b64encode(Path(p).read_bytes()).decode()


def flux_edit(key, prompt, images, out, st, aspect='auto', resolution='2k', attempt=0):
    """FLUX 3 edit-image: image 1 is the one being edited (keeps its layout), the rest are references. Returns out."""
    out = Path(out)
    sig = hashlib.sha256((prompt + '|' + '|'.join(hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in images)).encode()).hexdigest()
    if out.exists() and st.get('sig') == sig and st.get('attempt', 0) == attempt:
        return out
    fh = {'Authorization': f"Key {os.environ['FAL_AI_API_KEY']}", 'Content-Type': 'application/json'}
    body = {'prompt': prompt, 'image_urls': [_data_uri(p) for p in images], 'aspect_ratio': aspect, 'resolution': resolution,
            'output_format': 'png', 'enable_prompt_expansion': False, 'safety_tolerance': 2}
    r = _http.post('https://queue.fal.run/blackforestlabs/flux-3/edit-image', headers=fh, json=body, timeout=120)
    r.raise_for_status()
    j = r.json()
    t0 = time.time()
    while True:
        s = _http.get(j['status_url'], headers=fh, timeout=60)
        s.raise_for_status()
        if s.json().get('status') == 'COMPLETED':
            break
        if time.time() - t0 > 900:
            raise TimeoutError(f'{key}: fal edit timeout')
        time.sleep(3)
    res = _http.get(j['response_url'], headers=fh, timeout=60)
    if res.status_code >= 400:
        raise RuntimeError(f'{key}: edit {res.status_code} {res.text[:300]}')
    img = _http.get(res.json()['images'][0]['url'], timeout=180)
    img.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(img.content)
    st.update(sig=sig, prompt=prompt, attempt=attempt)
    return out


def rodin(key, images, out, st, save, tier='Gen-2', quality='high', material='PBR', mesh_mode='Quad', face_count=None,
          extra=None):
    """Image(s)-to-3D; resumes a submitted task instead of paying again. Returns the GLB path."""
    out = Path(out)
    if out.exists() and st.get('model') == 'done':
        return out
    rh = {'Authorization': f"Bearer {os.environ['RODIN_API_KEY']}"}
    if not st.get('sub'):
        files = [('images', (Path(p).name, open(p, 'rb'), 'image/png')) for p in images]
        data = {'tier': tier, 'geometry_file_format': 'glb', 'material': material, 'quality': quality, 'mesh_mode': mesh_mode,
                'texture_delight': 'true'}
        if face_count:
            data['quality_override'] = str(face_count)
        if extra:
            data.update(extra)
        try:
            r = _http.post(f'{RODIN}/rodin', headers=rh, files=files, data=data, timeout=180)
        finally:
            for _, f in files:
                f[1].close()
        if r.status_code >= 400:
            raise RuntimeError(f'{key}: rodin submit {r.status_code} {r.text[:300]}')
        j = r.json()
        st.update(task=j['uuid'], sub=j['jobs']['subscription_key'], params=data)
        save()
    t0 = time.time()
    while True:
        r = _http.post(f'{RODIN}/status', headers=rh, json={'subscription_key': st['sub']}, timeout=60)
        r.raise_for_status()
        stat = [x.get('status') for x in r.json().get('jobs', [])]
        if stat and all(s == 'Done' for s in stat):
            break
        if any(s == 'Failed' for s in stat):
            st.pop('sub', None)
            raise RuntimeError(f'{key}: rodin failed')
        if time.time() - t0 > 1800:
            raise TimeoutError(f'{key}: rodin still running; rerun to keep waiting')
        time.sleep(10)
    r = _http.post(f'{RODIN}/download', headers=rh, json={'task_uuid': st['task']}, timeout=60)
    r.raise_for_status()
    items = r.json().get('list', [])
    glb = next((x for x in items if x['name'].lower().endswith('.glb')), None)
    if not glb:
        raise RuntimeError(f'{key}: no glb in {[x["name"] for x in items]}')
    d = _http.get(glb['url'], timeout=600)
    d.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(d.content)
    st['model'] = 'done'
    save()
    return out
