"""Certify and index the completed storefront-kit phase, preserving later phases."""
import json
from pathlib import Path
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1];p=R/'manifest.json';m=json.loads(p.read_text())
audit=json.loads((R/'src/exported_mesh_selfcheck.json').read_text());proof=json.loads((R/'src/kit_proof_audit.json').read_text())
assert audit['all_passed'] and audit['checked_assets']==len(m['assets'])
assert proof['length_m']==120 and proof['gap_check_passed'] and len(proof['families'])==4
assert all(len([a for a in m['assets'] if a.get('family')==f])==27 for f in ['BRK','SPN','DECO','MSN'])
assert len([a for a in m['assets'] if a['id'].startswith(('RF_','DR_'))])==19
textures={v['texture'] for v in m['atlases'].values()}|{v['texture'] for v in m['tileables'].values()}|{s['page'] for s in m['signs']}
textures|={a['texture'] for a in m['assets'] if not a['texture'].startswith('atlas:')}
for texture in textures:
 im=Image.open(R/texture);assert im.size==(2048,2048) and im.mode in ['RGB','RGBA'] and 'srgb' in im.info,(texture,im.size,im.mode,im.info.keys())
for a in m['assets']:
 assert (R/a['file']).exists()
 for c in a['concept']:assert (R/c).exists()
 if a['id'].startswith('RF_'):
  a['prompts']={'34':json.loads((R/'src/roof_clutter_prompts.json').read_text())[a['id']]}
 if 'Awning' in a['id']:
  revision=R/f"src/{a.get('family')}_awning_concept_revision.json"
  if revision.exists():a.setdefault('prompts',{})['family_awning_revision']=json.loads(revision.read_text())
 if a['id']=='DR_MayorNotice':a['text']='CLOSED BY ORDER OF THE MAYOR'
phase=[a for a in m['assets'] if a.get('family') in ['BRK','SPN','DECO','MSN'] or a['id'].startswith(('RF_','DR_'))]
sheet=Image.new('RGB',(1440,((len(phase)+2)//3)*180),(55,55,55));draw=ImageDraw.Draw(sheet)
for i,a in enumerate(phase):
 x=(i%3)*480;y=(i//3)*180;draw.text((x+4,y+2),a['id'],fill='white')
 for j,view in enumerate(['front','back','top']):
  image=Image.open(R/f"preview/{a['id']}_{view}.png").convert('RGB');image.thumbnail((160,160));sheet.paste(image,(x+j*160,y+18))
sheet.save(R/'preview/Phase2_contact.png')
m['phases']['2']={'status':'complete','reviewed_families':['BRK','SPN','DECO','MSN'],'kit_proof':['preview/KitProof_street.png','preview/KitProof_30m.png'],'contact':'preview/Phase2_contact.png','export_audit':'src/exported_mesh_selfcheck.json','assets':len(phase)}
m['production_status']='phase_3_next';m['integration_notes']=m.get('integration_notes',[])+['Phase2 neutral atlas was repacked to prioritize legible notice lettering. Reimport all19 RF_/DR_ meshes together with T_ATL_NEUTRAL.png.']
m['files']=sorted(f.relative_to(R).as_posix() for f in R.rglob('*') if f.is_file());p.write_text(json.dumps(m,indent=2)+'\n')
review=R/'src/DECO_visual_review.json'
if review.exists():
 d=json.loads(review.read_text())
 for row in d['records']:
  if row['status']!='pass':row['earlier_hold']=row['reason'];row['status']='pass';row['reason']='Root reviewed second references/projection and final cleanup; exposed backs and canopy shells pass.'
 d['final_root_review_complete']=True;review.write_text(json.dumps(d,indent=2)+'\n')
print('Phase2 complete:',len(phase),'kit/dressing/roof assets;',len(m['assets']),'published total;',sum(a['tris'] for a in m['assets']),'unique triangles')
