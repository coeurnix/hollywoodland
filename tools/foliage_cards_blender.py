# Blender side of tools/foliage_cards.py: three crossed vertical quads per sprite, GLB out (1 unit = 1 m, Y up after
# export, pivot at the bottom centre). Run by foliage_cards.py; argument: the jobs JSON.
import bpy, json, math, sys

jobs = json.load(open(sys.argv[sys.argv.index('--') + 1]))
for j in jobs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    h = j['height']
    w = h  # the sprite is square: the quad spans height x height
    mesh = bpy.data.meshes.new(j['id'])
    verts, faces, uvs = [], [], []
    for k in range(3):
        a = math.radians(60 * k)
        dx, dy = math.cos(a) * w / 2, math.sin(a) * w / 2
        base = len(verts)
        verts += [(-dx, -dy, 0), (dx, dy, 0), (dx, dy, h), (-dx, -dy, h)]
        faces.append((base, base + 1, base + 2, base + 3))
        uvs += [(0, 0), (1, 0), (1, 1), (0, 1)]
    mesh.from_pydata(verts, [], faces)
    uvl = mesh.uv_layers.new(name='UVMap')
    for li, loop in enumerate(mesh.loops):
        uvl.data[li].uv = uvs[loop.vertex_index]
    # normals point up-ish so the cards light like the ground under them (no black backsides on the web tier)
    mesh.update()
    ob = bpy.data.objects.new(j['id'], mesh)
    bpy.context.scene.collection.objects.link(ob)
    mat = bpy.data.materials.new(f"M_{j['id']}")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')
    img = bpy.data.images.load(j['sprite'])
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = img
    nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    nt.links.new(tex.outputs['Alpha'], bsdf.inputs['Alpha'])
    bsdf.inputs['Roughness'].default_value = 0.9
    mat.blend_method = 'CLIP' if hasattr(mat, 'blend_method') else None
    mesh.materials.append(mat)
    bpy.ops.export_scene.gltf(filepath=j['glb'], export_format='GLB', export_yup=True, export_image_format='AUTO')
    print(f"HL_CARD {j['id']} h {h} -> {j['glb']}")
