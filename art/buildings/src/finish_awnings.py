"""Thin closed canopy shells make undersides visible with one-sided game materials."""
import bpy,bmesh,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];FAM=sys.argv[sys.argv.index('--')+1];stage=R/f'work/{FAM}_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
entries=json.loads((R/f'work/{FAM}_kit_assets.json').read_text());audits=json.loads((R/f'src/{FAM}_kit_export_audit.json').read_text())
for a in entries:
 if 'Awning' not in a['id']:continue
 obj=bpy.data.objects[a['id']]
 if obj.get('canopy_shell'):continue
 bpy.context.view_layer.objects.active=obj;obj.hide_set(False)
 mod=obj.modifiers.new('Authored canopy thickness','SOLIDIFY');mod.thickness=.025;mod.offset=0;mod.use_even_offset=True
 bpy.ops.object.modifier_apply(modifier=mod.name)
 bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
 hidden=[f for f in bm.faces if all(v.co.y>-.02 for v in f.verts) and abs(f.normal.y)>.8]
 bmesh.ops.delete(bm,geom=hidden,context='FACES');bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free();obj['canopy_shell']=True
 a['notes']+=' Thin authored canopy shell provides a textured underside; hidden wall-facing closing strip omitted.'
 entry,audit=export_asset(obj,a,600,output_root=stage);a.update(entry);audits[a['id']]=audit
(R/f'work/{FAM}_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/f'src/{FAM}_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
