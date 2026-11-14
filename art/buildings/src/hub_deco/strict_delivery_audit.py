"""Audit frozenGLB inputs and actual FBX roundtrips; never alter frozen inputs."""
import bpy,bmesh,json,sys,runpy
from pathlib import Path
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'src'))
from asset_io import export_asset
ids=['LM_Pantages','LM_Taft','LM_FirstNational','LM_BrownDerby','LM_PoliceStation']
results={}
for aid in ids:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.gltf(filepath=str(R/f'work/{aid}_input.glb'))
 obj=next(o for o in bpy.context.scene.objects if o.type=='MESH');obj.name=aid
 bpy.context.view_layer.objects.active=obj;obj.select_set(True)
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bm=bmesh.new();bm.from_mesh(obj.data)
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 audit={'tris':len(bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'noncontiguous_twoface_edges':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges)}
 bm.to_mesh(obj.data);bm.free()
 result={'frozen_input_after_weld_and_tri':audit}
 assert audit['branch_edges']==audit['zero_area_faces']==audit['loose_vertices']==audit['noncontiguous_twoface_edges']==0,(aid,audit)
 metadata=json.loads((R/f'src/hub_deco/{aid}_metadata.json').read_text())
 budget=20000 if aid in ['LM_Pantages','LM_Taft'] else 10000
 entry,fbx=export_asset(obj,metadata,budget,output_root=R/f'work/hub_deco/strict_audit/{aid}')
 result['actual_FBX_roundtrip']=fbx
 if aid=='LM_Pantages':
  runpy.run_path(str(R/'src/hub_deco/LM_Pantages_finish.py'))['apply'](obj)
  entry,finished=export_asset(obj,metadata,budget,output_root=R/f'work/hub_deco/strict_audit/{aid}_finished')
  result['post_finish_FBX_roundtrip']=finished
 results[aid]=result
 (R/'src/hub_deco/strict_delivery_audit.json').write_text(json.dumps(results,indent=2)+'\n')
 print('STRICT_PASS',aid,json.dumps(result))
