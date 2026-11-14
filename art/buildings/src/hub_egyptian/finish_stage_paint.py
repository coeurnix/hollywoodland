"""Isolated Blender atlas paint cleanup for Egyptian/Grauman's main stages.
Does not rebake, remodel or change semantic UVs. Uses existing painted masters
and original concept colour/brush samples; no new model/image service calls.
Run Blender --background --python .../finish_stage_paint.py -- <assetID>
"""
import bpy,json,sys,hashlib,numpy as np
from pathlib import Path
R=Path(__file__).resolve().parents[2];S=R/'src/hub_egyptian';W=R/'work/hub_egyptian';sys.path.insert(0,str(R/'src'));from asset_io import export_asset
aid=sys.argv[sys.argv.index('--')+1]
assert aid in ['LM_Egyptian','LM_Graumans']
stage=R/f'work/{aid}_candidate';source=R/f'src/{aid}_kit_textured.blend'
bpy.ops.wm.open_mainfile(filepath=str(source));bpy.context.preferences.filepaths.save_version=0
obj=bpy.data.objects[aid]
def semantic_signature(ob):
 uv=ob.data.uv_layers.active
 faces=sorted(tuple(sorted((tuple(round(c,6) for c in ob.data.vertices[ob.data.loops[i].vertex_index].co),tuple(round(c,7) for c in uv.data[i].uv)) for i in p.loop_indices)) for p in ob.data.polygons)
 return hashlib.sha256(repr(faces).encode()).hexdigest()
before=semantic_signature(obj)
paint_version='phase3_semanticpaint_v2' if aid=='LM_Graumans' else 'phase3_semanticpaint_v1'
if not obj.get(paint_version):
 image=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE' and n.image)
 if image.packed_file:image.unpack(method='REMOVE')
 assert tuple(image.size)==(2048,2048)
 pixels=np.empty(2048*2048*4,dtype=np.float32);image.pixels.foreach_get(pixels);pixels=pixels.reshape(2048,2048,4)
 backup=W/f'{aid}_before_semantic_paint.png'
 if not backup.exists():image.filepath_raw=str(backup);image.file_format='PNG';image.save()
 slots=json.loads((W/f'{aid}_metadata.json').read_text())['uv']['slots']
 def load_pixels(path):
  im=bpy.data.images.load(str(path),check_existing=True);p=np.empty(im.size[0]*im.size[1]*4,dtype=np.float32);im.pixels.foreach_get(p);return p.reshape(im.size[1],im.size[0],4)
 def stamp(role,field):
  x,y,w,h,_=slots[role];assert field.shape==(h,w,4)
  pixels[y:y+h,x:x+w]=field
  # Paint an8px gutter on each side to prevent distant filtering fringes.
  pixels[y-8:y,x:x+w]=field[0:1];pixels[y+h:y+h+8,x:x+w]=field[-1:]
  pixels[y-8:y+h+8,x-8:x]=pixels[y-8:y+h+8,x:x+1];pixels[y-8:y+h+8,x+w:x+w+8]=pixels[y-8:y+h+8,x+w-1:x+w]
 def painted_master(role,tile,tint=(1,1,1)):
  x,y,w,h,density=slots[role];src=load_pixels(R/f'textures/T_TILE_{tile}.png');yy,xx=np.mgrid[0:h,0:w];period=max(1,int(4*density));field=src[(yy%period)*src.shape[0]//period,(xx%period)*src.shape[1]//period].copy();field[:,:,:3]*=np.array(tint)[None,None,:];field[:,:,3]=1;return field
 changed=[]
 if aid=='LM_Egyptian':
  # Plain stucco areas should remain intact architecture, not broken black blocks.
  field=painted_master('wall','Stucco_Cream',(1,.93,.79));stamp('wall',field);changed.append('plain front-wall plaster role')
 else:
  # The raw bake put cornices/windows across every roof cell. Restore an opaque
  # painted tar master on the flat mass and copper plates on the swept pagoda.
  stamp('roof',painted_master('roof','Roof_TarGravel',(1.08,1.03,.97)));changed.append('flat tar roof role')
  x,y,w,h,density=slots['tile'];yy,xx=np.mgrid[0:h,0:w]
  concept=load_pixels(R/'concept/LM_Graumans_front.png');ch,cw=concept.shape[:2]
  # Sample only generated copper roof artwork, never the historic photograph.
  swatch=concept[int(ch*.61):int(ch*.65),int(cw*.44):int(cw*.56),:3];palette=np.median(swatch.reshape(-1,3),axis=0)
  brush=swatch[(yy*3)%swatch.shape[0],(xx*2)%swatch.shape[1]].mean(axis=2);brush=(brush-brush.mean())*.15
  px,py=max(12,int(2*density)),max(12,int(2.5*density));row=yy//py;column=(xx+(row%2)*(px//2))%px
  seam=(column<1.5)|((yy%py)<1.5);highlight=((column>=1.5)&(column<3.5))|(((yy%py)>=1.5)&((yy%py)<3.5))
  field=np.ones((h,w,4),dtype=np.float32);field[:,:,:3]=palette[None,None,:]*(1+.10*(yy%py)[:,:,None]/py)+brush[:,:,None]
  field[seam,:3]*=.53;field[highlight,:3]=field[highlight,:3]*.78+np.array((.12,.16,.10));field[:,:,:3]=np.clip(field[:,:,:3],0,1)
  stamp('tile',field);changed.append('verdigris copper pagoda role sampled from generated concept')
  stamp('wall',painted_master('wall','Stucco_Cream',(.90,.88,.61)));changed.append('plain yellow-green front plaster role')
  stamp('plant',painted_master('plant','Stucco_Cream',(.88,.89,.86)));changed.append('pale stone guardian-statue role')
 image.pixels.foreach_set(pixels.ravel());image.update();image.filepath_raw=str(stage/f'textures/T_{aid}.png');image.file_format='PNG';image.save()
 image.reload();image.pack();image.unpack(method='REMOVE');image.reload();image.pack()
 obj[paint_version]=True
 assert semantic_signature(obj)==before,'Geometry/UV changed during texture paint'
 entries=json.loads((R/f'work/{aid}_kit_assets.json').read_text());entry=next(a for a in entries if a['id']==aid)
 note=' Visual review hand-painted '+', '.join(changed)+'; other Rodin colour, authored geometry and UVs preserved.'
 prior=' '.join(entry['notes']) if isinstance(entry.get('notes'),list) else entry.get('notes','')
 entry['notes']=prior.split(' Visual review hand-painted ',1)[0]+note
 entry,audit=export_asset(obj,entry,20000,output_root=stage)
 assert semantic_signature(obj)==before,'Geometry/UV changed during final export'
 (R/f'work/{aid}_kit_assets.json').write_text(json.dumps([entry],indent=2)+'\n');(R/f'src/{aid}_export_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
 (W/f'{aid}_paint_audit.json').write_text(json.dumps({'id':aid,'changed':changed,'semantic_uv_geometry_sha256':before,'preserved':True,'export_audit':audit},indent=2))
 bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(source));print('PAINT CLEANUP READY',aid,changed,flush=True)
else:print('Paint cleanup already applied',aid,flush=True)
