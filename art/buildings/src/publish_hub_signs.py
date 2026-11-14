"""Publish reviewed card pages and mechanically resolve landmark attachments."""
from pathlib import Path
import json,shutil
R=Path(__file__).resolve().parents[1]
m=json.loads((R/'manifest.json').read_text());records=json.loads((R/'work/hub_sign_records.json').read_text())
entries=json.loads((R/'work/HUBSIGNS_kit_assets.json').read_text());audits=json.loads((R/'src/HUBSIGNS_export_audit.json').read_text())
for a in entries:
    check=audits[a['id']];assert a['tris']==check['tris']<=50 and a['bounds']==check['bounds']
    assert check['material_slots']==1 and check['triangulated']
    assert check['zero_area_faces']==check['branch_edges']==check['loose_vertices']==0
    for ext in ['fbx','glb']:shutil.copy2(R/f"work/HUBSIGNS_candidate/meshes/{a['id']}.{ext}",R/f"meshes/{a['id']}.{ext}")
for p in (R/'work/HUBSIGNS_candidate/preview').glob('*.png'):shutil.copy2(p,R/'preview'/p.name)
ids={a['id'] for a in entries};m['assets']=[a for a in m['assets'] if a['id'] not in ids]+entries
m['signs']=[s for s in m['signs'] if s['id'] not in ids]+records
cards={a['id']:a for a in m['assets'] if a['kind']=='sign_card'}
for a in m['assets']:
    if a['kind']!='landmark':continue
    placements=a.get('sign_attachments',[])+a.get('sign_attachment_rectangles',[])
    resolved=[t for t in a.get('attachments',[]) if t['asset'] not in {p['id'] for p in placements}]
    for p in placements:
        if p['id'] not in cards:continue # optional poster supplied in phase8
        card=cards[p['id']];cw,ch=card['nominal']
        if 'rect' in p:
            lo,hi=p['rect'];target=[hi[0]-lo[0],hi[2]-lo[2]];centre=[(lo[i]+hi[i])/2 for i in range(3)]
        else:
            centre=p.get('centre',p.get('position'));target=p.get('size_m',p.get('size'))
        if p['id']=='SIGN_BrownDerby':target=[7,2.5] # silhouette deliberately follows the complete hat backing
        loc=[centre[0]-target[0]/2,centre[1],centre[2]-target[1]/2]
        resolved.append({'asset':p['id'],'location':[round(v,6) for v in loc],
                         'rotation':[0,0,0],'scale':[round(target[0]/cw,8),1,round(target[1]/ch,8)]})
    if resolved:a['attachments']=resolved
m['sign_pages']={'SIGN_01':{'texture':'signs/T_SIGN_01.png'},'SIGN_02':{'texture':'signs/T_SIGN_02.png'},'SIGN_03':{'texture':'signs/T_SIGN_03.png'}}
m['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file())
(R/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('Published',len(entries),'sign cards; landmark attachments resolve bottom-left origins')
