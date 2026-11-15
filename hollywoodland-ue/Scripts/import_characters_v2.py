# Imports the v2 characters (art/characters_v2/export, written by tools/charv2/export_v2.py) into /Game/CharactersV2.
# Run: Scripts\ImportCharacters.ps1 -Script import_characters_v2.py [-Only mat,tex,mesh,anim]
#   M_HLCharV2        lit skinned surface: Tex * lerp(1, TintA, Mask.R) * lerp(1, TintB, Mask.G) * Mul; vertices whose
#                     zone bit (VertexColor.R * 31) is set in HideMask collapse onto the actor (a garment worn over them
#                     hides them, HLCharV2.cpp)
#   M_HLCharV2Masked  the same, alpha-tested on Tex.A and two-sided (brows, lashes, hair cards)
# Clips: the Mannequin's stock set is retargeted straight onto each v2 skeleton (fingers included); the commissioned v1
# clips (melee, casting, sitting, emotes) are retargeted from the v1 skeleton.
# The FBXs are centimetre files with an unscaled armature (export_v2.py): no import scale here, or the root bone carries it.
import json, os, glob, struct, zlib, tempfile
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXPORT = os.path.join(ROOT, 'art', 'characters_v2', 'export')
BASE = '/Game/CharactersV2'
V1 = '/Game/Characters'
ONLY = os.environ.get('HL_IMPORT_ONLY', '')
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG false')
SEXES = ['Male', 'Female']
# the FBX (Blender, facing -Y) comes in facing +Y like the Mannequin with no extra turn; a yaw here mirrors every
# retargeted clip front to back (checked against SKM_Manny: toes and right hand must sit on the same sides)
FACE_PLUS_Y = unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)


def want(k):
    return not ONLY or k in ONLY.split(',')


def log(s):
    unreal.log('HL_ASSET ' + s)


def import_png(f, folder, name):
    t = unreal.AssetImportTask()
    for k, v in (('filename', f), ('destination_path', folder), ('destination_name', name), ('replace_existing', True),
                 ('automated', True), ('save', False)):
        t.set_editor_property(k, v)
    return t


# ---------------------------------------------------------------- materials
def new_material(name):
    full = f'{BASE}/{name}'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    return tools.create_asset(name, BASE, unreal.Material, unreal.MaterialFactoryNew()), full


def expr(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def tex_param(m, name, x, y, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, default='/Engine/EngineResources/WhiteSquareTexture'):
    return expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name,
                texture=unreal.load_asset(default), sampler_type=sampler)


def vec_param(m, name, x, y, v=(1, 1, 1, 1)):
    return expr(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*v))


def scalar_param(m, name, x, y, v):
    return expr(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


def binop(m, cls, a, b, x, y, a_out='', b_out=''):
    e = mel.create_material_expression(m, cls, x, y)
    mel.connect_material_expressions(a, a_out, e, 'A')
    mel.connect_material_expressions(b, b_out, e, 'B')
    return e


def lerp(m, a, b, alpha, x, y, alpha_out=''):
    e = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    mel.connect_material_expressions(a, '', e, 'A')
    mel.connect_material_expressions(b, '', e, 'B')
    mel.connect_material_expressions(alpha, alpha_out, e, 'Alpha')
    return e


def custom_input(name):
    c = unreal.CustomInput()
    c.set_editor_property('input_name', name)
    return c


def no_mask():
    """A 4x4 black linear mask texture: the Mask parameter's default (no tint areas). A Masks sampler needs a linear
    texture as its default, or the whole material fails to compile."""
    path = f'{BASE}/Tex/T_V2_NoMask'
    if eal.does_asset_exist(path):
        return path
    raw = b''.join(bytes(1 + 12) for _ in range(4))  # filter byte + 4 black RGB pixels per row

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = bytes([0x89]) + b'PNG' + bytes([13, 10, 26, 10]) + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    f = os.path.join(tempfile.gettempdir(), 'T_V2_NoMask.png')
    with open(f, 'wb') as h:
        h.write(png)
    tools.import_asset_tasks([import_png(f, f'{BASE}/Tex', 'T_V2_NoMask')])
    tex = unreal.load_asset(path)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    eal.save_asset(path, only_if_is_dirty=False)
    return path


def char_material(name, masked):
    m, full = new_material(name)
    m.set_editor_property('used_with_skeletal_mesh', True)
    m.set_editor_property('used_with_static_lighting', False)
    tex = tex_param(m, 'Tex', -1400, 0)
    msk = tex_param(m, 'Mask', -1400, 300, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, no_mask())
    one = expr(m, unreal.MaterialExpressionConstant3Vector, -1400, 600, constant=unreal.LinearColor(1, 1, 1, 1))
    ta, tb = vec_param(m, 'TintA', -1400, 750), vec_param(m, 'TintB', -1400, 950)
    la = lerp(m, one, ta, msk, -1100, 700, 'R')
    lb = lerp(m, one, tb, msk, -1100, 900, 'G')
    color = binop(m, unreal.MaterialExpressionMultiply, binop(m, unreal.MaterialExpressionMultiply, tex, la, -800, 300, 'RGB'), lb, -600, 400)
    color = binop(m, unreal.MaterialExpressionMultiply, color, vec_param(m, 'Mul', -800, 600), -450, 400)  # skin tone / hair colour
    mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = scalar_param(m, 'Glow', -500, 700, 0.22)
    mel.connect_material_property(binop(m, unreal.MaterialExpressionMultiply, color, glow, -250, 650), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar_param(m, 'Roughness', -250, 800, 0.8), '', unreal.MaterialProperty.MP_ROUGHNESS)
    # zone hiding: bit (VertexColor.R * 31) of HideMask set -> the vertex sinks under the garment worn over it
    vc = expr(m, unreal.MaterialExpressionVertexColor, -1400, 1300)
    hm = scalar_param(m, 'HideMask', -1400, 1450, 0.0)
    cu = expr(m, unreal.MaterialExpressionCustom, -1100, 1350, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
              code='float id = floor(Z * 31.0 + 0.5); return fmod(floor(M / exp2(id) + 0.0001), 2.0);',
              inputs=[custom_input('Z'), custom_input('M')])
    mel.connect_material_expressions(vc, 'R', cu, 'Z')
    mel.connect_material_expressions(hm, '', cu, 'M')
    # (sunk 4.5 cm along the inward normal rather than collapsed onto the origin: a triangle on a zone border then only
    # dips under the garment instead of stretching into a long sliver across the body)
    nrm = expr(m, unreal.MaterialExpressionVertexNormalWS, -1100, 1550)
    k = expr(m, unreal.MaterialExpressionConstant, -1100, 1650)
    k.set_editor_property('r', -4.5)
    d = binop(m, unreal.MaterialExpressionMultiply, nrm, k, -850, 1600)
    mel.connect_material_property(binop(m, unreal.MaterialExpressionMultiply, d, cu, -600, 1450), '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if masked:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property('two_sided', True)
        mel.connect_material_property(tex, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
        m.set_editor_property('opacity_mask_clip_value', 0.35)
    mel.recompile_material(m)
    eal.save_asset(full, only_if_is_dirty=False)
    log(f'{full} saved')


def make_materials():
    char_material('M_HLCharV2', False)
    char_material('M_HLCharV2Masked', True)


# ---------------------------------------------------------------- textures
def import_textures():
    tasks, names = [], []
    looks = os.path.join(os.path.dirname(EXPORT), 'looks')
    # only what the game uses: the bodies' own maps and every texture characters_v2.json names (the base garments'
    # unpainted maps stay out of the package)
    data = json.load(open(os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'characters_v2.json'), encoding='utf-8'))
    used = set()
    for per in data['looks'].values():
        for slots in per.values():
            used |= {p['tex'] for p in slots.values()}
    for lst in data['hair'].values():
        used |= {h['tex'] for h in lst if h['tex']}
    hat_tex = {h['tex'] for per in data['hats'].values() for h in per.values()}
    slot_of = {p['tex']: slot for per in data['looks'].values() for slots in per.values() for slot, p in slots.items()}
    used |= hat_tex
    for f in glob.glob(os.path.join(EXPORT, 'SKV2_*_D.png')) + glob.glob(os.path.join(EXPORT, 'SKV2_*_M.png')) + \
            glob.glob(os.path.join(looks, 'SKV2_*_D.png')) + glob.glob(os.path.join(looks, 'SKV2_*_M.png')):
        name = 'T_' + os.path.basename(f)[:-4]
        if '_Body_' not in name and name not in used:
            continue
        tasks.append(import_png(f, f'{BASE}/Tex', name))
        names.append(name)
    tools.import_asset_tasks(tasks)
    n = 0
    for name in names:
        tex = unreal.load_asset(f'{BASE}/Tex/{name}')
        if not tex:
            log(f'FAILED texture {name}')
            continue
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        # the web package budget (itch: 500 MiB; masters stay 2K): the skin 2K, everything else 512 but the small pieces
        # (shoes, gloves, belts, wrist pieces, hats, shirts, trousers) at 256
        body = '_Body_' in name
        skin = body and not any(k in name for k in ('eye', 'lash', 'brow', 'high-poly'))
        coat = any(k in name for k in ('_m_suit', '_m_dinner', '_m_bowtie', '_m_jacket', '_m_longcoat', '_f_gown', '_f_midi', '_f_day',
                                       '_f_suit', '_f_jacket', '_f_longcoat', 'cuirass'))
        small = name in hat_tex or any(k in name for k in ('_gloves_', 'boot', 'oxford', 'saddle', 'heels', 'maryjane', '_ankle',
                                                             'belt_', 'cuff', 'bracer'))
        # shirts and trousers sit mostly under coats and between belts: 256 as well
        small = small or slot_of.get(name) in ('shirt', 'trousers', 'shoes', 'gloves', 'belt', 'wrist')
        tex.set_editor_property('max_texture_size', 2048 if skin else 256 if small and not coat else 512)
        if name.endswith('_M'):
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        eal.save_asset(f'{BASE}/Tex/{name}', only_if_is_dirty=False)
        n += 1
    log(f'textures {n}/{len(names)}')


# ---------------------------------------------------------------- meshes
def fbx_task(file, dest, name, options):
    t = unreal.AssetImportTask()
    for k, v in (('filename', file), ('destination_path', dest), ('destination_name', name), ('replace_existing', True),
                 ('automated', True), ('save', True), ('options', options)):
        t.set_editor_property(k, v)
    return t


def skel_options(skeleton=None):
    o = unreal.FbxImportUI()
    for k, v in (('import_mesh', True), ('import_as_skeletal', True), ('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH),
                 ('import_animations', False), ('import_materials', False), ('import_textures', False), ('create_physics_asset', False)):
        o.set_editor_property(k, v)
    if skeleton:
        o.set_editor_property('skeleton', skeleton)
    d = o.get_editor_property('skeletal_mesh_import_data')
    d.set_editor_property('import_morph_targets', True)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)  # tangents computed here
    d.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    d.set_editor_property('use_t0_as_ref_pose', False)
    d.set_editor_property('import_rotation', FACE_PLUS_Y)
    return o


def static_options():
    o = unreal.FbxImportUI()
    for k, v in (('import_mesh', True), ('import_as_skeletal', False), ('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH),
                 ('import_materials', False), ('import_textures', False), ('import_animations', False)):
        o.set_editor_property(k, v)
    d = o.get_editor_property('static_mesh_import_data')
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('import_rotation', FACE_PLUS_Y)
    return o


def skeleton_for(sex):
    return unreal.load_asset(f'{BASE}/Body/SKV2_{sex}_Body_Skeleton')


def import_meshes():
    for sex in SEXES:
        f = os.path.join(EXPORT, f'SKV2_{sex}_Body.fbx')
        if not os.path.exists(f):
            continue
        tools.import_asset_tasks([fbx_task(f, f'{BASE}/Body', f'SKV2_{sex}_Body', skel_options())])
        eal.save_asset(f'{BASE}/Body/SKV2_{sex}_Body_Skeleton', only_if_is_dirty=False)
        log(f'body {sex}: skeleton {skeleton_for(sex)}')
    tasks, n = [], 0
    # only the meshes characters_v2.json names (and each hair style's hat-safe variant)
    data = json.load(open(os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'characters_v2.json'), encoding='utf-8'))
    used = {p['mesh'] for per in data['looks'].values() for slots in per.values() for p in slots.values()}
    used |= {h['mesh'] for lst in data['hair'].values() for h in lst if h['mesh']}
    used |= {h['mesh'] + 'hat' for lst in data['hair'].values() for h in lst if h['mesh']}
    used |= {h['mesh'] for per in data['hats'].values() for h in per.values()}
    for f in glob.glob(os.path.join(EXPORT, 'SKV2_*.fbx')):
        name = os.path.basename(f)[:-4]
        if name.endswith('_Body') or name not in used:
            continue
        sex = 'Female' if name.startswith('SKV2_Female') else 'Male'
        if name.endswith('_hat'):
            tasks.append(fbx_task(f, f'{BASE}/Hats', name, static_options()))
        else:
            tasks.append(fbx_task(f, f'{BASE}/Garments', name, skel_options(skeleton_for(sex))))
        n += 1
    tools.import_asset_tasks(tasks)
    log(f'garments/hats {n}')


# ---------------------------------------------------------------- animation
STOCK = {
    'Idle': 'Anims/Unarmed/MM_Idle', 'Walk': 'Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd', 'WalkBack': 'Anims/Unarmed/Walk/MF_Unarmed_Walk_Bwd',
    'Run': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd', 'RunBack': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Bwd',
    'StrafeL': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Left', 'StrafeR': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Right',
    'JumpStart': 'Anims/Unarmed/Jump/MM_Jump', 'JumpLoop': 'Anims/Unarmed/Jump/MM_Fall_Loop', 'JumpLand': 'Anims/Unarmed/Jump/MM_Land',
    'Death': 'Anims/Death/MM_Death_Front_01', 'Death_B': 'Anims/Death/MM_Death_Back_01',
    'Attack_Unarmed': 'Anims/Unarmed/Attack/MM_Attack_01', 'Attack_Unarmed_B': 'Anims/Unarmed/Attack/MM_Attack_02',
    'Attack_Unarmed_C': 'Anims/Unarmed/Attack/MM_Attack_03', 'Shoot_Pistol': 'Anims/Pistol/MM_Pistol_Fire',
    'Combat_Ranged1H': 'Anims/Pistol/MF_Pistol_Idle_ADS', 'Run_Pistol': 'Anims/Pistol/Jog/MF_Pistol_Jog_Fwd',
    'Shoot_Rifle': 'Anims/Rifle/MM_Rifle_Fire', 'Combat_Rifle': 'Anims/Rifle/MF_Rifle_Idle_ADS', 'Run_Rifle': 'Anims/Rifle/Jog/MF_Rifle_Jog_Fwd',
    'HitReact': 'Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01',
}
MQ = f'{V1}/Mannequins'
MQ_MESH = {'Male': 'Meshes/SKM_Manny_Simple', 'Female': 'Meshes/SKM_Quinn_Simple'}


def fresh(name, folder, cls, factory):
    full = f'{folder}/{name}'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    return tools.create_asset(name, folder, cls, factory)


def ik_rig(name, mesh, folder):
    rig = fresh(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    c.apply_auto_generated_retarget_definition()
    log(f'{name}: root {c.get_retarget_root()} chains {[str(x.get_editor_property("chain_name")) for x in c.get_retarget_chains()]}')
    eal.save_asset(f'{folder}/{name}', only_if_is_dirty=False)
    return rig


def retarget(sex, src_mesh, src_rig, tgt_mesh, tgt_rig, clips, label):
    """clips: {canonical name: source asset path}; writes BASE/Anim/A_<Sex>_<name>."""
    rtg = fresh(f'RTG_{label}_To_{sex}', f'{BASE}/Rigs', unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    rc.add_default_ops()
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    try:
        rc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    except Exception as e:
        log(f'auto_align_all_bones: {e}')
    eal.save_asset(f'{BASE}/Rigs/RTG_{label}_To_{sex}', only_if_is_dirty=False)
    assets = []
    for name, path in clips.items():
        ad = eal.find_asset_data(path)
        if ad.is_valid():
            assets.append((name, ad))
        else:
            log(f'missing source {path}')
    tmp = f'{BASE}/Anim/Tmp_{label}_{sex}'
    unreal.IKRetargetBatchOperation.duplicate_and_retarget([a for _, a in assets], src_mesh, tgt_mesh, rtg, '', '', 'RT_', '', tmp, False, False, True)
    n = 0
    for name, ad in assets:
        have = f'{tmp}/RT_{ad.asset_name}'
        dest = f'{BASE}/Anim/A_{sex}_{name}'
        if not eal.does_asset_exist(have):
            log(f'no output for {name} ({have})')
            continue
        if eal.does_asset_exist(dest):
            eal.delete_asset(dest)
        eal.rename_asset(have, dest)
        seq = unreal.load_asset(dest)
        if isinstance(seq, unreal.AnimSequence):
            seq.set_editor_property('enable_root_motion', False)
            seq.set_editor_property('force_root_lock', True)
            seq.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_NONE)
        eal.save_asset(dest, only_if_is_dirty=False)
        n += 1
    eal.delete_directory(tmp)
    log(f'{sex} {label}: {n}/{len(assets)} clips')


def import_anims():
    for sex in SEXES:
        tgt_mesh = unreal.load_asset(f'{BASE}/Body/SKV2_{sex}_Body')
        if not tgt_mesh:
            continue
        tgt_rig = ik_rig(f'IK_V2_{sex}', tgt_mesh, f'{BASE}/Rigs')
        mq_mesh = unreal.load_asset(f'{MQ}/{MQ_MESH[sex]}')
        if mq_mesh:
            mq_rig = ik_rig(f'IK_MQ_{sex}', mq_mesh, f'{BASE}/Rigs')
            retarget(sex, mq_mesh, mq_rig, tgt_mesh, tgt_rig, {k: f'{MQ}/{v}' for k, v in STOCK.items()}, 'Mannequin')
        # the commissioned v1 clips (everything in /Game/Characters/Anim for this sex that is not a stock clip)
        v1_mesh = unreal.load_asset(f'{V1}/Body/SK_{sex}_Default')
        if v1_mesh:
            v1_rig = ik_rig(f'IK_V1_{sex}', v1_mesh, f'{BASE}/Rigs')
            clips = {}
            for p in eal.list_assets(f'{V1}/Anim', recursive=False):
                n = p.split('.')[0].split('/')[-1]
                if n.startswith(f'A_{sex}_'):
                    k = n[len(f'A_{sex}_'):]
                    if k not in STOCK and k != 'ROM':
                        clips[k] = p.split('.')[0]
            retarget(sex, v1_mesh, v1_rig, tgt_mesh, tgt_rig, clips, 'V1')


if want('mat'):
    make_materials()
if want('tex'):
    import_textures()
if want('mesh'):
    import_meshes()
if want('anim'):
    import_anims()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('HL_ASSETS_DONE')
