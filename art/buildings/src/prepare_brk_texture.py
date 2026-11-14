"""Option A shared-atlas texture-only input. Run after build_brk_kit.py and concept review."""
import bpy,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'src/BRK_kit_authored.blend'))
plan=[a for a in json.loads((R/'src/kit_plan.json').read_text()) if a['family']=='BRK' and a['kind']=='module']
bpy.ops.object.select_all(action='DESELECT')
layout=[]
for i,a in enumerate(plan):
 o=bpy.data.objects[a['id']];offset=Vector(((i%4)*10,0,(i//4)*6))
 copy=o.copy();copy.data=o.data.copy();bpy.context.collection.objects.link(copy)
 copy.name='TEXTURE_'+a['id'];copy.location=offset;copy.select_set(True)
 layout.append({'id':a['id'],'offset':list(offset),'nominal':a['nominal']})
bpy.ops.export_scene.gltf(filepath=str(R/'work/ATL_BRK_input.glb'),export_format='GLB',use_selection=True)
(R/'work/BRK_texture_layout.json').write_text(json.dumps(layout,indent=2)+'\n')
print('Shared UV input prepared:',len(plan),'modules, texture-only required')
