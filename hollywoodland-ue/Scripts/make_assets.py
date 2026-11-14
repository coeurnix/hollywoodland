# Creates the authored materials the game uses in /Game/HL, and imports the painted world textures.
# Run: Scripts\MakeAssets.ps1 (UnrealEditor-Cmd -run=pythonscript). Idempotent.
#   M_HL         lit, BaseColor = Color, Emissive = Color * Glow (ambient term: the web tier has no sky light)
#   M_HLSky      unlit, two-sided, Emissive = Color (markers, selection ring, fallback sky)
#   M_HLGround   lit, BaseColor = Tex(world XY / Tile metres) * Tint, Emissive = BaseColor * Glow (ground surfaces)
#   M_HLSkyDome  unlit, two-sided, equirectangular Day/Night panoramas looked up by view direction, lerped by Night
# Textures: art/textures/T_GND_*.png (ground tiles, wrap) and T_SKY_*.png (no mips, so the u wrap leaves no seam line)
import os
import unreal

PATH = '/Game/HL'
TEX = f'{PATH}/Tex'
ART = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'art', 'textures')
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def fresh(name):
    full = f'{PATH}/{name}'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    return full, tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())


def ex(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin='', out=''):
    mel.connect_material_expressions(a, out, b, pin)


def save(full, m):
    mel.recompile_material(m)
    unreal.log(f'HL_ASSET {full} saved={eal.save_asset(full, only_if_is_dirty=False)}')


def make(name, unlit):
    full, m = fresh(name)
    col = ex(m, unreal.MaterialExpressionVectorParameter, -700, 0, parameter_name='Color', default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    if unlit:
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        m.set_editor_property('two_sided', True)
        mel.connect_material_property(col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
        glow = ex(m, unreal.MaterialExpressionScalarParameter, -700, 300, parameter_name='Glow', default_value=0.3)
        mul = ex(m, unreal.MaterialExpressionMultiply, -400, 200)
        link(col, mul, 'A'); link(glow, mul, 'B')
        mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        rough = ex(m, unreal.MaterialExpressionConstant, -400, 400, r=0.9)
        mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    save(full, m)


def import_textures():
    tasks = []
    for f in sorted(os.listdir(ART)):
        if not (f.startswith('T_GND_') or f.startswith('T_SKY_')) or not f.endswith('.png'):
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property('filename', os.path.join(ART, f))
        t.set_editor_property('destination_path', TEX)
        t.set_editor_property('replace_existing', True)
        t.set_editor_property('automated', True)
        t.set_editor_property('save', False)
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    for t in tasks:
        name = os.path.splitext(os.path.basename(t.get_editor_property('filename')))[0]
        tex = unreal.load_asset(f'{TEX}/{name}')
        if not tex:
            unreal.log_error(f'HL_ASSET texture import failed: {name}')
            continue
        if name.startswith('T_SKY_'):
            tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
        else:
            tex.set_editor_property('max_texture_size', 1024)
        tex.set_editor_property('srgb', True)
        unreal.log(f'HL_ASSET {TEX}/{name} saved={eal.save_asset(f"{TEX}/{name}", only_if_is_dirty=False)}')


def make_ground():
    full, m = fresh('M_HLGround')
    wp = ex(m, unreal.MaterialExpressionWorldPosition, -1400, 0)
    xy = ex(m, unreal.MaterialExpressionComponentMask, -1200, 0, r=True, g=True, b=False, a=False)
    link(wp, xy)
    tile = ex(m, unreal.MaterialExpressionScalarParameter, -1400, 150, parameter_name='Tile', default_value=4.0)
    cm = ex(m, unreal.MaterialExpressionMultiply, -1200, 150, const_b=100.0)
    link(tile, cm, 'A')
    uv = ex(m, unreal.MaterialExpressionDivide, -1000, 50)
    link(xy, uv, 'A'); link(cm, uv, 'B')
    tex = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, 0, parameter_name='Tex',
             texture=unreal.load_asset(f'{TEX}/T_GND_Lot'))
    link(uv, tex, 'UVs')
    tint = ex(m, unreal.MaterialExpressionVectorParameter, -800, 300, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    base = ex(m, unreal.MaterialExpressionMultiply, -500, 100)
    link(tex, base, 'A', 'RGB'); link(tint, base, 'B')
    mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = ex(m, unreal.MaterialExpressionScalarParameter, -500, 300, parameter_name='Glow', default_value=0.3)
    em = ex(m, unreal.MaterialExpressionMultiply, -300, 250)
    link(base, em, 'A'); link(glow, em, 'B')
    mel.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = ex(m, unreal.MaterialExpressionConstant, -300, 450, r=0.95)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    save(full, m)


def make_sky():
    full, m = fresh('M_HLSkyDome')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    # view direction from the camera: the sky sits at infinity wherever the player walks
    wp = ex(m, unreal.MaterialExpressionWorldPosition, -1800, 0)
    cam = ex(m, unreal.MaterialExpressionCameraPositionWS, -1800, 120)
    d = ex(m, unreal.MaterialExpressionSubtract, -1600, 50)
    link(wp, d, 'A'); link(cam, d, 'B')
    n = ex(m, unreal.MaterialExpressionNormalize, -1450, 50)
    link(d, n, 'VectorInput')
    dx = ex(m, unreal.MaterialExpressionComponentMask, -1300, -50, r=True, g=False, b=False, a=False)
    dy = ex(m, unreal.MaterialExpressionComponentMask, -1300, 50, r=False, g=True, b=False, a=False)
    dz = ex(m, unreal.MaterialExpressionComponentMask, -1300, 150, r=False, g=False, b=True, a=False)
    for c in (dx, dy, dz):
        link(n, c)
    at = ex(m, unreal.MaterialExpressionArctangent2, -1100, 0)
    link(dy, at, 'Y'); link(dx, at, 'X')
    u0 = ex(m, unreal.MaterialExpressionMultiply, -950, 0, const_b=0.15915494)  # 1 / 2pi
    link(at, u0, 'A')
    u = ex(m, unreal.MaterialExpressionAdd, -800, 0, const_b=0.5)
    link(u0, u, 'A')
    cz = ex(m, unreal.MaterialExpressionClamp, -1100, 150, min_default=-1.0, max_default=1.0)
    link(dz, cz, 'Input')
    ac = ex(m, unreal.MaterialExpressionArccosine, -950, 150)
    link(cz, ac, 'Input')
    v = ex(m, unreal.MaterialExpressionMultiply, -800, 150, const_b=0.31830989)  # 1 / pi: zenith 0, horizon 0.5
    link(ac, v, 'A')
    uv = ex(m, unreal.MaterialExpressionAppendVector, -650, 50)
    link(u, uv, 'A'); link(v, uv, 'B')
    day = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -450, -100, parameter_name='Day', texture=unreal.load_asset(f'{TEX}/T_SKY_Day'))
    night = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -450, 150, parameter_name='NightTex', texture=unreal.load_asset(f'{TEX}/T_SKY_Night'))
    link(uv, day, 'UVs'); link(uv, night, 'UVs')
    nt = ex(m, unreal.MaterialExpressionScalarParameter, -450, 350, parameter_name='Night', default_value=0.0)
    lp = ex(m, unreal.MaterialExpressionLinearInterpolate, -200, 50)
    link(day, lp, 'A', 'RGB'); link(night, lp, 'B', 'RGB'); link(nt, lp, 'Alpha')
    tint = ex(m, unreal.MaterialExpressionVectorParameter, -200, 300, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    out = ex(m, unreal.MaterialExpressionMultiply, 0, 100)
    link(lp, out, 'A'); link(tint, out, 'B')
    mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    save(full, m)


def make_add():
    # M_HLAdd: unlit additive glow for effects (bolts, tracers, rings, bubbles). Rim 0 = bright core fading to the
    # silhouette (orbs, tracers), Rim 1 = bright silhouette with a clear middle (bubbles, columns, rings).
    full, m = fresh('M_HLAdd')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided', True)
    col = ex(m, unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name='Color', default_value=unreal.LinearColor(1, 0.8, 0.4, 1))
    op = ex(m, unreal.MaterialExpressionScalarParameter, -900, 200, parameter_name='Opacity', default_value=1.0)
    rim = ex(m, unreal.MaterialExpressionScalarParameter, -900, 350, parameter_name='Rim', default_value=0.0)
    fr = ex(m, unreal.MaterialExpressionFresnel, -900, 500, exponent=2.0)
    inv = ex(m, unreal.MaterialExpressionOneMinus, -700, 450)
    link(fr, inv)
    lp = ex(m, unreal.MaterialExpressionLinearInterpolate, -500, 400)
    link(inv, lp, 'A'); link(fr, lp, 'B'); link(rim, lp, 'Alpha')
    a = ex(m, unreal.MaterialExpressionMultiply, -500, 100)
    link(col, a, 'A'); link(op, a, 'B')
    b = ex(m, unreal.MaterialExpressionMultiply, -300, 200)
    link(a, b, 'A'); link(lp, b, 'B')
    mel.connect_material_property(b, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    save(full, m)


make('M_HL', False)
make('M_HLSky', True)
make_add()
import_textures()
make_ground()
make_sky()
unreal.log('HL_ASSETS_DONE')
