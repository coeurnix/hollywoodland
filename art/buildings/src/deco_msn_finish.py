"""Post-bake architectural cleanup for DECO/MSN; run after bake_brk_option_b.

Adds readable door push bars to the painted glazing using an existing atlas
swatch and keeps the parapet lower70% plain by redistributing the baked role.
Geometry is authored here; each final FBX is reimported for measurement.
"""
import bpy, bmesh, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1]
FAM=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'DECO'
assert FAM in ['DECO','MSN']
STAGE=R/f'work/{FAM}_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
entries=json.loads((R/f'work/{FAM}_kit_assets.json').read_text())
audits=json.loads((R/f'src/{FAM}_kit_export_audit.json').read_text())
if not bpy.context.scene.get('deco_msn_parapet_cleaned'):
 import numpy as np
 mat=bpy.data.materials['M_ATL_'+FAM]
 image=next(n.image for n in mat.node_tree.nodes if n.type=='TEX_IMAGE')
 pixels=np.empty(2048*2048*4,dtype=np.float32)
 image.pixels.foreach_get(pixels);pixels=pixels.reshape(2048,2048,4)
 source=pixels[960:1260,248:1048].copy()
 # Rodin's clean bottom stucco becomes a 70% stretchable field. Reuse the
 # rest of its own painted architecture in the upper30%; retain border8px.
 pixels[960:1170,248:1048]=source[np.minimum(59,np.arange(210)*60//210)]
 pixels[1170:1260,248:1048]=source[60+np.arange(90)*240//90]
 pixels[952:960,248:1048]=pixels[960:961,248:1048]
 pixels[1260:1268,248:1048]=pixels[1259:1260,248:1048]
 image.pixels.foreach_set(pixels.ravel());image.update()
 image.filepath_raw=str(STAGE/f'textures/T_ATL_{FAM}.png')
 image.file_format='PNG';image.save();image.pack()
 bpy.context.scene['deco_msn_parapet_cleaned']=True
 for a in entries:
  if 'Parapet' in a['id']:
   a['notes']+=' Baked atlas role cleaned so lower70% is plain stretchable stucco and ornament remains in upper30%.'
for a in entries:
 aid=a['id']
 if '_G_' not in aid or 'Damaged' in aid: continue
 obj=bpy.data.objects[aid]
 if obj.get('door_pushbar'): continue
 w=a['nominal'][0]
 x=w/2+.48 if aid.endswith('B') or 'Entry' in aid else w-.78
 y=.95 if 'Entry' in aid else -.055
 # Small open-backed rectangular metal pull, facing the street (-Y).
 bm=bmesh.new();bm.from_mesh(obj.data)
 corners=[(x-.035,y,1.35),(x+.035,y,1.35),(x+.035,y,2.05),(x-.035,y,2.05),
          (x-.035,y+.035,1.35),(x+.035,y+.035,1.35),(x+.035,y+.035,2.05),(x-.035,y+.035,2.05)]
 verts=[bm.verts.new(v) for v in corners]
 layer=bm.loops.layers.uv.active
 for inds in [(0,1,2,3),(0,4,5,1),(3,2,6,7),(0,3,7,4),(1,5,6,2)]:
  f=bm.faces.new([verts[i] for i in inds])
  for l,uv in zip(f.loops,[(.045,.87),(.05,.87),(.05,.90),(.045,.90)]): l[layer].uv=uv
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
 bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 obj['door_pushbar']=True
 a['notes']+=' Authored metal door pull makes painted glazed entrance readable.'
 entry,audit=export_asset(obj,a,600,output_root=STAGE)
 a.update(entry);audits[aid]=audit
(R/f'work/{FAM}_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n')
(R/f'src/{FAM}_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
print('Door pull cleanup complete:',FAM)
