"""Strict independent import -> cleanup -> asset_io FBX round-trip proof."""
import bpy,bmesh,json,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2];W=R/'work/hub_egyptian'
sys.path.insert(0,str(R/'src'));sys.path.insert(0,str(R/'src/hub_egyptian'))
from cleanup import apply
from asset_io import export_asset
reports=[]
for aid in ['LM_Egyptian','LM_Egyptian_Wing','LM_Egyptian_Gateway','LM_RedCarStop','LM_Kiosk_Auction']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(R/'work'/f'{aid}_input.glb'));ob=next(o for o in bpy.context.scene.objects if o.type=='MESH');ob.name=aid
 cleanup=apply(ob)
 entry,audit=export_asset(ob,{'id':aid},20000 if aid=='LM_Egyptian' else 1500,W/'cleaned_roundtrip')
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(W/'cleaned_roundtrip'/entry['file']));fbx=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');bm=bmesh.new();bm.from_mesh(fbx.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 welded={'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'inconsistent_shared_edges':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges)};bm.free();assert all(v==0 for v in welded.values()),(aid,welded)
 report={'id':aid,'cleanup':cleanup,'fbx_roundtrip':audit,'post_fbx_weld':welded,'file':entry['file']};reports.append(report)
 print('CLEAN FBX PASS',aid,audit['tris'],cleanup['removed_buried_triangles'],flush=True)
(W/'cleanup_audit.json').write_text(json.dumps(reports,indent=2))
