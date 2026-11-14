"""Read-only source check and scratch preview for post-bake crown cleanup."""
import bpy,bmesh,runpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[2]
bpy.ops.wm.open_mainfile(filepath=str(R/'src/hub_deco/LM_Pantages.blend'))
o=bpy.data.objects['LM_Pantages'];runpy.run_path(str(R/'src/hub_deco/LM_Pantages_finish.py'))['apply'](o)
bm=bmesh.new();bm.from_mesh(o.data)
a={'tris':len(o.data.polygons),'branches':sum(len(e.link_faces)>2 for e in bm.edges),'degenerate':sum(f.calc_area()<1e-10 for f in bm.faces),'loose':sum(not v.link_faces for v in bm.verts),'material_slots':len(o.data.materials)}
assert a['branches']==a['degenerate']==a['loose']==0,a
bm.free();(R/'src/hub_deco/LM_Pantages_finish_audit.json').write_text(json.dumps(a,indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath=str(R/'work/hub_deco/LM_Pantages_finished_review.blend'));print(a)
