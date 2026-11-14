import bpy,bmesh,json
from pathlib import Path
R=Path(__file__).resolve().parents[2]
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(R/'work/hub_egyptian/graumans_roundtrip/meshes/LM_Graumans.fbx'));ob=next(o for o in bpy.context.scene.objects if o.type=='MESH');bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
report=[{'edge':[list(v.co) for v in e.verts],'faces':[[list(v.co) for v in f.verts] for f in e.link_faces]} for e in bm.edges if len(e.link_faces)>2];print(json.dumps(report,indent=2));bm.free()
