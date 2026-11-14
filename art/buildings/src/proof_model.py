import bpy, bmesh
from pathlib import Path
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=1
# Contiguous front surface: projecting cornice is formed by profile strips, not overlapping boxes.
xs=[0,.5,3.9,4.15,5.45,6]
zs=[0,.2,.75,2.9,3.2,3.55,3.75,4.05,4.3,4.5]
verts=[]
for z in zs:
    for x in xs:
        y=-.25 if z>=4.3 else (-.125 if z>=3.75 else 0)
        verts.append((x,y,z))
faces=[]
for j in range(len(zs)-1):
    for i in range(len(xs)-1):
        a=j*len(xs)+i
        faces.append((a,a+1,a+1+len(xs),a+len(xs)))
mesh=bpy.data.meshes.new('BRK_G_Bay6A'); mesh.from_pydata(verts,[],faces);mesh.update()
o=bpy.data.objects.new('BRK_G_Bay6A',mesh);bpy.context.collection.objects.link(o)
bpy.context.view_layer.objects.active=o;o.select_set(True)
uv=mesh.uv_layers.new(name='UVMap')
for p in mesh.polygons:
    for li in p.loop_indices:
        v=mesh.vertices[mesh.loops[li].vertex_index].co
        uv.data[li].uv=(.01+v.x/6*.98,.01+v.z/4.5*.735)
m=bpy.data.materials.new('M_ATL_BRK');m.use_nodes=True
next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Base Color'].default_value=(.5,.25,.12,1)
mesh.materials.append(m)
bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
bpy.ops.wm.save_as_mainfile(filepath=str(R/'src'/'phase1_work.blend'))
bpy.ops.export_scene.gltf(filepath=str(R/'work'/'BRK_G_Bay6A_input.glb'),export_format='GLB',use_selection=True)
print('Model created:',len(mesh.polygons),'triangles')
