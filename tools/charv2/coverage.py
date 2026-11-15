"""How much of each body zone every garment really covers, measured on the exported meshes.

For each body vertex (its zone read from the Zone vertex colour), a ray is cast outward along its normal: the vertex is
covered when the garment's surface lies within 15 cm. Writes art/characters_v2/export/coverage.json:
{mesh: {zone: fraction}}. make_json.py hides a zone under a garment only when that garment covers most of it, so a short
sleeve no longer hides (and shrinks) the whole arm.

Run: blender -b --python tools/charv2/coverage.py
"""
import bpy, json, sys
from pathlib import Path
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
EXPORT = ROOT / 'art' / 'characters_v2' / 'export'
DATA = json.loads((ROOT / 'hollywoodland-ue' / 'Content' / 'Data' / 'characters_v2.json').read_text(encoding='utf-8'))
ZONES = DATA['zones']
REACH = 15.0  # centimetres (the FBX scene is in cm)


def zone_of(c):
    """The FBX importer reads the (linear) Zone colour as sRGB and linearises it: undo that before reading the id."""
    s = c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055
    return int(round(s * 31))


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)


def load(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    return [o for o in set(bpy.data.objects) - before if o.type == 'MESH']


def body_samples(sex):
    clear()
    objs = load(EXPORT / f'SKV2_{sex}_Body.fbx')
    body = max(objs, key=lambda o: len(o.data.vertices))
    me = body.data
    ca = me.color_attributes.get('Zone') or (me.color_attributes[0] if me.color_attributes else None)
    mw = body.matrix_world
    n3 = mw.to_3x3()
    out = []
    for v in me.vertices:
        if ca is None:
            break
        z = zone_of(ca.data[v.index].color[0]) if ca.domain == 'POINT' else 0
        out.append((mw @ v.co, (n3 @ v.normal).normalized(), z))
    if ca is not None and ca.domain != 'POINT':
        # corner colours: take each vertex's first corner
        zone = {}
        for loop in me.loops:
            zone.setdefault(loop.vertex_index, zone_of(ca.data[loop.index].color[0]))
        out = [(p, n, zone.get(i, 0)) for i, (p, n, _) in enumerate(out)] if out else [(mw @ v.co, (n3 @ v.normal).normalized(), zone.get(v.index, 0)) for v in me.vertices]
    return out


def main():
    meshes = {p['mesh'] for per in DATA['looks'].values() for slots in per.values() for p in slots.values()}
    result = {}
    for sex in ('Male', 'Female'):
        samples = body_samples(sex)
        keep = {o.name for o in bpy.data.objects}
        per_zone = {}
        for _, _, z in samples:
            per_zone[z] = per_zone.get(z, 0) + 1
        for mesh in sorted(m for m in meshes if m.startswith(f'SKV2_{sex}_')):
            f = EXPORT / f'{mesh}.fbx'
            if not f.exists():
                continue
            # the body stays loaded; each garment comes and goes
            for o in [o for o in bpy.data.objects if o.name not in keep]:
                bpy.data.objects.remove(o, do_unlink=True)
            objs = load(f)
            verts, polys = [], []
            for o in objs:
                base = len(verts)
                mw = o.matrix_world
                verts += [mw @ v.co for v in o.data.vertices]
                polys += [[base + i for i in p.vertices] for p in o.data.polygons]
            if not polys:
                continue
            tree = BVHTree.FromPolygons(verts, polys)
            hit = {}
            for p, n, z in samples:
                h = tree.ray_cast(p + n * 0.05, n, REACH)
                if h[0] is None:
                    h = tree.ray_cast(p - n * 0.5, n, REACH)  # a garment hugging (or just inside) the skin
                if h[0] is not None:
                    hit[z] = hit.get(z, 0) + 1
            result[mesh] = {ZONES[z]: round(hit.get(z, 0) / per_zone[z], 3) for z in per_zone if z < len(ZONES)}
            print('HL_COV', mesh, {k: v for k, v in result[mesh].items() if v > 0.05}, flush=True)
        clear()
    (EXPORT / 'coverage.json').write_text(json.dumps(result, indent=1))
    print('HL_COV done', len(result))


main()
