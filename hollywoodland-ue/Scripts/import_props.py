# Imports the staged prop package (tools/propprep.py -> Saved/PropStage) into /Game/Props. Run: Scripts\ImportProps.ps1.
#   M_HLWorldProp  lit: BaseColor = B * Tint, Normal = N, Roughness/Metallic from MR (glTF G/B) when HasMR,
#                  Emissive = BaseColor * Glow (the web tier has no sky light; same ambient term as M_HL)
#   MI_<Name>      one instance per prop with its textures
#   SM_<Name>      static mesh, no collision (the sim's nav boxes are the collision), LOD1 50% (under 20% of the screen) / LOD2 25% (under 7%)
import json, os
import unreal

# HL_IMPORT_STAGE picks another staged package (Scripts\ImportBuildings.ps1 -> Saved/BuildStage: /Game/Buildings, buildings.json)
STAGE = os.environ.get('HL_IMPORT_STAGE') or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Saved', 'PropStage', 'stage.json')
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
stage = json.load(open(STAGE))
BASE = stage.get('base', '/Game/Props')
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG false')


def log(s):
    unreal.log('HL_ASSET ' + s)


def import_textures():
    tasks = []
    for t in stage['textures']:
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', t['file'])
        task.set_editor_property('destination_path', f'{BASE}/Tex')
        task.set_editor_property('destination_name', t['name'])
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    n = 0
    for t in stage['textures']:
        path = f"{BASE}/Tex/{t['name']}"
        tex = unreal.load_asset(path)
        if not tex:
            log(f'FAILED {path}')
            continue
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        if t['kind'] == 'N':
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        elif t['kind'] == 'MR':
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        eal.save_asset(path, only_if_is_dirty=False)
        n += 1
    log(f'textures {n}/{len(stage["textures"])}')


def tex_param(m, name, x, y, sampler, default):
    t = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property('parameter_name', name)
    t.set_editor_property('texture', unreal.load_asset(default))
    t.set_editor_property('sampler_type', sampler)
    return t


def scalar(m, name, x, y, v):
    p = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property('parameter_name', name)
    p.set_editor_property('default_value', v)
    return p


def op(m, cls, x, y, a, b, a_out='', b_out=''):
    e = mel.create_material_expression(m, cls, x, y)
    mel.connect_material_expressions(a, a_out, e, 'A')
    mel.connect_material_expressions(b, b_out, e, 'B')
    return e


def make_material():
    full = f'{BASE}/M_HLWorldProp'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    m = tools.create_asset('M_HLWorldProp', BASE, unreal.Material, unreal.MaterialFactoryNew())
    # the building kit draws through instanced static mesh components (HLWorldActor BuildWorld)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    any_mr = next((f"{BASE}/Tex/{t['name']}" for t in stage['textures'] if t['kind'] == 'MR'), None)
    b = tex_param(m, 'B', -1100, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, '/Engine/EngineResources/WhiteSquareTexture')
    n = tex_param(m, 'N', -1100, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, '/Engine/EngineMaterials/DefaultNormal')
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1100, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    col = op(m, unreal.MaterialExpressionMultiply, -800, 0, b, tint, 'RGB')
    mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(op(m, unreal.MaterialExpressionMultiply, -500, 150, col, scalar(m, 'Glow', -800, 200, 0.22)), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    if not any_mr:
        # a package without roughness/metal maps (the building kit): painted, matte, no metal
        c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -800, 450)
        c.set_editor_property('r', 0.85)
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(m)
        eal.save_asset(full, only_if_is_dirty=False)
        log(f'{full} saved (no MR)')
        return m
    mr = tex_param(m, 'MR', -1100, 600, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, any_mr)
    has = scalar(m, 'HasMR', -800, 800, 0.0)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -500, 500)
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -800, 450)
    c.set_editor_property('r', 0.85)
    mel.connect_material_expressions(c, '', rough, 'A'); mel.connect_material_expressions(mr, 'G', rough, 'B'); mel.connect_material_expressions(has, '', rough, 'Alpha')
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    # metallic is damped: with no reflection captures or sky light, metal reads black on the web tier
    metal = op(m, unreal.MaterialExpressionMultiply, -500, 700, op(m, unreal.MaterialExpressionMultiply, -650, 700, mr, has, 'B'), scalar(m, 'MetalScale', -800, 950, 0.3))
    mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_asset(full, only_if_is_dirty=False)
    log(f'{full} saved')
    return m


def make_foliage_material():
    """M_HLFoliage: two-sided, masked by the base colour's alpha (grass and flower cards), and swaying in the wind:
    world position offset = sin(time + a per-plant phase) * height above the pivot * Sway (cm per metre).
    Ambient as M_HLWorldProp (Glow; Scripts/patch_ambient.py adds the hemisphere term)."""
    full = f'{BASE}/M_HLFoliage'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    m = tools.create_asset('M_HLFoliage', BASE, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('opacity_mask_clip_value', 0.4)
    b = tex_param(m, 'B', -1100, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, '/Engine/EngineResources/WhiteSquareTexture')
    n = tex_param(m, 'N', -1100, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, '/Engine/EngineMaterials/DefaultNormal')
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1100, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    col = op(m, unreal.MaterialExpressionMultiply, -800, 0, b, tint, 'RGB')
    mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(op(m, unreal.MaterialExpressionMultiply, -500, 150, col, scalar(m, 'Glow', -800, 200, 0.26)), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(b, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -800, 450)
    c.set_editor_property('r', 0.9)
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_ROUGHNESS)
    # wind
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1700, 900)
    op_ = mel.create_material_expression(m, unreal.MaterialExpressionObjectPositionWS, -1700, 1050)
    hz = op(m, unreal.MaterialExpressionSubtract, -1450, 950, wp, op_)
    hmask = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1300, 950)
    hmask.set_editor_property('r', False); hmask.set_editor_property('g', False); hmask.set_editor_property('b', True); hmask.set_editor_property('a', False)
    mel.connect_material_expressions(hz, '', hmask, '')
    hmax = mel.create_material_expression(m, unreal.MaterialExpressionMax, -1150, 950)
    hmax.set_editor_property('const_b', 0.0)
    mel.connect_material_expressions(hmask, '', hmax, 'A')
    # phase: per plant (its position) plus a little along the height, so tall plants bend rather than slide
    pxy = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1450, 1150)
    pxy.set_editor_property('r', True); pxy.set_editor_property('g', True); pxy.set_editor_property('b', False); pxy.set_editor_property('a', False)
    mel.connect_material_expressions(op_, '', pxy, '')
    dot = mel.create_material_expression(m, unreal.MaterialExpressionDotProduct, -1300, 1150)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -1450, 1250)
    vc.set_editor_property('r', 0.0021); vc.set_editor_property('g', 0.0017)
    mel.connect_material_expressions(pxy, '', dot, 'A'); mel.connect_material_expressions(vc, '', dot, 'B')
    t = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1450, 1350)
    tsp = op(m, unreal.MaterialExpressionMultiply, -1300, 1350, t, scalar(m, 'WindSpeed', -1450, 1450, 1.6))
    hph = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -1000, 1050)
    hph.set_editor_property('const_b', 0.0015)
    mel.connect_material_expressions(hmax, '', hph, 'A')
    phase = op(m, unreal.MaterialExpressionAdd, -1000, 1250, op(m, unreal.MaterialExpressionAdd, -1150, 1250, dot, tsp), hph)
    s1 = mel.create_material_expression(m, unreal.MaterialExpressionSine, -850, 1200)
    s1.set_editor_property('period', 6.2831853)
    mel.connect_material_expressions(phase, '', s1, '')
    ph2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -850, 1350)
    ph2.set_editor_property('const_b', 2.7)
    mel.connect_material_expressions(phase, '', ph2, 'A')
    s2 = mel.create_material_expression(m, unreal.MaterialExpressionSine, -700, 1350)
    s2.set_editor_property('period', 6.2831853)
    mel.connect_material_expressions(ph2, '', s2, '')
    s2s = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -550, 1350)
    s2s.set_editor_property('const_b', 0.35)
    mel.connect_material_expressions(s2, '', s2s, 'A')
    wave = op(m, unreal.MaterialExpressionAdd, -550, 1200, s1, s2s)
    # cm of sway = wave * height (cm) / 100 * Sway
    amt = op(m, unreal.MaterialExpressionMultiply, -400, 1100, hmax, scalar(m, 'Sway', -550, 1000, 1.0))
    amt2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 1100)
    amt2.set_editor_property('const_b', 0.01)
    mel.connect_material_expressions(amt, '', amt2, 'A')
    off = op(m, unreal.MaterialExpressionMultiply, -200, 1200, wave, amt2)
    offy = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -100, 1300)
    offy.set_editor_property('const_b', 0.55)
    mel.connect_material_expressions(off, '', offy, 'A')
    xy = op(m, unreal.MaterialExpressionAppendVector, 0, 1200, off, offy)
    zero = mel.create_material_expression(m, unreal.MaterialExpressionConstant, 0, 1350)
    zero.set_editor_property('r', 0.0)
    wpo = op(m, unreal.MaterialExpressionAppendVector, 150, 1250, xy, zero)
    mel.connect_material_property(wpo, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    eal.save_asset(full, only_if_is_dirty=False)
    log(f'{full} saved')
    return m


def make_instances(parent, foliage=None):
    made = set()
    for me in stage['meshes']:
        if me['material'] in made:
            continue
        made.add(me['material'])
        path = f"{BASE}/Mat/{me['material']}"
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        mi = tools.create_asset(me['material'], f'{BASE}/Mat', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if foliage and 'foliage' in me:
            mel.set_material_instance_parent(mi, foliage)
            mel.set_material_instance_scalar_parameter_value(mi, 'Sway', float(me['foliage']))
            for k, tex in me['tex'].items():
                if k in ('B', 'N'):
                    mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(f'{BASE}/Tex/{tex}'))
            eal.save_asset(path, only_if_is_dirty=False)
            continue
        mel.set_material_instance_parent(mi, parent)
        for k, tex in me['tex'].items():
            mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(f'{BASE}/Tex/{tex}'))
        if any(t['kind'] == 'MR' for t in stage['textures']):
            mel.set_material_instance_scalar_parameter_value(mi, 'HasMR', 1.0 if 'MR' in me['tex'] else 0.0)
        eal.save_asset(path, only_if_is_dirty=False)
    log(f'instances {len(stage["meshes"])}')


def static_options():
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
    d.set_editor_property('generate_lightmap_u_vs', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    # Blender's metre FBX arrives at 1 unit = 1 cm, front along +Y: scale to centimetres and turn the front to +X
    d.set_editor_property('import_uniform_scale', 100.0)
    d.set_editor_property('import_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
    return o


def import_meshes():
    tasks = []
    for me in stage['meshes']:
        # a re-import over an existing mesh keeps its old import settings (scale, rotation): start clean
        if eal.does_asset_exist(f"{BASE}/{me['name']}"):
            eal.delete_asset(f"{BASE}/{me['name']}")
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', me['file'])
        task.set_editor_property('destination_path', BASE)
        task.set_editor_property('destination_name', me['name'])
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', False)
        task.set_editor_property('options', static_options())
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    # get_editor_subsystem is None under -run=pythonscript; a plain instance of the subsystem class runs the same code
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()
    lods = unreal.StaticMeshReductionOptions()
    lods.set_editor_property('auto_compute_lod_screen_size', False)
    steps = []
    for pct, screen in ((1.0, 1.0), (0.5, 0.2), (0.25, 0.07)):
        s = unreal.StaticMeshReductionSettings()
        s.set_editor_property('percent_triangles', pct)
        s.set_editor_property('screen_size', screen)
        steps.append(s)
    lods.set_editor_property('reduction_settings', steps)
    ok = 0
    for me in stage['meshes']:
        path = f"{BASE}/{me['name']}"
        mesh = unreal.load_asset(path)
        if not mesh:
            log(f'FAILED {path}')
            continue
        mesh.set_material(0, unreal.load_asset(f"{BASE}/Mat/{me['material']}"))
        n = sms.set_lods(mesh, lods)
        b = mesh.get_bounding_box()
        log(f"mesh {me['name']} lods {n} min {b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f} max {b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f}")
        eal.save_asset(path, only_if_is_dirty=False)
        ok += 1
    log(f'meshes {ok}/{len(stage["meshes"])}')
    # the runtime placement (tools/gen/props.mjs) needs the imported bounds in metres: write them into props.json
    data = stage.get('table') or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Content', 'Data', 'props.json')
    table = json.load(open(data))
    for pid, p in table['props'].items():
        mesh = unreal.load_asset(p['mesh'].split('.')[0])
        if mesh:
            b = mesh.get_bounding_box()
            p['min'] = [round(b.min.x / 100, 2), round(b.min.y / 100, 2), round(b.min.z / 100, 2)]
            p['max'] = [round(b.max.x / 100, 2), round(b.max.y / 100, 2), round(b.max.z / 100, 2)]
    json.dump(table, open(data, 'w'), indent=1)


import_textures()
make_instances(make_material(), make_foliage_material() if any('foliage' in me for me in stage['meshes']) else None)
import_meshes()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('HL_ASSETS_DONE')
