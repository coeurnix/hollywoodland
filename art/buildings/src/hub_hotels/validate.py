"""Strict welded GLB and asset_io FBX roundtrip checks, no central file writes."""
import bpy,bmesh,json,sys,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[2];SRC=R/'src/hub_hotels';OUT=R/'work/hub_hotels/audit';OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(R/'src'));from asset_io import export_asset
def welded_audit(obj):
 bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces))
 a={'tris':len(bm.faces),'vertices':len(bm.verts),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'orientation_discontinuities':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges)}
 uv=bm.loops.layers.uv.active
 a['uv_zero_area_triangles']=sum(abs((f.loops[1][uv].uv-f.loops[0][uv].uv).cross(f.loops[2][uv].uv-f.loops[0][uv].uv))<1e-12 for f in bm.faces)
 bm.free();return a
records=[]
for aid in ['LM_Roosevelt','LM_HollywoodHotel','LM_HollywoodHotel_Wing','LM_HollywoodHotel_CourtWall']:
 bpy.ops.wm.read_factory_settings(use_empty=True);path=R/f'work/{aid}_input.glb';bpy.ops.import_scene.gltf(filepath=str(path));obj=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 pre=welded_audit(obj);meta=json.loads((SRC/f'{aid}.json').read_text());entry,fbxaudit=export_asset(obj,{'id':aid,'kind':'landmark','nominal':meta['metadata']['nominal']},meta['metadata']['budget'],OUT)
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(OUT/entry['file']));check=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');post=welded_audit(check)
 rec={'id':aid,'input_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'glb_welded':pre,'fbx_roundtrip':fbxaudit,'fbx_welded':post};records.append(rec)
 print(aid,json.dumps(rec),flush=True)
(SRC/'strict_validation.json').write_text(json.dumps({'version':1,'assets':records},indent=2)+'\n')
for r in records:
 for key in ['glb_welded','fbx_welded']:
  a=r[key];assert a['branch_edges']==a['zero_area_faces']==a['loose_vertices']==a['orientation_discontinuities']==a['uv_zero_area_triangles']==0,(r['id'],key,a)
print('ALL_HOTELS_STRICT_PASS',flush=True)
