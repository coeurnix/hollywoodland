import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
report={}
for name in ['BRK_G_Bay6A_input.glb','BRK_G_Bay6A/base_basic_pbr.glb','BRK_G_Bay6A/base_basic_shaded.glb']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.gltf(filepath=str(R/'work'/name))
 obs=[o for o in bpy.context.scene.objects if o.type=='MESH']
 report[name]=[{'vertices':len(o.data.vertices),'faces':len(o.data.polygons),'dimensions':list(o.dimensions),'location':list(o.location),'bounds':[[min((o.matrix_world@v.co)[i] for v in o.data.vertices) for i in range(3)],[max((o.matrix_world@v.co)[i] for v in o.data.vertices) for i in range(3)]],'uv':len(o.data.uv_layers.active.data) if o.data.uv_layers else 0} for o in obs]
 for im in bpy.data.images:
  if im.type=='IMAGE' and im.size[0]>0:
   im.filepath_raw=str(R/'work'/(Path(name).stem+'_'+im.name.replace('/','_')+'.png')); im.file_format='PNG';im.save()
(R/'work'/'rodin_inspection.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
