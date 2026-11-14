import bpy,bmesh,json
from pathlib import Path
R=Path(__file__).resolve().parents[2];W=R/'work/hub_egyptian';reports=[]
for aid in ['LM_Egyptian','LM_Egyptian_Wing','LM_Egyptian_Gateway','LM_RedCarStop','LM_Kiosk_Auction']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(R/'work'/f'{aid}_input.glb'));ob=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.verts.ensure_lookup_table();bm.edges.ensure_lookup_table();bm.faces.ensure_lookup_table()
 badedges=[e for e in bm.edges if len(e.link_faces)>2];zeros=[f for f in bm.faces if f.calc_area()<1e-10];inconsistent=[e for e in bm.edges if len(e.link_faces)==2 and not e.is_contiguous]
 report={'id':aid,'verts':len(bm.verts),'faces':len(bm.faces),'branch':len(badedges),'zero':len(zeros),'loose':sum(not v.link_faces for v in bm.verts),'inconsistent':len(inconsistent),'bad_edges':[{'v':[[round(x,6) for x in v.co] for v in e.verts],'faces':[[[round(x,6) for x in v.co] for v in f.verts] for f in e.link_faces]} for e in badedges],'zeros':[[list(v.co) for v in f.verts] for f in zeros]};reports.append(report);bm.free();print(aid,{k:v for k,v in report.items() if k not in ['bad_edges','zeros']},flush=True)
(W/'weld_diagnostics.json').write_text(json.dumps(reports,indent=2))
