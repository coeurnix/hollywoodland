"""Frozen GLB -> positional weld -> asset_io actual FBX proof, no input edits."""
import bpy,bmesh,json,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2];W=R/'work/hub_egyptian';sys.path.insert(0,str(R/'src'));from asset_io import export_asset
reports=[]
for aid,budget in [('LM_Graumans',20000),('LM_Graumans_ForecourtWall',2000)]:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(R/'work'/f'{aid}_input.glb'));ob=next(o for o in bpy.context.scene.objects if o.type=='MESH');ob.name=aid
 bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces))
 welded={'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'inconsistent_shared_edges':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges)};assert all(v==0 for v in welded.values()),(aid,welded);bm.to_mesh(ob.data);bm.free()
 entry,audit=export_asset(ob,{'id':aid},budget,W/'graumans_input_proof');report={'id':aid,'input_welded':welded,'actual_fbx_roundtrip':audit};reports.append(report);print('FROZEN INPUT FBX PASS',aid,audit['tris'],flush=True)
(W/'graumans_input_audit.json').write_text(json.dumps(reports,indent=2))
