"""Publish reviewed shared roof clutter and occupation dressing checkpoint."""
import json,shutil
from pathlib import Path
from PIL import Image,PngImagePlugin
R=Path(__file__).resolve().parents[1];stage=R/'work/NEUTRAL_candidate'
assets=json.loads((R/'work/NEUTRAL_kit_assets.json').read_text());audits=json.loads((R/'src/NEUTRAL_kit_export_audit.json').read_text())
plans=json.loads((R/'work/neutral_assets.json').read_text());budgets={a['id']:a['budget'] for a in plans}
for a in assets:
 c=audits[a['id']];assert c['tris']<=budgets[a['id']]
 assert c['material_slots']==1 and c['triangulated'] and c['zero_area_faces']==c['loose_vertices']==c['branch_edges']==0
 assert all((R/p).exists() for p in a['concept'])
 p=R/f"work/dressing/{a['id']}_prompt.txt"
 a['prompts']={'generated_sheet':p.read_text(encoding='utf-8')} if p.exists() else {'source':'src/roof_clutter_prompts.json'}
for folder in ['meshes','textures','preview']:
 (R/folder).mkdir(exist_ok=True)
 for p in (stage/folder).glob('*'):
  if p.is_file():shutil.copy2(p,R/folder/p.name)
meta=PngImagePlugin.PngInfo();meta.add(b'sRGB',b'\x00')
p=R/'textures/T_ATL_NEUTRAL.png';im=Image.open(p).convert('RGB');assert im.size==(2048,2048);im.save(p,pnginfo=meta)
m=json.loads((R/'manifest.json').read_text());ids={a['id'] for a in assets};m['assets']=[a for a in m['assets'] if a['id'] not in ids]+assets
m['atlases']['ATL_NEUTRAL']={'texture':'textures/T_ATL_NEUTRAL.png','emissive':None,'method':'Option B; independent sources baked to packed authored UVs'}
base={v['texture'] for v in m['atlases'].values()}|{v['texture'] for v in m['tileables'].values()}|{a['texture'] for a in m['assets'] if a['kind']!='sign_card' and not a['texture'].startswith('atlas:')};m['texture_sets']=len(base)
m['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file());(R/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
from rodin_client import key
needle=key()[:12].encode();assert not any(needle in p.read_bytes() for p in R.rglob('*') if p.is_file()),'Secret scan failed; do not expose credentials'
print('Published',len(assets),'neutral meshes;',len(m['assets']),'total;',m['texture_sets'],'texture sets; secret scan passed')
