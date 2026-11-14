import json,shutil,struct
from pathlib import Path
from PIL import Image,ImageDraw,PngImagePlugin
R=Path(__file__).resolve().parents[1]
for aid in ['BRK_G_Bay6A','LM_Drugstore']:
 dest=R/'src/rodin'/aid;dest.mkdir(parents=True,exist_ok=True)
 for f in (R/'work'/aid).glob('*.glb'):shutil.copy2(f,dest/f.name)
assets=json.loads((R/'work/phase1_assets.json').read_text())
assets+=json.loads((R/'work/sign_assets.json').read_text())
prompts=json.loads((R/'src/concept_prompts.json').read_text())
for a in assets:
 a['prompts']={k:v for k,v in prompts.items() if k.startswith(a['id']+'_')}
 if a['id']=='LM_Drugstore':
  a['attachments']=[{'asset':'SIGN_Drugstore','location':[-6,-18.025,4.3]},{'asset':'SIGN_SodaFountain','location':[-6,-18.025,3.6]}]
  a['notes']='Use attachments for correct lettering. Main mass 18 x 36 x 6; front awnings project 1.25 m with 2.7 m clearance. Game-data frontage discrepancy recorded in README.'
sheet=Image.new('RGB',(1800,1980),(48,49,48));d=ImageDraw.Draw(sheet)
for row,aid in enumerate(['BRK_G_Bay6A','BRK_U_Bay3A','LM_Drugstore']):
 for col,view in enumerate(['front','back','top']):
  im=Image.open(R/f'preview/{aid}_{view}.png').convert('RGB');sheet.paste(im,(col*600,row*660+40));d.text((col*600+15,row*660+15),aid+' / '+view,fill='white')
sheet.save(R/'preview/phase1_contact.png')
# Runtime master PNGs explicitly carry the sRGB rendering intent chunk.
for folder in ['textures','signs']:
 for f in (R/folder).glob('*.png'):
  im=Image.open(f).convert('RGB');meta=PngImagePlugin.PngInfo();meta.add(b'sRGB',b'\x00');im.save(f,pnginfo=meta)
checks=json.loads((R/'work/phase1_checks.json').read_text())
(R/'src/phase1_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
manifest={'version':1,'units':'meters','up':'+Z','front':'-Y','texture_sets':2,'atlases':{'ATL_BRK':{'texture':'textures/T_ATL_BRK.png','emissive':None}},'tileables':{},'assets':assets,'signs':json.loads((R/'work/sign_records.json').read_text()),'recipes':{},'production_status':'phase_1_complete_phase_2_pending','phases':{'1':{'status':'complete','preview':'preview/phase1_contact.png'},'2':{'status':'pending'}},'files':[]}
manifest['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file())
(R/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
# Read credentials solely in memory to audit for their accidental inclusion; report paths only.
from rodin_client import key
needle=key()[:12].encode();leaks=[]
for p in R.rglob('*'):
 if p.is_file() and needle in p.read_bytes():leaks.append(p.relative_to(R).as_posix())
audit={'secret_scan_pass':not leaks,'leak_paths':leaks,'manifest_inventory_exact':set(manifest['files'])=={p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file()},'runtime_textures':{},'mesh_checks':checks,'full_order_complete':False,'kit_proof_pending':True}
for folder in ['textures','signs']:
 for p in (R/folder).glob('*.png'):
  im=Image.open(p);audit['runtime_textures'][p.relative_to(R).as_posix()]={'size':list(im.size),'srgb':im.info.get('srgb')==0}
(R/'src/phase1_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
manifest['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file())
(R/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
assert not leaks
print('Published',len(assets),'assets; 2 texture sets; 2.5 Rodin credits; secret scan passed')
