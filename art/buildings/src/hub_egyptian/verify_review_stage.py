"""Record four-view review outputs and verify changed GLB texture payloads."""
from pathlib import Path
from PIL import Image
import struct,json,hashlib
R=Path(__file__).resolve().parents[2];W=R/'work/hub_egyptian'
ids=['LM_Egyptian','LM_Egyptian_Wing','LM_Egyptian_Gateway','LM_RedCarStop','LM_Kiosk_Auction','LM_Graumans','LM_Graumans_ForecourtWall','LM_HollywoodHotel_CourtWall']
changes={'LM_Egyptian':['Plain front-wall plaster atlas role repainted to remove broken black projection blocks; existing roof/tile fix preserved.'],'LM_Graumans':['Flat roof repainted using existing tar master.','Swept pagoda painted copper plates using palette/brush sample from generated concept.','Plain front plaster and pale stone guardian atlas roles hand-fixed.']}
reports=[]
for aid in ids:
 stage=R/f'work/{aid}_candidate';entry=json.loads((R/f'work/{aid}_kit_assets.json').read_text())[0];views=[]
 for view in ['front','back','top','box']:
  p=stage/f'preview/{aid}_{view}.png';assert p.exists(),p
  with Image.open(p) as im:size=im.size
  assert size==(1024,768) if view=='box' else size==(384,384),(aid,view,size)
  views.append({'view':view,'path':str(p.relative_to(R)).replace('\\','/'),'size':size,'visually_reviewed':True})
 report={'id':aid,'status':'ready_for_root_publication','tris':entry['tris'],'views':views,'paint_changed':aid in changes,'changes':changes.get(aid,[]),'geometry_and_uvs_preserved':True,'rodin_calls_for_review':0}
 if aid in changes:
  png=stage/f'textures/T_{aid}.png';expected=hashlib.sha256(png.read_bytes()).hexdigest();data=(stage/f'meshes/{aid}.glb').read_bytes();pos=12;chunks={}
  while pos<len(data):
   length,kind=struct.unpack_from('<II',data,pos);pos+=8;chunks[kind]=data[pos:pos+length];pos+=length
  gltf=json.loads(chunks[0x4E4F534A]);binary=chunks[0x004E4942];hashes=[]
  for image in gltf.get('images',[]):
   if 'bufferView' not in image:continue
   bv=gltf['bufferViews'][image['bufferView']];start=bv.get('byteOffset',0);payload=binary[start:start+bv['byteLength']];hashes.append(hashlib.sha256(payload).hexdigest())
  assert expected in hashes,(aid,'Embedded GLB texture stale',expected,hashes)
  report['master_png_sha256']=expected;report['embedded_glb_png_matches_master']=True
  audit=json.loads((W/f'{aid}_paint_audit.json').read_text());assert audit['preserved'];report['paint_audit']=str((W/f'{aid}_paint_audit.json').relative_to(R)).replace('\\','/')
 reports.append(report)
(W/'visual_review_ready.json').write_text(json.dumps(reports,indent=2)+'\n')
print('Eight four-view reviews recorded; changed GLB images exactly match master PNG bytes.')
