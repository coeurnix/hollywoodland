"""Texture-only client. Official Hyper3D generate-texture/status/download docs checked 2026-11-13.
Secrets are read from repository .env at runtime, never persisted or printed.
"""
import json, time, sys
from pathlib import Path
import requests
ROOT = Path(__file__).resolve().parents[1]
LEDGER = ROOT / 'rodin_ledger.json'
def key():
    for line in (ROOT.parents[1] / '.env').read_text(encoding='utf-8-sig').splitlines():
        if line.strip().startswith('RODIN_API_KEY='):
            return line.split('=',1)[1].strip().strip('\"').strip("'")
    raise RuntimeError('RODIN_API_KEY missing')
def run(asset, reference_path=None, model_path=None, prompt=None):
    ledger=json.loads(LEDGER.read_text()) if LEDGER.exists() else {'credits_used':0,'calls':[]}
    if ledger['credits_used']+0.5>200: raise RuntimeError('Credit ceiling reached')
    if sum(c['asset']==asset for c in ledger['calls'])>=2:
        print(asset,'two attempts already recorded; use SPEC 15 fallback');return False
    s=requests.Session(); s.headers['Authorization']='Bearer '+key()
    def api(endpoint, **kwargs):
        r=s.post('https://api.hyper3d.com/api/v2/'+endpoint,timeout=120,**kwargs)
        if r.status_code==429: raise RuntimeError('HTTP 429; retry later, no automatic submission retry')
        if not r.ok: raise RuntimeError('HTTP '+str(r.status_code))
        d=r.json()
        if d.get('error'): raise RuntimeError('API '+str(d['error']))
        return d
    opts={'material':'Shaded','resolution':'Basic','geometry_file_format':'glb','texture_mode':'high'}
    palette={'BRK':'sepia brick, cream stone, bottle-green wood and oxblood doors','SPN':'ivory stucco, terracotta tile and bulkheads, dark green ironwork and warm wood','DECO':'cream and buff stone, black marble, restrained chrome and oxblood accents','MSN':'sand stucco, dark timber, small green windows and terracotta trim'}.get(asset.split('_')[0],'the reference colors and materials')
    opts['prompt']='Hand-painted early classic MMO asset. Use '+palette+'. Painted top-light and ambient occlusion, restrained brush texture. Paint every model surface as its actual material. No background, sky, ground, lettering or people. No unrelated added objects or photographic shading.'
    if asset.startswith('ATL_'):
        opts['material']='Shaded'
        opts['prompt']='Hand-painted early classic MMO architecture, warm cream stone and sepia brick, painted ambient occlusion, dark slate glass, bottle green wood and oxblood doors. Preserve shared UV layout. No lettering, no people, no sky or ground. Restrained brush texture, simple readable shapes, top lighting.'
    if prompt: opts['prompt']=prompt
    entry={'asset':asset,'attempt':1+sum(c['asset']==asset for c in ledger['calls']),'task_uuid':None,'options':opts,'credits_consumed':0,'status':'submitting'}
    ledger['calls'].append(entry)
    def save():
        temporary=LEDGER.with_suffix('.json.tmp');temporary.write_text(json.dumps(ledger,indent=2)+'\n');temporary.replace(LEDGER)
    save()
    try:
        reference='front' if entry['attempt']==1 else '34'
        if 'Awning' in asset:reference='34'
        entry['reference']=reference_path or f'concept/{asset}_{reference}.png'
        entry['input_model']=model_path or f'work/{asset}_input.glb'
        with (ROOT/entry['reference']).open('rb') as im, (ROOT/entry['input_model']).open('rb') as model:
            d=api('rodin_texture_only',files={'image':('reference.png',im,'image/png'),'model':('model.glb',model,'model/gltf-binary')},data=opts)
        if not d.get('uuid'): raise RuntimeError('Missing task uuid')
        entry.update(task_uuid=d['uuid'],credits_consumed=d.get('consumed',0.5),status='accepted')
        ledger['credits_used']+=entry['credits_consumed']; save()
        if ledger['credits_used']>200: raise RuntimeError('STOP: credits exceed 200')
        start=time.monotonic(); delay=5
        while time.monotonic()-start<1200:
            time.sleep(delay)
            states=[j['status'] for j in api('status',json={'subscription_key':d['jobs']['subscription_key']})['jobs']]
            print(asset,states,flush=True)
            if 'Failed' in states: raise RuntimeError('Generation failed')
            if states and all(x=='Done' for x in states): break
            delay=min(delay+5,30)
        else: raise RuntimeError('Polling deadline exceeded')
        files=api('download',json={'task_uuid':d['uuid']})['list']
        target=ROOT/'work'/asset; target.mkdir(exist_ok=True)
        entry['downloads']=[]
        for f in files:
            name=Path(f['name']).name
            r=requests.get(f['url'],timeout=120); r.raise_for_status()
            (target/name).write_bytes(r.content); entry['downloads'].append(str((target/name).relative_to(ROOT)))
        entry['status']='downloaded'; save(); print(asset,'downloaded',entry['downloads'])
    except Exception as e:
        detail=('HTTP '+str(e.response.status_code)) if isinstance(e,requests.HTTPError) and e.response is not None else str(e)
        if 'https://' in detail or 'http://' in detail:detail=type(e).__name__+' (URL omitted)'
        entry['status']='failed'; entry['error']=detail.replace(key(),'[REDACTED]'); save()
        print(asset,entry['error']); return False
    return True
if __name__=='__main__':
    sys.exit(0 if run(sys.argv[1]) else 1)
