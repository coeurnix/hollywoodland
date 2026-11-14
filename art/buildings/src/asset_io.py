"""Static mesh exports and actual FBX measurements shared by subsequent phases."""
import bpy,bmesh
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def export_asset(obj,metadata,budget,output_root=None):
 output_root=Path(output_root) if output_root else R
 (output_root/'meshes').mkdir(parents=True,exist_ok=True)
 bpy.ops.object.select_all(action='DESELECT');obj.hide_set(False);obj.select_set(True);bpy.context.view_layer.objects.active=obj
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 aid=metadata['id'];path=output_root/'meshes'/f'{aid}.fbx'
 bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_anim=False,use_mesh_modifiers=True,use_triangles=True,path_mode='RELATIVE',add_leaf_bones=False)
 bpy.ops.export_scene.gltf(filepath=str(output_root/'meshes'/f'{aid}.glb'),export_format='GLB',use_selection=True)
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path));check=next(o for o in set(bpy.data.objects)-old if o.type=='MESH')
 vv=[check.matrix_world@v.co for v in check.data.vertices]
 bounds=[[round(fn(v[i] for v in vv),6) for i in range(3)] for fn in [min,max]]
 bm=bmesh.new();bm.from_mesh(check.data)
 audit={'bounds':bounds,'tris':sum(len(p.vertices)-2 for p in check.data.polygons),'material_slots':len(check.material_slots),'triangulated':all(len(p.vertices)==3 for p in check.data.polygons),'scale':list(check.scale),'rotation':list(check.rotation_euler),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts)}
 bm.free();bpy.data.objects.remove(check,do_unlink=True)
 assert audit['tris']<=budget,(aid,audit['tris'],budget)
 assert audit['material_slots']==1 and audit['triangulated']
 assert audit['zero_area_faces']==audit['branch_edges']==audit['loose_vertices']==0,(aid,audit)
 entry=dict(metadata);entry.update(file=f'meshes/{aid}.fbx',bounds=bounds,tris=audit['tris'])
 return entry,audit
