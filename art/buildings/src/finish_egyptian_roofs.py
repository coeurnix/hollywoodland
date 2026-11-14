"""Blender atlas-paint cleanup of repeated roof-role contamination after Rodin bake."""
import bpy,json,sys,numpy as np
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];aid=sys.argv[sys.argv.index('--')+1];stage=R/f'work/{aid}_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{aid}_kit_textured.blend'))
obj=bpy.data.objects[aid];mat=obj.data.materials[0];image=next(n.image for n in mat.node_tree.nodes if n.type=='TEX_IMAGE')
if not obj.get('roof_role_paint_fixed'):
 if image.packed_file:image.unpack(method='REMOVE')
 pixels=np.empty(2048*2048*4,dtype=np.float32);image.pixels.foreach_get(pixels);pixels=pixels.reshape(2048,2048,4)
 meta=json.loads((R/f'work/hub_egyptian/{aid}_metadata.json').read_text());slots=meta['uv']['slots']
 for role,tile_name in [('roof','Stucco_Cream' if aid.endswith('Gateway') else 'Roof_TarGravel'),('tile','Roof_RedTile')]:
  x,y,w,h,density=slots[role];tile=bpy.data.images.load(str(R/f'textures/T_TILE_{tile_name}.png'),check_existing=True)
  source=np.empty(2048*2048*4,dtype=np.float32);tile.pixels.foreach_get(source);source=source.reshape(2048,2048,4)
  yy,xx=np.mgrid[0:h,0:w];period=max(1,int(4*density))
  pixels[y:y+h,x:x+w]=source[(yy%period)*2048//period,(xx%period)*2048//period]
  pixels[y-8:y,x:x+w]=pixels[y:y+1,x:x+w];pixels[y+h:y+h+8,x:x+w]=pixels[y+h-1:y+h,x:x+w]
  pixels[y-8:y+h+8,x-8:x]=pixels[y-8:y+h+8,x:x+1];pixels[y-8:y+h+8,x+w:x+w+8]=pixels[y-8:y+h+8,x+w-1:x+w]
 image.pixels.foreach_set(pixels.ravel());image.update();image.filepath_raw=str(stage/f'textures/T_{aid}.png');image.file_format='PNG';image.save();image.pack();obj['roof_role_paint_fixed']=True
 # Explicit reload prevents stale packed bytes in the preview/source/GLB.
 image.unpack(method='REMOVE');image.reload();image.pack()
 entries=json.loads((R/f'work/{aid}_kit_assets.json').read_text());entry=entries[0]
 entry['notes']=(' '.join(entry['notes']) if isinstance(entry.get('notes'),list) else entry.get('notes',''))+' Repeated roof/tile atlas fields hand-fixed in Blender with the painted seamless masters, removing facade-like roof artifacts from the Rodin bake.'
 entry,audit=export_asset(obj,entry,20000 if aid=='LM_Egyptian' else 10000,output_root=stage)
 (R/f'work/{aid}_kit_assets.json').write_text(json.dumps([entry],indent=2)+'\n');(R/f'src/{aid}_export_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
 bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{aid}_kit_textured.blend'))
