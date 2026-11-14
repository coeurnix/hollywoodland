"""Keep curved Spanish coping wholly in the upper30% stretchable band."""
import bpy,json,sys,math,numpy as np
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];stage=R/'work/SPN_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/'src/SPN_kit_textured.blend'))
if not bpy.context.scene.get('spanish_plain70_profile'):
 im=next(n.image for n in bpy.data.materials['M_ATL_SPN'].node_tree.nodes if n.type=='TEX_IMAGE')
 if im.packed_file:im.unpack(method='REMOVE')
 p=np.empty(2048*2048*4,dtype=np.float32);im.pixels.foreach_get(p);p=p.reshape(2048,2048,4)
 plain=p[960:1160,248:1048].copy();p[960:1260,248:1048]=plain[np.arange(300)*200//300]
 tile=bpy.data.images.load(str(R/'textures/T_TILE_Roof_RedTile.png'),check_existing=True);src=np.empty(2048*2048*4,dtype=np.float32);tile.pixels.foreach_get(src);src=src.reshape(2048,2048,4)
 yy,xx=np.mgrid[0:300,0:800];z=yy/200;top=1.25+.25*np.sin(np.pi*xx/799);mask=(z>=top-.18)&(z<=top+.04)
 role=p[960:1260,248:1048];role[mask]=src[((yy[mask]-200)*8)%2048,(xx[mask]*2)%2048]
 im.pixels.foreach_set(p.ravel());im.update();im.filepath_raw=str(stage/'textures/T_ATL_SPN.png');im.file_format='PNG';im.save();im.pack()
 entries=json.loads((R/'work/SPN_kit_assets.json').read_text());audits=json.loads((R/'src/SPN_kit_export_audit.json').read_text())
 for a in entries:
  if 'Parapet' not in a['id']:continue
  obj=bpy.data.objects[a['id']];width=a['nominal'][0]
  for v in obj.data.vertices:
   if v.co.z>.99:v.co.z=1.25+.25*math.sin(math.pi*v.co.x/width)
  for poly in obj.data.polygons:
   if abs(poly.normal.y)<.8:continue
   for i in poly.loop_indices:
    v=obj.data.vertices[obj.data.loops[i].vertex_index].co;obj.data.uv_layers.active.data[i].uv=((248+v.x/width*800)/2048,(960+v.z/1.5*300)/2048)
  a['notes']+=' Curved cap starts above z1.05, preserving the required plain lower70%.'
  entry,audit=export_asset(obj,a,600,output_root=stage);a.update(entry);audits[a['id']]=audit
 (R/'work/SPN_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/'src/SPN_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
 bpy.context.scene['spanish_plain70_profile']=True
 bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/SPN_kit_textured.blend'))
