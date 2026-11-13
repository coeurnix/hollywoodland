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

# id: (file, budget, scale (uniform or [width, height, length] in model axes), pivot)
P = {
    'brick_rubble': ('01-brick-rubble', SET14, 1.4, 'floor'),
    'red_car': ('02-red-car', BIG, [0.75, 0.72, 1.0], 'floor'),          # 3.9 m wide, 6.1 m tall read toy-like next to people
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
    'coffee_tin_b': ('coffee-tin-suncrest', TINY, 1.0, 'floor'),
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
    'callbox_alt': ('police-call-box-alt', SMALL, 0.85, 'floor'),
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
    'mailbox': ('us-mailbox', SMALL, 1.0, 'floor'),
    'stair_wooden': ('wooden-staircase', MED, 1.0, 'floor'),
    'wreck_van': ('wrecked-red-van', CAR, 1.0, 'floor'),
}


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


def bounds(j, s):
    mn, mx = [1e9] * 3, [-1e9] * 3
    for m in j['meshes']:
        for p in m['primitives']:
            a = j['accessors'][p['attributes']['POSITION']]
            for k in range(3):
                mn[k] = min(mn[k], a['min'][k]); mx[k] = max(mx[k], a['max'][k])
    return [(mx[k] - mn[k]) * s[k] for k in range(3)], [mn[k] * s[k] for k in range(3)], [mx[k] * s[k] for k in range(3)]


def main():
    os.makedirs(os.path.join(STAGE, 'Tex'), exist_ok=True)
    os.makedirs(os.path.join(STAGE, 'Mesh'), exist_ok=True)
    os.makedirs(os.path.join(STAGE, 'Opt'), exist_ok=True)
    stage, table, jobs, opt_jobs = {'textures': [], 'meshes': []}, {}, [], []
    for pid, (file, (tris, bpx, npx, mpx), scale, pivot) in P.items():
        path = os.path.join(ART, file + '.glb')
        j, binc = read_glb(path)
        name = camel(pid)
        s = [scale] * 3 if not isinstance(scale, list) else scale
        mat = j['materials'][0]
        pbr = mat.get('pbrMetallicRoughness', {})
        tex = {}
        for key, ref, px in (('B', pbr.get('baseColorTexture'), bpx), ('N', mat.get('normalTexture'), npx), ('MR', pbr.get('metallicRoughnessTexture'), mpx)):
            if not ref or not px:
                continue
            im = image_bytes(j, binc, ref['index']).convert('RGB')
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
        opt = os.path.join(STAGE, 'Opt', file + '.glb')
        opt_jobs.append({'in': path, 'out': opt, 'error': ERROR, 'max_tris': tris})
        jobs.append({'glb': opt, 'fbx': fbx, 'scale': s, 'tris': 10 ** 9})  # no Blender decimation
        stage['meshes'].append({'file': fbx, 'name': f'SM_{name}', 'material': f'MI_{name}', 'tex': tex})
        size, mn, mx = bounds(j, s)
        # model axes (glTF): x width, y height, z length (front +Z). Unreal after the turn: X length (front), Y width, Z height.
        table[pid] = {'mesh': f'/Game/Props/SM_{name}.SM_{name}', 'size': [round(size[2], 2), round(size[0], 2), round(size[1], 2)],
                      'zmin': round(mn[1], 2), 'pivot': pivot, 'source': file + '.glb'}
        opt_file = os.path.join(STAGE, 'opt_jobs.json')
        json.dump(opt_jobs, open(opt_file, 'w'), indent=1)
        r = subprocess.run(['node', os.path.join(ROOT, 'tools', 'props', 'optimize.mjs'), opt_file], capture_output=True, text=True, shell=os.name == 'nt')
        print('\n'.join(l for l in r.stdout.splitlines() if l.startswith('HL_OPT')))
        if r.returncode != 0:
            print(r.stdout[-3000:], r.stderr[-3000:])
            sys.exit(1)
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
