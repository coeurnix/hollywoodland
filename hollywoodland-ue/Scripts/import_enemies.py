# Imports the enemy package (art/enemies, see its README) into /Game/Enemies. Run: Scripts\ImportEnemies.ps1.
#   M_HLEnemy           lit skinned master: BaseColor = B * Tint, Normal = N (glTF +Y: green flipped on import),
#                       roughness/metal from MR (glTF G/B), Emissive = BaseColor * Glow (same ambient term as M_HL)
#   <Model>/SK_<Model>  skeletal mesh + skeleton, every take as A_<Model>_<Clip>, LOD1 50% / LOD2 25%
#   <Model>/MI_<Model>[_<Variant>]   instances per base-colour variant
#   HiveStructures/SM_* destroyed / burned hive pieces (static)
# Textures: base colour 1024, normal and MR 512 (enemies are seen from a third-person camera; keeps the web package small).
import json, os, re
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'art', 'enemies')
BASE = '/Game/Enemies'
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
with open(os.path.join(SRC, "enemies.json"), encoding="utf-8") as _f:
    man = json.load(_f)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG false')
ONLY = [s for s in os.environ.get('HL_ENEMIES', '').split(',') if s]


def log(s):
    unreal.log('HL_ASSET ' + s)


def task(file, dest, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', file)
    t.set_editor_property('destination_path', dest)
    t.set_editor_property('destination_name', name)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    if options:
        t.set_editor_property('options', options)
    return t


def import_textures(folder, dest):
    tasks = []
    for f in sorted(os.listdir(folder)):
        if f.endswith('.png'):
            tasks.append(task(os.path.join(folder, f), dest, f[:-4]))
    tools.import_asset_tasks(tasks)
    for t in tasks:
        name = t.get_editor_property('destination_name')
        path = f'{dest}/{name}'
        tex = unreal.load_asset(path)
        if not tex:
            log(f'FAILED {path}')
            continue
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        if name.endswith('_Normal'):
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('flip_green_channel', True)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('max_texture_size', 512)
        elif name.endswith('_MetallicRoughness'):
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property('max_texture_size', 512)
        else:
            tex.set_editor_property('max_texture_size', 1024)
        eal.save_asset(path, only_if_is_dirty=False)
    return [t.get_editor_property('destination_name') for t in tasks]


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


def make_material(defaults):
    full = f'{BASE}/M_HLEnemy'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    m = tools.create_asset('M_HLEnemy', BASE, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_skeletal_mesh', True)
    m.set_editor_property('used_with_static_lighting', False)
    b = tex_param(m, 'B', -1100, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, defaults['BaseColor'])
    n = tex_param(m, 'N', -1100, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, defaults['Normal'])
    mr = tex_param(m, 'MR', -1100, 600, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, defaults['MetallicRoughness'])
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1100, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    col = op(m, unreal.MaterialExpressionMultiply, -800, 0, b, tint, 'RGB')
    mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(op(m, unreal.MaterialExpressionMultiply, -500, 150, col, scalar(m, 'Glow', -800, 200, 0.22)), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(mr, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    # metallic damped: with no reflection captures or sky light, metal reads black on the web tier
    mel.connect_material_property(op(m, unreal.MaterialExpressionMultiply, -500, 700, mr, scalar(m, 'MetalScale', -800, 800, 0.3), 'B'), '', unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_asset(full, only_if_is_dirty=False)
    log(f'{full} saved')
    return m


def instance(parent, dest, name, b, n, mr):
    path = f'{dest}/{name}'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (('B', b), ('N', n), ('MR', mr)):
        mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(f'{dest}/{v}'))
    eal.save_asset(path, only_if_is_dirty=False)
    return path


def skel_options():
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True)
    o.set_editor_property('import_as_skeletal', True)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property('import_animations', True)
    o.set_editor_property('import_materials', False)
    o.set_editor_property('import_textures', False)
    o.set_editor_property('create_physics_asset', False)
    d = o.get_editor_property('skeletal_mesh_import_data')
    d.set_editor_property('import_morph_targets', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('use_t0_as_ref_pose', False)
    d.set_editor_property('import_uniform_scale', 100.0)
    d.set_editor_property('import_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    a = o.get_editor_property('anim_sequence_import_data')
    a.set_editor_property('import_bone_tracks', True)
    a.set_editor_property('remove_redundant_keys', False)
    a.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    a.set_editor_property('import_uniform_scale', 100.0)
    a.set_editor_property('import_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    return o


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
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('import_uniform_scale', 100.0)
    d.set_editor_property('import_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    return o


def lods(mesh):
    try:
        opts = unreal.SkeletalMeshEditorSubsystem if hasattr(unreal, 'SkeletalMeshEditorSubsystem') else None
        sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem) if opts else None
        if sub and hasattr(sub, 'regenerate_lod'):
            sub.regenerate_lod(mesh, 3, False, False)
    except Exception as e:  # noqa: BLE001 - LODs are an optimisation; carry on without them
        log(f'lods skipped: {e}')


def rename_takes(dest, model):
    # the importer names takes "<Mesh>_Anim_<Take>" (or just "<Take>"); the game wants A_<Model>_<Clip>
    n = 0
    for p in eal.list_assets(dest, recursive=False):
        name = p.split('/')[-1].split('.')[0]
        m = re.search(r'(A_' + re.escape(model) + r'_[A-Za-z0-9]+)$', name)
        if not m or name == m.group(1):
            continue
        new = f'{dest}/{m.group(1)}'
        if eal.does_asset_exist(new):
            eal.delete_asset(new)
        if eal.rename_asset(p.split('.')[0], new):
            n += 1
    return n


def main():
    models = [k for k in man['models'] if not ONLY or k in ONLY]
    defaults = {}
    for k in models:
        dest = f'{BASE}/{k}'

        names = import_textures(os.path.join(SRC, k, 'textures'), dest)
        for x in names:
            for suf in ('BaseColor', 'Normal', 'MetallicRoughness'):
                if x == f'T_{k}_{suf}':
                    defaults.setdefault(suf, f'{dest}/{x}')
    parent = make_material(defaults)
    ok = 0
    for k in models:
        info = man['models'][k]
        dest = f'{BASE}/{k}'
        b, n, mr = f'T_{k}_BaseColor', f'T_{k}_Normal', f'T_{k}_MetallicRoughness'
        instance(parent, dest, f'MI_{k}', b, n, mr)
        for f in os.listdir(os.path.join(SRC, k, 'textures')):
            v = re.match(r'T_' + re.escape(k) + r'_(\w+)_BaseColor\.png$', f)
            if v:
                instance(parent, dest, f'MI_{k}_{v.group(1)}', f[:-4], n, mr)
        tools.import_asset_tasks([task(os.path.join(SRC, info['fbx'].replace('/', os.sep)), dest, f'SK_{k}', skel_options())])
        mesh = unreal.load_asset(f'{dest}/SK_{k}')
        if not mesh:
            log(f'FAILED mesh {k}')
            continue
        mesh.set_material(0, unreal.load_asset(f'{dest}/MI_{k}')) if hasattr(mesh, 'set_material') else None
        lods(mesh)
        r = rename_takes(dest, k)
        have = [c for c, v in info['clips'].items() if eal.does_asset_exist(f"{dest}/{v['anim']}")]
        b0 = mesh.get_bounds()
        log(f"mesh SK_{k} clips {len(have)}/{len(info['clips'])} renamed {r} bounds origin {b0.origin} extent {b0.box_extent}")
        ok += 1
    # destroyed hive structures
    hs = os.path.join(SRC, 'HiveStructures')
    if os.path.isdir(hs) and not ONLY:
        dest = f'{BASE}/HiveStructures'
        import_textures(os.path.join(hs, 'textures'), dest)
        for f in sorted(os.listdir(hs)):
            if not f.endswith('.fbx'):
                continue
            name = f[:-4]
            short = name[3:]
            tools.import_asset_tasks([task(os.path.join(hs, f), dest, name, static_options())])
            mi = instance(parent, dest, f'MI_{short}', f'T_{short}_BaseColor', f'T_{short}_Normal', f'T_{short}_MetallicRoughness')
            sm = unreal.load_asset(f'{dest}/{name}')
            if sm:
                sm.set_material(0, unreal.load_asset(mi))
                log(f'static {name}')
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f'models {ok}/{len(models)}')


main()
unreal.log('HL_ASSETS_DONE')
