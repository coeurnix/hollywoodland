"""Fresh constant-material GLB exports for retry inputs; authored inputs stay frozen."""
import bpy,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
for aid in sys.argv[sys.argv.index('--')+1:]:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.gltf(filepath=str(R/f'work/{aid}_input.glb'))
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];assert len(objects)==1
 obj=objects[0];world=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=world
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 for attr in list(obj.data.color_attributes):obj.data.color_attributes.remove(attr)
 obj.data.materials.clear();mat=bpy.data.materials.new('TextureOnlyNeutral');mat.use_nodes=True;mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.65,.55,.4,1);obj.data.materials.append(mat)
 for poly in obj.data.polygons:poly.material_index=0
 bpy.ops.export_scene.gltf(filepath=str(R/f'work/{aid}_clean_proxy.glb'),export_format='GLB',use_selection=True)
 print('Fresh clean proxy exported:',aid)
