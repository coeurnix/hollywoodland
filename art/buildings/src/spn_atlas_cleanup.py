"""Blender texture-paint equivalent: fill bake misses using nearby painted stucco."""
import bpy,json,sys,numpy as np
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];stage=R/'work/SPN_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/'src/SPN_kit_textured.blend'))
im=next(n.image for n in bpy.data.materials['M_ATL_SPN'].node_tree.nodes if n.type=='TEX_IMAGE')
p=np.empty(2048*2048*4,dtype=np.float32);im.pixels.foreach_get(p);p=p.reshape(2048,2048,4)
role=p[960:1260,248:1048];black=role[:,:,:3].max(axis=2)<.055
yy,xx=np.where(black);role[yy,xx]=p[980+(yy%60),248+xx]
pillar=p[960:1860,16:216].copy();brightness=pillar[:,:,:3].mean(axis=(0,2));valid=np.where(brightness>.10)[0]
if len(valid):
 left,right=48,156
 p[960:1860,16:216]=pillar[:,left+np.arange(200)*(right-left)//199]
tile=bpy.data.images.load(str(R/'textures/T_TILE_Roof_RedTile.png'),check_existing=True)
source=np.empty(2048*2048*4,dtype=np.float32);tile.pixels.foreach_get(source);source=source.reshape(2048,2048,4)
yy,xx=np.mgrid[0:300,0:800];z=yy/200;top=1.25+.25*np.sin(np.pi*xx/799)
cap=(z>top-.18)&(z<=top+.04)
role[cap]=source[((yy[cap]-200)*8)%2048,(xx[cap]*2)%2048]
for x0,y0,w,h in [(248,960,800,300),(16,960,200,900)]:
 p[y0-8:y0,x0:x0+w]=p[y0:y0+1,x0:x0+w]
 p[y0+h:y0+h+8,x0:x0+w]=p[y0+h-1:y0+h,x0:x0+w]
 p[y0-8:y0+h+8,x0-8:x0]=p[y0-8:y0+h+8,x0:x0+1]
 p[y0-8:y0+h+8,x0+w:x0+w+8]=p[y0-8:y0+h+8,x0+w-1:x0+w]
im.pixels.foreach_set(p.ravel());im.update();im.filepath_raw=str(stage/'textures/T_ATL_SPN.png');im.file_format='PNG';im.save();im.pack()
entries=json.loads((R/'work/SPN_kit_assets.json').read_text());audits={}
for a in entries:
 if 'Parapet' in a['id'] or 'Pilaster' in a['id'] or 'Corner' in a['id']:
  a['notes']+=' Bake misses filled from adjacent Rodin-painted stucco; pillar role cropped to its painted surface.'
 entry,audit=export_asset(bpy.data.objects[a['id']],a,600 if a['kind']=='module' else 60,output_root=stage);a.update(entry);audits[a['id']]=audit
(R/'work/SPN_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/'src/SPN_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/SPN_kit_textured.blend'))
