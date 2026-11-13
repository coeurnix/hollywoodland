# Imports the staged character package (tools/charprep.py -> Saved/CharStage) into /Game/Characters and creates the
# character materials. Run: Scripts\ImportCharacters.ps1 (UnrealEditor-Cmd -run=pythonscript). Idempotent: re-imports
# replace existing assets.
#   M_HLChar       lit skinned body, BaseColor = Atlas (the per-character render target composed by HLCharacter.cpp)
#   M_HLCharLayer  unlit translucent, used to draw one layer into that render target: Layer * TintA * TintB (masked)
#   M_HLHair       lit skinned hair, Tex (grayscale) * Tint
#   M_HLProp       lit static hats / held items, Tex * Tint (+ Glow for lamps)
import json, os, sys
import unreal

STAGE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Saved', 'CharStage', 'stage.json')
BASE = '/Game/Characters'
ONLY = os.environ.get('HL_IMPORT_ONLY', '')  # comma list: mat,tex,mesh,anim (default all)
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
with open(STAGE) as f:
    stage = json.load(f)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG false')


def want(k):
    return not ONLY or k in ONLY.split(',')


def log(s):
    unreal.log('HL_ASSET ' + s)


# ---------------------------------------------------------------- materials
def new_material(name):
    full = f'{BASE}/{name}'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    return tools.create_asset(name, BASE, unreal.Material, unreal.MaterialFactoryNew()), full


def tex_param(m, name, x, y, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, default='/Engine/EngineResources/WhiteSquareTexture'):
    t = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property('parameter_name', name)
    t.set_editor_property('texture', unreal.load_asset(default))
    t.set_editor_property('sampler_type', sampler)
    return t


def vec_param(m, name, x, y, v=(1, 1, 1, 1)):
    p = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property('parameter_name', name)
    p.set_editor_property('default_value', unreal.LinearColor(*v))
    return p


def scalar_param(m, name, x, y, v):
    p = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property('parameter_name', name)
    p.set_editor_property('default_value', v)
    return p


def mul(m, a, b, x, y, a_out='', b_out=''):
    e = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, a_out, e, 'A')
    mel.connect_material_expressions(b, b_out, e, 'B')
    return e


def lit(m, color, full, skinned, glow_default=0.22):
    """BaseColor = color, Emissive = color * Glow (the web tier has no sky light; same as M_HL)."""
    mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = scalar_param(m, 'Glow', -500, 300, glow_default)
    mel.connect_material_property(mul(m, color, glow, -250, 250), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 400)
    rough.set_editor_property('r', 0.85)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if skinned:
        m.set_editor_property('used_with_skeletal_mesh', True)
    mel.recompile_material(m)
    eal.save_asset(full, only_if_is_dirty=False)
    log(f'{full} saved')


def make_materials():
    m, full = new_material('M_HLChar')
    lit(m, tex_param(m, 'Atlas', -800, 0), full, True)
    m, full = new_material('M_HLHair')
    lit(m, mul(m, tex_param(m, 'Tex', -900, 0), vec_param(m, 'Tint', -900, 250, (0.25, 0.16, 0.1, 1)), -600, 50), full, True)
    m, full = new_material('M_HLProp')
    lit(m, mul(m, tex_param(m, 'Tex', -900, 0), vec_param(m, 'Tint', -900, 250), -600, 50), full, False)
    # layer compositing material (drawn with UCanvas into the character's render target)
    m, full = new_material('M_HLCharLayer')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    layer = tex_param(m, 'Layer', -1200, 0)
    # the default mask must itself be a linear (Masks) texture; layers without a mask set TintA = TintB = white
    msk = tex_param(m, 'Mask', -1200, 300, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, f'{BASE}/Tex/Looks/T_Starter_Male_Shirt_Mask')
    ta, tb = vec_param(m, 'TintA', -1200, 600), vec_param(m, 'TintB', -1200, 800)
    one = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -1200, 1000)
    one.set_editor_property('constant', unreal.LinearColor(1, 1, 1, 1))
    la = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -900, 600)
    mel.connect_material_expressions(one, '', la, 'A'); mel.connect_material_expressions(ta, '', la, 'B'); mel.connect_material_expressions(msk, 'R', la, 'Alpha')
    lb = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -900, 800)
    mel.connect_material_expressions(one, '', lb, 'A'); mel.connect_material_expressions(tb, '', lb, 'B'); mel.connect_material_expressions(msk, 'G', lb, 'Alpha')
    tint = mul(m, mul(m, la, lb, -700, 700), vec_param(m, 'Mul', -900, 1000), -600, 800)  # Mul: scalp hair colour
    mel.connect_material_property(mul(m, layer, tint, -450, 200, 'RGB'), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    op = scalar_param(m, 'Opacity', -700, 400, 1.0)
    mel.connect_material_property(mul(m, layer, op, -450, 400, 'A'), '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_asset(full, only_if_is_dirty=False)
    log(f'{full} saved')


# ---------------------------------------------------------------- textures
def import_textures():
    tasks = []
    for t in stage['textures']:
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', t['file'])
        task.set_editor_property('destination_path', f"{BASE}/Tex/{t['folder']}")
        task.set_editor_property('destination_name', t['name'])
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', False)
        tasks.append((task, t))
    tools.import_asset_tasks([x[0] for x in tasks])
    n = 0
    for task, t in tasks:
        path = f"{BASE}/Tex/{t['folder']}/{t['name']}"
        tex = unreal.load_asset(path)
        if not tex:
            log(f'FAILED {path}')
            continue
        g = t['group']
        if g in ('layer', 'mask'):
            # composited 1:1 into the character atlas: full resolution, no mips, never streamed
            tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property('never_stream', True)
            if g == 'mask':
                tex.set_editor_property('srgb', False)
                tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        elif g == 'skin':
            tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property('never_stream', True)
        else:  # hair atlas, hats, held items: sampled directly by meshes
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        eal.save_asset(path, only_if_is_dirty=False)
        n += 1
    log(f'textures {n}/{len(tasks)}')


# ---------------------------------------------------------------- meshes
def fbx_task(file, dest, name, options):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', file)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('options', options)
    return task


# the package's bodies face -Y once imported; UE (and the Mannequin clips we retarget from) face +Y. Turning bodies,
# hair, hats (all authored in body space) and clips half a turn keeps retargeted motion from mirroring front to back.
# Held items stay as they are: they are authored in their own grip frame.
FACE_PLUS_Y = unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0)


def skel_options(skeleton=None):
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True)
    o.set_editor_property('import_as_skeletal', True)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property('import_animations', False)
    o.set_editor_property('import_materials', False)
    o.set_editor_property('import_textures', False)
    o.set_editor_property('create_physics_asset', False)
    if skeleton:
        o.set_editor_property('skeleton', skeleton)
    d = o.get_editor_property('skeletal_mesh_import_data')
    d.set_editor_property('import_morph_targets', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('use_t0_as_ref_pose', False)
    d.set_editor_property('import_rotation', FACE_PLUS_Y)
    return o


def static_options(turn=False):
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True)
    o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False)
    o.set_editor_property('import_textures', False)
    o.set_editor_property('import_animations', False)
    d = o.get_editor_property('static_mesh_import_data')
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    if turn:
        d.set_editor_property('import_rotation', FACE_PLUS_Y)
    return o


def anim_options(skeleton):
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', False)
    o.set_editor_property('import_as_skeletal', True)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
    o.set_editor_property('import_animations', True)
    o.set_editor_property('import_materials', False)
    o.set_editor_property('import_textures', False)
    o.set_editor_property('skeleton', skeleton)
    a = o.get_editor_property('anim_sequence_import_data')
    a.set_editor_property('import_bone_tracks', True)
    a.set_editor_property('remove_redundant_keys', False)
    a.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    a.set_editor_property('import_rotation', FACE_PLUS_Y)
    return o


def skeleton_for(sex):
    return unreal.load_asset(f'{BASE}/Body/SK_{sex}_Default_Skeleton')


def import_meshes():
    # 1) the welded defaults create one skeleton per sex
    for m in stage['meshes']:
        if m['kind'] == 'skeleton':
            tools.import_asset_tasks([fbx_task(m['file'], f'{BASE}/Body', m['name'], skel_options())])
            eal.save_asset(f"{BASE}/Body/{m['name']}_Skeleton", only_if_is_dirty=False)
            log(f"skeleton {m['name']} -> {skeleton_for(m['sex'])}")
    # 2) parts and hair against them, statics for hats and held items
    tasks = []
    for m in stage['meshes']:
        if m['kind'] == 'skeletal':
            tasks.append(fbx_task(m['file'], f"{BASE}/{m['folder']}", m['name'], skel_options(skeleton_for(m['sex']))))
        elif m['kind'] == 'static':
            tasks.append(fbx_task(m['file'], f"{BASE}/{m['folder']}", m['name'], static_options(turn=m['folder'] == 'Hats')))
    tools.import_asset_tasks(tasks)
    ok = sum(1 for m in stage['meshes'] if eal.does_asset_exist(f"{BASE}/{m['folder']}/{m['name']}"))
    log(f'meshes {ok}/{len(stage["meshes"])}')


def import_anims():
    tasks = []
    for a in stage['anims']:
        tasks.append(fbx_task(a['file'], f'{BASE}/Anim', a['name'], anim_options(skeleton_for(a['sex']))))
    tools.import_asset_tasks(tasks)
    ok = sum(1 for a in stage['anims'] if eal.does_asset_exist(f"{BASE}/Anim/{a['name']}"))
    log(f'anims {ok}/{len(stage["anims"])}')


if want('tex'):
    import_textures()
if want('mat'):
    make_materials()
if want('mesh'):
    import_meshes()
if want('anim'):
    import_anims()
# the importer creates skeletons as separate packages that the per-task save does not write
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('HL_ASSETS_DONE')
