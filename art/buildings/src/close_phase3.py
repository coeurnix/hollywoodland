"""Close the mandatory hub checkpoint only after all exported deliverables pass."""
import json
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1];m=json.loads((R/'manifest.json').read_text());idx={a['id']:a for a in m['assets']}
required=['LM_Graumans','LM_Graumans_ForecourtWall','LM_Egyptian','LM_Egyptian_Wing','LM_Egyptian_Gateway','LM_Roosevelt','LM_Pantages','LM_Taft','LM_HollywoodHotel','LM_HollywoodHotel_Wing','LM_HollywoodHotel_CourtWall','LM_FirstNational','LM_PoliceStation','LM_BrownDerby','LM_RedCarStop','LM_Kiosk_Auction']
checks=json.loads((R/'src/exported_mesh_selfcheck.json').read_text());assert checks['all_passed'] and checks['checked_assets']==len(idx)
for aid in required:
    a=idx[aid];assert a.get('prompts') and all((R/p).exists() for p in a['concept'])
    for p in [a['file'],a['texture'],a['source'],a.get('export_audit',f'src/{aid}_export_audit.json')]:assert (R/p).exists(),p
    for view in ['front','back','top','box']:assert (R/f'preview/{aid}_{view}.png').exists()
    for t in a.get('attachments',[]):assert idx[t['asset']]['kind']=='sign_card'
    image=Image.open(R/a['texture']);assert image.mode=='RGB' and image.size==(2048,2048) and 'srgb' in image.info
assert (R/'preview/Phase3_contact.png').exists()
assert len([s for s in m['signs'] if s['page'] in ['signs/T_SIGN_02.png','signs/T_SIGN_03.png']])==15
m['phases']['3'].update(status='complete',assets=31,landmark_meshes=16,sign_cards=15,contact='preview/Phase3_contact.png',export_audit='src/exported_mesh_selfcheck.json')
m['production_status']='phase_4_in_progress';m['phases']['4']={'status':'in_progress','groups':['street_furniture','vehicles','backlot','ground_tileables']}
m['files']=sorted(p.relative_to(R).as_posix() for p in R.rglob('*') if p.is_file())
(R/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('PHASE3 COMPLETE:16 landmarks,15 sign cards; all',len(idx),'published FBXs pass independent checks')
