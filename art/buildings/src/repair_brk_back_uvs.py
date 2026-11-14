"""Migrate first thickness trial to stacked front/back UVs without rebaking."""
import bpy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];stage=R/'work/BRK_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/'src/BRK_kit_textured.blend'))
entries=json.loads((R/'work/BRK_kit_assets.json').read_text());audits=json.loads((R/'src/BRK_kit_export_audit.json').read_text())
for a in entries:
 if not any(s in a['id'] for s in ['Crest','Parapet']):continue
 obj=bpy.data.objects[a['id']];uv=obj.data.uv_layers.active.data;w,h=a['nominal']
 sx,sy=(248,960) if 'Parapet' in a['id'] else ((248,1300) if a['id'].endswith('A') else (1080,960))
 for p in obj.data.polygons:
  if p.normal.y>.5:
   for i in p.loop_indices:
    v=obj.data.vertices[obj.data.loops[i].vertex_index].co
    uv[i].uv=((sx+v.x/w*800)/2048,(sy+v.z/h*(300 if 'Parapet' in a['id'] else 400))/2048)
  elif abs(p.normal.y)<.5:
   for i in p.loop_indices:uv[i].uv.x+=60/2048
 entry,audit=export_asset(obj,a,600,output_root=stage);a.update(entry);audits[a['id']]=audit
(R/'work/BRK_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/'src/BRK_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/BRK_kit_textured.blend'))
