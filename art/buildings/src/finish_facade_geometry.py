"""Add visible backs and coping thickness after atlas baking, then measure exports.
No hidden bottom faces. Source fronts and their UVs remain unchanged.
"""
import bpy,bmesh,json,sys,math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1]
FAM=sys.argv[sys.argv.index('--')+1]
STAGE=R/f'work/{FAM}_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
entries=json.loads((R/f'work/{FAM}_kit_assets.json').read_text())
audits=json.loads((R/f'src/{FAM}_kit_export_audit.json').read_text())
for a in entries:
 if not any(role in a['id'] for role in ['Crest','Parapet']):continue
 obj=bpy.data.objects[a['id']]
 if obj.get('visible_roofward_back'):continue
 bm=bmesh.new();bm.from_mesh(obj.data);layer=bm.loops.layers.uv.active
 oldfaces=list(bm.faces);oldverts=list(bm.verts)
 boundary=[(e.link_loops[0].vert,e.link_loops[0].link_loop_next.vert) for e in bm.edges if len(e.link_faces)==1]
 newverts={v:bm.verts.new((v.co.x,v.co.y+.25,v.co.z)) for v in oldverts}
 width,height=a['nominal']
 for f in oldfaces:
  nf=bm.faces.new([newverts[v] for v in reversed(f.verts)])
  sx,sy=(248,960) if 'Parapet' in a['id'] else ((248,1300) if a['id'].endswith('A') else (1080,960))
  for l in nf.loops:
   l[layer].uv=((sx+l.vert.co.x/width*800)/2048,(sy+l.vert.co.z/height*(300 if 'Parapet' in a['id'] else 400))/2048)
 for va,vb in boundary:
  if va.co.z<1e-6 and vb.co.z<1e-6:continue
  nf=bm.faces.new([vb,va,newverts[va],newverts[vb]])
  length=min(880,max(4,(vb.co-va.co).length*200))
  for l,uv in zip(nf.loops,[(80,968),(80,968+length),(130,968+length),(130,968)]):l[layer].uv=(uv[0]/2048,uv[1]/2048)
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 obj['visible_roofward_back']=True
 a['notes']+=' Authored 0.25 m coping thickness and finished roofward back; hidden base omitted.'
 entry,audit=export_asset(obj,a,600,output_root=STAGE);a.update(entry);audits[a['id']]=audit
if FAM=='SPN' and not bpy.context.scene.get('spanish_tile_cap_painted'):
 import numpy as np
 image=next(n.image for n in bpy.data.materials['M_ATL_SPN'].node_tree.nodes if n.type=='TEX_IMAGE')
 pixels=np.empty(2048*2048*4,dtype=np.float32);image.pixels.foreach_get(pixels);pixels=pixels.reshape(2048,2048,4)
 tile=bpy.data.images.load(str(R/'textures/T_TILE_Roof_RedTile.png'),check_existing=True)
 source=np.empty(2048*2048*4,dtype=np.float32);tile.pixels.foreach_get(source);source=source.reshape(2048,2048,4)
 yy,xx=np.mgrid[0:300,0:800];z=yy/200;top=1.25+.25*np.sin(np.pi*xx/799)
 cap=(z>top-.18)&(z<=top+.04)
 region=pixels[960:1260,248:1048]
 region[cap]=source[((yy[cap]-200)*8)%2048,(xx[cap]*2)%2048]
 image.pixels.foreach_set(pixels.ravel());image.update();image.filepath_raw=str(STAGE/'textures/T_ATL_SPN.png');image.file_format='PNG';image.save();image.pack()
 bpy.context.scene['spanish_tile_cap_painted']=True
 for a in entries:
  if 'Parapet' in a['id']:a['notes']+=' Curved stucco silhouette with hand-painted barrel-tile coping from the seamless roof master.'
(R/f'work/{FAM}_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n')
(R/f'src/{FAM}_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
print('Visible coping and crest backs finished:',FAM)
