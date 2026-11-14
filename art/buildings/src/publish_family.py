"""Publish a visually reviewed family checkpoint. Does not mark phase two complete."""
import json,shutil,sys
from pathlib import Path
from PIL import Image,PngImagePlugin
R=Path(__file__).resolve().parents[1];FAM=sys.argv[1];stage=R/f'work/{FAM}_candidate'
assets=json.loads((R/f'work/{FAM}_kit_assets.json').read_text());audit=json.loads((R/f'src/{FAM}_kit_export_audit.json').read_text())
for a in assets:
 c=audit[a['id']];assert c['tris']<= (60 if a['kind'] in ['wall','roof'] else 600)
 assert c['material_slots']==1 and c['triangulated'] and c['zero_area_faces']==c['loose_vertices']==c['branch_edges']==0
 assert all((R/p).exists() for p in a['concept'])
 prompts={}
 original=json.loads((R/'src/concept_prompts.json').read_text())
 prompts.update({k:v for k,v in original.items() if k.startswith(a['id']+'_')})
 batch=R/'src/kit_prompts_batch1.json'
 if batch.exists():
  for row in json.loads(batch.read_text()):
   if row['id']==a['id']:prompts['batch1_'+row['view']]=row['prompt']
 for p in (R/'src/concept_prompts').glob(a['id']+'_*.json'):
  row=json.loads(p.read_text());prompts[p.stem]=row['prompt']
 a['prompts']=prompts
for folder in ['meshes','textures','preview']:
 (R/folder).mkdir(exist_ok=True)
 for p in (stage/folder).glob('*'):
  if p.is_file():shutil.copy2(p,R/folder/p.name)
meta=PngImagePlugin.PngInfo();meta.add(b'sRGB',b'\x00')
for p in (R/'textures').glob('*.png'):
 im=Image.open(p);im=im.convert('RGBA' if 'A' in im.getbands() and im.getextrema()[-1][0]<255 else 'RGB');im.save(p,pnginfo=meta)
manifest=json.loads((R/'manifest.json').read_text());ids={a['id'] for a in assets}
manifest['assets']=[a for a in manifest['assets'] if a['id'] not in ids]+assets
manifest['atlases']['ATL_'+FAM]={'texture':f'textures/T_ATL_{FAM}.png','emissive':None,'method':'Option B'}
manifest['tileables'].update(json.loads((R/'work/kit_tileables.json').read_text()))
manifest['recipes'][FAM]={'ground':[FAM+f'_G_Bay{w}{v}' for w in [8,6,4] for v in 'AB'],'upper':[FAM+f'_U_Bay{w}{v}' for w in [4,3,2] for v in 'AB'],'entry':FAM+'_G_Entry4','pilaster':FAM+'_Pilaster1_G','pilaster_upper':FAM+'_Pilaster1_U','corner':FAM+'_Corner_G','corner_upper':FAM+'_Corner_U','corner_front_m':.5,'corner_return_m':.5,'parapet':[FAM+'_Parapet2',FAM+'_Parapet4'],'crest':[FAM+'_CrestA',FAM+'_CrestB'],'awning':[FAM+'_Awning4',FAM+'_Awning6'],'wall':FAM+'_Wall','roof':FAM+'_Roof','damaged':[FAM+'_Damaged_G_Bay6',FAM+'_Damaged_U_Bay3'],'ground_h':4.5,'upper_h':3.5,'parapet_h':1.5,'height_fit_note':'12 m and 15 m recipe/parapet scale conflict is documented in README; exported modules retain exact nominal heights.'}
manifest['production_status']='phase_2_in_progress';manifest['phases']['2']={'status':'in_progress','reviewed_families':sorted(manifest['recipes']),'kit_proof':'pending'}
base={v['texture'] for v in manifest['atlases'].values()}|{v['texture'] for v in manifest['tileables'].values()}
base|={a['texture'] for a in manifest['assets'] if a['kind']!='sign_card' and not a['texture'].startswith('atlas:')}
manifest['texture_sets']=len(base)
manifest['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file())
(R/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for a in manifest['assets']:
 assert (R/a['file']).exists()
 if not a['texture'].startswith('atlas:'):assert (R/a['texture']).exists()
from rodin_client import key
needle=key()[:12].encode();leaks=[p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file() and needle in p.read_bytes()]
assert not leaks,'Secret scan failed; inspect local report paths without exposing credentials'
print('Published',FAM,len(assets),'family meshes;',len(manifest['assets']),'total assets;',manifest['texture_sets'],'texture sets; secret scan passed')
