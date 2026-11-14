"""Merge visually reviewed landmark checkpoints without changing existing phases."""
import json,shutil,sys
from pathlib import Path
from PIL import Image,PngImagePlugin
R=Path(__file__).resolve().parents[1];m=json.loads((R/'manifest.json').read_text());entries=[]
for aid in sys.argv[1:]:
 stage=R/f'work/{aid}_candidate';a=json.loads((R/f'work/{aid}_kit_assets.json').read_text())[0];check=json.loads((R/f'src/{aid}_export_audit.json').read_text())
 assert a['tris']==check['tris'] and a['bounds']==check['bounds'];assert check['material_slots']==1 and check['triangulated']
 assert check['zero_area_faces']==check['branch_edges']==check['loose_vertices']==0
 assert a['material']=='M_'+aid,(aid,a['material'])
 assert all((stage/'preview'/f'{aid}_{view}.png').exists() for view in ['front','back','top','box'])
 if isinstance(a.get('notes'),list):a['notes']=' '.join(a['notes'])
 if not a.get('prompts') and a.get('prompt'):
  prompt=R/a['prompt'];assert prompt.exists()
  a['prompts']=json.loads(prompt.read_text()) if prompt.suffix=='.json' else {'concept':prompt.read_text()}
 a['authored_source']=a.get('source');a['source']=f'src/{aid}_kit_textured.blend'
 a['export_audit']=f'src/{aid}_export_audit.json';a['source_checks']=f'src/{aid}_source_checks.json'
 for folder in ['meshes','textures','preview']:
  for path in (stage/folder).glob('*'):
   if path.is_file():shutil.copy2(path,R/folder/path.name)
 png=R/a['texture'];im=Image.open(png).convert('RGB');assert im.size==(2048,2048)
 info=PngImagePlugin.PngInfo();info.add(b'sRGB',b'\x00');im.save(png,pnginfo=info)
 for concept in a['concept']:assert (R/concept).exists(),concept
 entries.append(a)
ids={a['id'] for a in entries};m['assets']=[a for a in m['assets'] if a['id'] not in ids]+entries
base={v['texture'] for v in m['atlases'].values()}|{v['texture'] for v in m['tileables'].values()}|{a['texture'] for a in m['assets'] if a['kind']!='sign_card' and not a['texture'].startswith('atlas:')};m['texture_sets']=len(base)
m['phases']['3'].update(status='in_progress',reviewed_landmarks=sorted(a['id'] for a in m['assets'] if a['kind']=='landmark' and a['id']!='LM_Drugstore'))
m['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file());(R/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
# Read credentials only at runtime; scan exact prefixes without printing values.
needles=[]
for line in (R.parents[1]/'.env').read_text(encoding='utf-8-sig').splitlines():
 if line.startswith(('RODIN_API_KEY=','FAL_AI_API_KEY=','FAL_KEY=')):needles.append(line.split('=',1)[1].strip().strip('"').strip("'")[:12].encode())
assert not any(any(n and n in p.read_bytes() for n in needles) for p in R.rglob('*') if p.is_file()),'Secret scan failed; inspect locally without printing credentials'
print('Published',len(entries),'reviewed landmarks;',len(m['assets']),'total meshes;',m['texture_sets'],'texture sets; secret scan passed')
