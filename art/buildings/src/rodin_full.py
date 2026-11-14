"""Rodin Gen-2 full generation (geometry + texture) from one reference image, for the hive kit, backdrops and props.

    python -I art/buildings/src/rodin_full.py <asset id> <reference png, relative to art/buildings> [material]

The key is read from the repo-root .env at runtime and never written anywhere. Every job is recorded in
rodin_ledger.json under "session_2026_11_13" (asset, task uuid, credits); subscription keys and URLs are not stored.
Limits: 20 credits for the session, at most 2 attempts per asset (the second only after a failure), no automatic retry.
Output: work/<asset>/ (GLB + textures).
"""
import json, sys, time
from pathlib import Path
import requests

ROOT = Path(__file__).resolve().parents[1]
LEDGER = ROOT / 'rodin_ledger.json'
API = 'https://api.hyper3d.com/api/v2/'
BUDGET = 20.0


def key():
    for line in (ROOT.parents[1] / '.env').read_text(encoding='utf-8-sig').splitlines():
        if line.strip().startswith('RODIN_API_KEY='):
            return line.split('=', 1)[1].strip().strip('"').strip("'")
    raise RuntimeError('RODIN_API_KEY missing')


def run(asset, ref, material='Shaded'):
    ledger = json.loads(LEDGER.read_text())
    sess = ledger.setdefault('session_2026_11_13', {'budget': BUDGET, 'credits_used': 0, 'calls': []})
    prior = [c for c in sess['calls'] if c['asset'] == asset]
    if len(prior) >= 2 or any(c['status'] == 'downloaded' for c in prior):
        print(asset, 'already attempted twice or done; use the fallback'); return False
    if sess['credits_used'] + 0.5 > BUDGET:
        print('STOP: session credit budget reached'); return False
    s = requests.Session(); s.headers['Authorization'] = 'Bearer ' + key()

    def api(ep, **kw):
        r = s.post(API + ep, timeout=180, **kw)
        if not r.ok: raise RuntimeError(f'{ep} HTTP {r.status_code}')
        d = r.json()
        if d.get('error'): raise RuntimeError(f'{ep} API {d["error"]}')
        return d

    opts = {'tier': 'Gen-2', 'geometry_file_format': 'glb', 'material': material, 'quality': 'medium', 'mesh_mode': 'Raw', 'texture_delight': 'true'}
    entry = {'asset': asset, 'attempt': len(prior) + 1, 'task_uuid': None, 'options': opts, 'reference': ref, 'credits_consumed': 0, 'status': 'submitting',
             'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}
    sess['calls'].append(entry)

    def save():
        # merge with the file on disk, so parallel jobs never drop each other's entries
        disk = json.loads(LEDGER.read_text())
        ds = disk.setdefault('session_2026_11_13', {'budget': BUDGET, 'credits_used': 0, 'calls': []})
        ds['calls'] = [c for c in ds['calls'] if not (c['asset'] == asset and c['attempt'] == entry['attempt'])] + [entry]
        ds['credits_used'] = round(sum(c.get('credits_consumed', 0) for c in ds['calls']), 3)
        sess['credits_used'] = ds['credits_used']
        tmp = LEDGER.with_suffix(f'.{asset}.tmp'); tmp.write_text(json.dumps(disk, indent=2) + '\n'); tmp.replace(LEDGER)
    save()
    try:
        with (ROOT / ref).open('rb') as im:
            d = api('rodin', files={'images': (Path(ref).name, im, 'image/png')}, data=opts)
        entry.update(task_uuid=d['uuid'], credits_consumed=d.get('consumed', d.get('credits', 0.5)) or 0.5, status='accepted')
        save(); print(asset, 'task', d['uuid'], 'credits', entry['credits_consumed'], flush=True)
        sub = d['jobs']['subscription_key']
        t0 = time.monotonic()
        while time.monotonic() - t0 < 1800:
            time.sleep(10)
            st = [j['status'] for j in api('status', json={'subscription_key': sub})['jobs']]
            if 'Failed' in st: raise RuntimeError('Generation failed')
            if st and all(x == 'Done' for x in st): break
        else:
            raise RuntimeError('poll deadline')
        out = ROOT / 'work' / asset; out.mkdir(parents=True, exist_ok=True)
        names = []
        for f in api('download', json={'task_uuid': d['uuid']})['list']:
            name = Path(f['name']).name
            r = requests.get(f['url'], timeout=600); r.raise_for_status()
            (out / name).write_bytes(r.content); names.append(name)
        entry.update(status='downloaded', downloads=names); save()
        print(asset, 'downloaded', names, 'credits', entry['credits_consumed'], 'session total', sess['credits_used'])
        return True
    except Exception as e:
        msg = str(e).replace(key(), '[REDACTED]')
        if 'http' in msg: msg = type(e).__name__
        entry.update(status='failed', error=msg); save(); print(asset, 'FAILED', msg); return False


if __name__ == '__main__':
    sys.exit(0 if run(sys.argv[1], sys.argv[2], *(sys.argv[3:4])) else 1)
