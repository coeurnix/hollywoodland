"""Prepares the Rodin prop package (art/props, see its README.txt) for the Unreal import.

Per prop (table below): a proportion/scale fix where the README's sizes read wrong in the game, a soft triangle
ceiling and a texture budget. Geometry: tools/props/optimize.mjs welds and simplifies with meshoptimizer to a geometric
error bound (0.05% of the prop's size), so detail and UV seams survive; the ceiling only pushes that bound up to 4x on
props far over it. (Blender's collapse decimation of the unwelded Rodin meshes tore seams open.) Unreal adds gentle
LOD1/LOD2 for distance. Textures come straight out of the GLB:
base colour, normal (green flipped: glTF is OpenGL-style, Unreal DirectX-style) and metallic-roughness, each resized.
Orientation: every mesh is turned so its front (glTF +Z) faces Unreal +X, pivot unchanged (floor / ceiling / wall).

Writes:
  hollywoodland-ue/Saved/PropStage/Tex/T_<Name>_{B,N,MR}.png   textures for Scripts/import_props.py
  hollywoodland-ue/Saved/PropStage/Mesh/SM_<Name>.fbx          meshes (Blender 5.3, tools/propprep_blender.py)
  hollywoodland-ue/Saved/PropStage/stage.json                  what to import
  hollywoodland-ue/Content/Data/props.json                     runtime table: id -> asset, size in metres, pivot

Run: python -I tools/propprep.py   (BLENDER env var overrides the Blender 5.3 path)
"""
import io, json, os, struct, subprocess, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'art', 'props')
STAGE = os.path.join(ROOT, 'hollywoodland-ue', 'Saved', 'PropStage')
DATA = os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'props.json')
BLENDER = os.environ.get('BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.3\blender.exe')

# budgets: (max triangles, base colour px, normal px or 0, metallic-roughness px or 0)
TINY = (10000, 512, 512, 256)
SMALL = (16000, 1024, 512, 256)
MED = (20000, 1024, 512, 256)
CAR = (25000, 1024, 1024, 512)
BIG = (32000, 1024, 1024, 512)
SET14 = (8000, 1024, 0, 0)  # the hand-picked set: base colour only, already light
ERROR = 0.0005  # meshoptimizer error bound, fraction of the prop's size
TREE = (30000, 1024, 512, 0)
SHRUB = (6000, 512, 256, 0)
ROCK = (3000, 512, 512, 0)
CARD = (64, 512, 0, 0)    # crossed alpha cards (art/foliage sprites): base colour with alpha only
WORLD = (20000, 1024, 512, 256)
LETTER = (4000, 512, 0, 0)    # seen from 700 m away
INTER = (6000, 512, 256, 0)
EGY = (12000, 1024, 512, 0)
# a file is art/props/<file>.glb unless it names another package: 'world:<category>/<name>' (art/world/models) or
# 'foliage:<id>' (art/foliage: raw/<id>.glb from Rodin, else the card in models/<id>.glb). A scale of ('h', metres)
# scales uniformly to that height. FOLIAGE ids sway in the wind (M_HLFoliage; the value is the sway strength).
FOLIAGE = {'palm_fan': 1.0, 'palm_canary': 1.2, 'tree_eucalyptus': 1.2, 'tree_pepper': 1.6, 'shrub_chaparral': 0.6, 'shrub_oleander': 0.8,
           'agave': 0.0, 'yucca': 0.5, 'cactus_pear': 0.0, 'grass_dry': 5.0, 'grass_green': 4.0, 'flower_poppy': 5.0, 'flower_lupine': 4.0,
           'flower_daisy': 5.0, 'flower_mustard': 5.0}

# id: (file, budget, scale (uniform or [width, height, length] in model axes), pivot)
P = {
    'brick_rubble': ('01-brick-rubble', SET14, 1.4, 'floor'),
    'hive_ceiling': ('03-ceiling-hive-cluster', SET14, 2.0, 'ceiling'),
    'hive_tendrils': ('04-hive-tendril-bundle', SET14, 1.0, 'floor'),
    'crates': ('05-shipping-crates', SET14, 1.0, 'floor'),
    'coffee_sacks': ('06-coffee-sacks', SET14, 1.0, 'floor'),
    'ballroom_furniture': ('07-ballroom-furniture', SET14, 1.0, 'floor'),
    'sign_max_factor': ('08-max-factor-sign', SET14, 1.8, 'wall'),
    'sign_gun_shop': ('09-hollywood-gun-shop-sign', SET14, 1.8, 'wall'),
    'sign_blade_tailor': ('10a-blade-tailor', SET14, 1.3, 'wall'),
    'sign_blade_photo': ('10b-blade-photo', SET14, 1.3, 'wall'),
    'sign_fascia': ('11-fascia-sign', SET14, 2.6, 'wall'),
    'sign_neon': ('12-neon-variant', SET14, 1.8, 'wall'),
    'marquee': ('13-under-canopy-marquee', SET14, 3.4, 'ceiling'),
    'arc_lamp_a': ('arc-lamp-a', SMALL, 1.25, 'floor'),
    'arc_lamp_b': ('arc-lamp-b', SMALL, 1.25, 'floor'),
    'chandelier_rubble': ('chandelier-on-rubble', MED, 1.3, 'floor'),
    'car_chevrolet_blue': ('chevrolet-coach-blue', CAR, 1.0, 'floor'),
    'car_chevrolet': ('chevrolet-coach', CAR, 1.0, 'floor'),
    'coffee_tin_a': ('coffee-tin-sierra-valley', TINY, 1.0, 'floor'),
    'shed_collapsed': ('collapsed-shed', MED, 1.0, 'floor'),
    'van_delivery': ('delivery-van-hollywoodland', CAR, 1.0, 'floor'),
    'pole_fallen': ('fallen-utility-pole', MED, 1.0, 'floor'),
    'film_can_a': ('film-can-alt', TINY, 1.0, 'floor'),
    'film_can_b': ('film-can', TINY, 1.0, 'floor'),
    'film_canister': ('film-canister-octagonal', TINY, 1.0, 'floor'),
    'fire_escape_wall': ('fire-escape-brick-wall', BIG, 1.0, 'floor'),
    'fire_escape_ladder': ('fire-escape-ladder', MED, 1.0, 'floor'),
    'fire_escape': ('fire-escape', BIG, 1.0, 'floor'),
    'car_ford_a': ('ford-model-a', CAR, 1.0, 'floor'),
    'hive_arch': ('hive-claw-arch', MED, 1.0, 'floor'),
    'hive_membrane': ('hive-dripping-membrane', MED, 1.0, 'ceiling'),
    'hive_eggs_pale': ('hive-egg-cluster-pale', MED, 1.0, 'floor'),
    'hive_eggs': ('hive-egg-cluster', MED, 1.0, 'floor'),
    'hive_goo': ('hive-goo-puddle', SMALL, 1.0, 'floor'),
    'hive_spire_tall': ('hive-spire-tall', MED, 1.0, 'floor'),
    'hive_spire': ('hive-spire', MED, 1.0, 'floor'),
    'car_sedan_burgundy': ('luxury-sedan-burgundy', CAR, 1.0, 'floor'),
    'car_model_t': ('model-t-pickup', CAR, 1.0, 'floor'),
    'rock_mossy': ('mossy-rock-outcrop', SMALL, 1.0, 'floor'),
    'truck_starlight_wreck': ('newsreel-truck-starlight-wreck', CAR, 1.0, 'floor'),
    'truck_wrecked': ('newsreel-truck-wrecked', CAR, 1.0, 'floor'),
    'truck_newsreel': ('newsreel-truck', CAR, 1.0, 'floor'),
    'callbox_blue': ('police-call-box-blue', SMALL, 0.85, 'floor'),
    'callbox': ('police-call-box', SMALL, 0.85, 'floor'),
    'wall_ruined': ('ruined-stone-wall', MED, 1.0, 'floor'),
    'wreck_rusted': ('rusted-sedan-wreck', CAR, 1.0, 'floor'),
    'sandbags': ('sandbag-wall', MED, 1.0, 'floor'),
    'scrap_pile': ('scrap-pile', SMALL, 1.0, 'floor'),
    'wreck_charred': ('sedan-wreck-charred', CAR, 1.0, 'floor'),
    'sign_cafe': ('sign-silver-screen-cafe', SMALL, 1.3, 'wall'),
    'sign_drugstore': ('sign-starlight-drug', SMALL, 1.5, 'wall'),
    'sign_haberdashery': ('sign-sunset-haberdashery', SMALL, 1.2, 'wall'),
    'stair_landing': ('stairway-landing', MED, 1.0, 'floor'),
    'street_lamp': ('street-lamp-double', MED, 1.0, 'floor'),
    'track_curve': ('streetcar-track-curve', SMALL, 1.0, 'floor'),
    'track_straight': ('streetcar-track-straight', SMALL, 1.0, 'floor'),
    'theatre_facade': ('theater-facade-riviera', BIG, 1.0, 'floor'),
    'tent_triage': ('triage-tent', MED, 1.0, 'floor'),
    'trolley_pole': ('trolley-wire-pole', MED, 1.0, 'floor'),
    'car_tudor': ('tudor-car-green', CAR, 1.0, 'floor'),
    'stair_wooden': ('wooden-staircase', MED, 1.0, 'floor'),
    'wreck_van': ('wrecked-red-van', CAR, 1.0, 'floor'),
    # art/world (README there): mounts under riders, the Pacific Electric car, wrecks, interactables, the Egyptian kit, the sign
    'redcar_pe': ('world:red-car/redcar-pacific-electric', BIG, [1.0, 0.8, 1.0], 'floor'),
    'mount_horse': ('world:mounts/mount-horse', WORLD, 1.0, 'floor'),
    'mount_scout': ('world:mounts/mount-motorcycle-indian-scout', WORLD, 1.0, 'floor'),
    'mount_model_a': ('world:mounts/mount-ford-model-a', WORLD, 1.0, 'floor'),
    'mount_police': ('world:mounts/mount-ford-model-a-police', WORLD, 1.0, 'floor'),
    'wreck_newsreel': ('world:wrecks/wreck-newsreel-truck', WORLD, 1.0, 'floor'),
    'wreck_overturned': ('world:wrecks/wreck-sedan-overturned', WORLD, 1.0, 'floor'),
    'wreck_burned': ('world:wrecks/wreck-model-a-burned', WORLD, 1.0, 'floor'),
    'wreck_hive_truck': ('world:wrecks/wreck-delivery-truck-hive', WORLD, 1.0, 'floor'),
    'int_film_cans': ('world:interactables/int-film-can-stack', INTER, 1.0, 'floor'),
    'int_film_reel': ('world:interactables/int-film-reel', INTER, 1.0, 'floor'),
    'int_coffee_tin': ('world:interactables/int-coffee-tin', INTER, 1.6, 'floor'),
    'int_arc_lamp': ('world:interactables/int-arc-lamp', INTER, 1.0, 'floor'),
    'int_callbox': ('world:interactables/int-police-call-box', INTER, 1.0, 'floor'),
    'int_mailbox': ('world:interactables/int-us-mailbox', INTER, 1.0, 'floor'),
    'int_scrap': ('world:interactables/int-scrap-pile', INTER, 1.0, 'floor'),
    'int_cocoon': ('world:interactables/int-captured-usher-cocoon', INTER, 1.0, 'floor'),
    'egy_column': ('world:egyptian-interior/egy-lotus-column', EGY, 1.0, 'floor'),
    'egy_pharaoh': ('world:egyptian-interior/egy-pharaoh-statue', EGY, 1.0, 'floor'),
    'egy_sphinx': ('world:egyptian-interior/egy-sphinx', EGY, 1.0, 'floor'),
    'egy_booth': ('world:egyptian-interior/egy-ticket-booth', EGY, 1.0, 'floor'),
    'egy_seats': ('world:egyptian-interior/egy-seat-row', EGY, 1.0, 'floor'),
    'egy_projector': ('world:egyptian-interior/egy-projector', EGY, 1.0, 'floor'),
    'egy_stage': ('world:egyptian-interior/egy-proscenium-stage', EGY, 1.0, 'floor'),
    'egy_wall': ('world:egyptian-interior/egy-hieroglyph-wall', EGY, 1.0, 'floor'),
    'egy_organ': ('world:egyptian-interior/egy-pipe-organ-console', EGY, 1.0, 'floor'),
    'sign_h': ('world:hollywoodland-sign/sign-letter-H', LETTER, 1.0, 'floor'),
    'sign_o': ('world:hollywoodland-sign/sign-letter-O', LETTER, 1.0, 'floor'),
    'sign_l': ('world:hollywoodland-sign/sign-letter-L', LETTER, 1.0, 'floor'),
    'sign_y': ('world:hollywoodland-sign/sign-letter-Y', LETTER, 1.0, 'floor'),
    'sign_w': ('world:hollywoodland-sign/sign-letter-W', LETTER, 1.0, 'floor'),
    'sign_d': ('world:hollywoodland-sign/sign-letter-D', LETTER, 1.0, 'floor'),
    'sign_a': ('world:hollywoodland-sign/sign-letter-A', LETTER, 1.0, 'floor'),
    'sign_n': ('world:hollywoodland-sign/sign-letter-N', LETTER, 1.0, 'floor'),
    # art/foliage (tools/foliagegen.py, tools/foliage_cards.py): heights in metres as they stand in the game
    'palm_fan': ('foliage:palm_fan', TREE, ('h', 11.0, 0.7), 'floor'),
    'palm_canary': ('foliage:palm_canary', TREE, ('h', 9.0), 'floor'),
    'tree_eucalyptus': ('foliage:tree_eucalyptus', TREE, ('h', 13.0), 'floor'),
    'tree_pepper': ('foliage:tree_pepper', TREE, ('h', 7.0), 'floor'),
    'shrub_chaparral': ('foliage:shrub_chaparral', SHRUB, ('h', 1.6), 'floor'),
    'shrub_oleander': ('foliage:shrub_oleander', SHRUB, ('h', 2.2), 'floor'),
    'agave': ('foliage:agave', SHRUB, ('h', 1.3), 'floor'),
    'yucca': ('foliage:yucca', SHRUB, ('h', 2.6), 'floor'),
    'cactus_pear': ('foliage:cactus_pear', SHRUB, ('h', 1.4), 'floor'),
    'rock_boulder': ('foliage:rock_boulder', ROCK, ('h', 1.4), 'floor'),
    'rock_flat': ('foliage:rock_flat', ROCK, ('h', 0.6), 'floor'),
    'grass_dry': ('foliage:grass_dry', CARD, 1.0, 'floor'),
    'grass_green': ('foliage:grass_green', CARD, 1.0, 'floor'),
    'flower_poppy': ('foliage:flower_poppy', CARD, 1.0, 'floor'),
    'flower_lupine': ('foliage:flower_lupine', CARD, 1.0, 'floor'),
    'flower_daisy': ('foliage:flower_daisy', CARD, 1.0, 'floor'),
    'flower_mustard': ('foliage:flower_mustard', CARD, 1.0, 'floor'),
}


def source(file):
    if file.startswith('world:'):
        return os.path.join(ROOT, 'art', 'world', 'models', file[6:] + '.glb')
    if file.startswith('foliage:'):
        k = file[8:]
        raw = os.path.join(ROOT, 'art', 'foliage', 'raw', k + '.glb')
        return raw if os.path.exists(raw) else os.path.join(ROOT, 'art', 'foliage', 'models', k + '.glb')
    return os.path.join(ART, file + '.glb')


def camel(pid):
    return ''.join(w.capitalize() for w in pid.split('_'))


def read_glb(path):
    b = open(path, 'rb').read()
    jl = struct.unpack('<I', b[12:16])[0]
    j = json.loads(b[20:20 + jl])
    bin_off = 20 + jl + 8
    return j, b[bin_off:]


def image_bytes(j, binc, tex_index):
    src = j['textures'][tex_index]['source']
    bv = j['bufferViews'][j['images'][src]['bufferView']]
    o = bv.get('byteOffset', 0)
    return Image.open(io.BytesIO(binc[o:o + bv['byteLength']]))


def node_matrix(n):
    """A glTF node's local transform as a 3x4 (rows) matrix."""
    if 'matrix' in n:
        m = n['matrix']  # column-major 4x4
        return [[m[c * 4 + r] for c in range(4)] for r in range(3)]
    x, y, z, w = n.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = n.get('scale', [1, 1, 1])
    t = n.get('translation', [0, 0, 0])
    R = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[R[r][0] * sx, R[r][1] * sy, R[r][2] * sz, t[r]] for r in range(3)]


def mat_mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(3)) + (a[r][3] if c == 3 else 0) for c in range(4)] for r in range(3)]


def bounds(j, s):
    """Scene bounds with the node transforms applied (the art/world GLBs carry their real-world scale on the node)."""
    mn, mx = [1e9] * 3, [-1e9] * 3
    ident = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0]]

    def visit(ni, parent):
        n = j['nodes'][ni]
        m = mat_mul(parent, node_matrix(n))
        if 'mesh' in n:
            for p in j['meshes'][n['mesh']]['primitives']:
                a = j['accessors'][p['attributes']['POSITION']]
                for cx in (a['min'][0], a['max'][0]):
                    for cy in (a['min'][1], a['max'][1]):
                        for cz in (a['min'][2], a['max'][2]):
                            for k in range(3):
                                v = m[k][0] * cx + m[k][1] * cy + m[k][2] * cz + m[k][3]
                                mn[k] = min(mn[k], v); mx[k] = max(mx[k], v)
        for c in n.get('children', []):
            visit(c, m)

    roots = j['scenes'][j.get('scene', 0)]['nodes'] if j.get('scenes') else range(len(j.get('nodes', [])))
    for r in roots:
        visit(r, ident)
    return [(mx[k] - mn[k]) * s[k] for k in range(3)], [mn[k] * s[k] for k in range(3)], [mx[k] * s[k] for k in range(3)]


def main():
    os.makedirs(os.path.join(STAGE, 'Tex'), exist_ok=True)
    os.makedirs(os.path.join(STAGE, 'Mesh'), exist_ok=True)
    os.makedirs(os.path.join(STAGE, 'Opt'), exist_ok=True)
    stage, table, jobs, opt_jobs = {'textures': [], 'meshes': []}, {}, [], []
    for pid, (file, (tris, bpx, npx, mpx), scale, pivot) in P.items():
        path = source(file)
        j, binc = read_glb(path)
        name = camel(pid)
        if isinstance(scale, tuple):  # ('h', metres): uniform, to that height
            size1, _, _ = bounds(j, [1, 1, 1])
            k = scale[1] / max(1e-6, size1[1])
            scale = [k * scale[2], k, k * scale[2]] if len(scale) > 2 else k  # optional third: a width factor
        s = [scale] * 3 if not isinstance(scale, list) else scale
        card = (tris, bpx, npx, mpx) == CARD
        mat = j['materials'][0]
        pbr = mat.get('pbrMetallicRoughness', {})
        tex = {}
        for key, ref, px in (('B', pbr.get('baseColorTexture'), bpx), ('N', mat.get('normalTexture'), npx), ('MR', pbr.get('metallicRoughnessTexture'), mpx)):
            if not ref or not px:
                continue
            im = image_bytes(j, binc, ref['index']).convert('RGBA' if card and key == 'B' else 'RGB')
            if im.width > px:
                im = im.resize((px, px), Image.LANCZOS)
            if key == 'N':
                r, g, b = im.split()
                im = Image.merge('RGB', (r, g.point(lambda v: 255 - v), b))
            out = os.path.join(STAGE, 'Tex', f'T_{name}_{key}.png')
            im.save(out)
            tex[key] = f'T_{name}_{key}'
            stage['textures'].append({'file': out, 'name': f'T_{name}_{key}', 'kind': key})
        fbx = os.path.join(STAGE, 'Mesh', f'SM_{name}.fbx')
        opt = os.path.join(STAGE, 'Opt', pid + '.glb')
        # the generated packages (world, foliage) arrive at ~500k triangles: they go all the way down to the ceiling
        opt_jobs.append({'in': path, 'out': opt, 'error': ERROR, 'max_tris': tris, 'hard': ':' in file})
        jobs.append({'glb': opt, 'fbx': fbx, 'scale': s, 'tris': 10 ** 9, 'ground': file.startswith('foliage:')})  # no Blender decimation
        stage['meshes'].append({'file': fbx, 'name': f'SM_{name}', 'material': f'MI_{name}', 'tex': tex})
        if pid in FOLIAGE:
            stage['meshes'][-1]['foliage'] = FOLIAGE[pid]
            stage['meshes'][-1]['card'] = card
        size, mn, mx = bounds(j, s)
        # model axes (glTF): x width, y height, z length (front +Z). Unreal after the turn: X length (front), Y width, Z height.
        table[pid] = {'mesh': f'/Game/Props/SM_{name}.SM_{name}', 'size': [round(size[2], 2), round(size[0], 2), round(size[1], 2)],
                      'zmin': round(mn[1], 2), 'pivot': pivot, 'source': os.path.relpath(path, ROOT).replace(os.sep, '/')}
        if pid in FOLIAGE:
            table[pid]['foliage'] = True
        if file.startswith('foliage:'):
            table[pid]['zmin'] = 0.0  # propprep_blender re-pivots foliage to its base
    opt_file = os.path.join(STAGE, 'opt_jobs.json')
    json.dump(opt_jobs, open(opt_file, 'w'), indent=1)
    r = subprocess.run(['node', os.path.join(ROOT, 'tools', 'props', 'optimize.mjs'), opt_file], capture_output=True, text=True, shell=os.name == 'nt')
    print('\n'.join(l for l in r.stdout.splitlines() if l.startswith('HL_OPT')))
    if r.returncode != 0:
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit(1)
    # meshoptimizer stalls on some generated meshes (dense UV seams lock it): Blender's collapse finishes those
    left = {l.split()[1]: int(l.split()[4]) for l in r.stdout.splitlines() if l.startswith('HL_OPT')}
    for oj, bj in zip(opt_jobs, jobs):
        if oj['hard'] and left.get(os.path.splitext(os.path.basename(oj['in']))[0], 0) > oj['max_tris'] * 1.2:
            bj['tris'] = oj['max_tris']
    job_file = os.path.join(STAGE, 'blender_jobs.json')
    json.dump(jobs, open(job_file, 'w'), indent=1)
    script = os.path.join(ROOT, 'tools', 'propprep_blender.py')
    r = subprocess.run([BLENDER, '--background', '--factory-startup', '--python', script, '--', job_file], capture_output=True, text=True)
    report = [l for l in r.stdout.splitlines() if l.startswith('HL_PROP')]
    print('\n'.join(report))
    if r.returncode != 0 or len(report) < len(jobs):
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit(1)
    tri_after = {l.split()[1]: int(l.split()[3]) for l in report}
    for pid in table:
        table[pid]['tris'] = tri_after.get(f'SM_{camel(pid)}', 0)
    json.dump(stage, open(os.path.join(STAGE, 'stage.json'), 'w'), indent=1)
    json.dump({'note': 'Generated by tools/propprep.py from art/props. Sizes in metres: [length (front +X), width, height].', 'props': table},
              open(DATA, 'w'), indent=1)
    print(f'{len(table)} props, {sum(t["tris"] for t in table.values())} triangles in all, {len(stage["textures"])} textures')


main()
