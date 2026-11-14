# Creates the authored materials the game uses in /Game/HL, and imports the painted world textures.
# Run: Scripts\MakeAssets.ps1 (UnrealEditor-Cmd -run=pythonscript). Idempotent.
#   M_HL         lit, BaseColor = Color, Emissive = Color * Glow (ambient term: the web tier has no sky light)
#   M_HLSky      unlit, two-sided, Emissive = Color (markers, selection ring, fallback sky)
#   M_HLGround   lit, BaseColor = Tex(world XY / Tile metres) * Tint, Emissive = BaseColor * Glow (ground surfaces)
#   M_HLWall     lit, box-mapped world UVs: sides sample Tex, up faces sample Top (Tile metres), * Tint, Emissive * Glow
#   M_HLTerrain  lit, the outdoor floor: meadow (Tex) / grass (Tex2) / worn dirt / road / blight blended by a per-area mask
#                (R road, G grass, B dirt, A blight) that AHLWorldActor paints at load time; soft road edges for free
#   M_HLSkyDome    unlit, two-sided, equirectangular Day/Night panoramas looked up by view direction, lerped by Night
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


def make_wall():
    # plain boxes (shop sides and backs, low walls): a tileable facade texture on the sides, roofing on top, both in
    # world metres so the brick courses line up across neighbours whatever the box's size
    full, m = fresh('M_HLWall')
    wp = ex(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    nrm = ex(m, unreal.MaterialExpressionVertexNormalWS, -1600, 150)
    tile = ex(m, unreal.MaterialExpressionScalarParameter, -1600, 300, parameter_name='Tile', default_value=3.0)
    cu = ex(m, unreal.MaterialExpressionCustom, -1300, 100, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,
            code='float3 a = abs(N); float2 uv = a.z > 0.5 ? P.xy : (a.x > a.y ? float2(P.y, -P.z) : float2(P.x, -P.z)); return uv / (Tile * 100.0);')
    ins = []
    for n in ('P', 'N', 'Tile'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    cu.set_editor_property('inputs', ins)
    link(wp, cu, 'P'); link(nrm, cu, 'N'); link(tile, cu, 'Tile')
    side = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -100, parameter_name='Tex',
              texture=unreal.load_asset('/Game/Buildings/Tex/T_TILE_Stucco_Cream'))
    top = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -1000, 200, parameter_name='Top',
             texture=unreal.load_asset('/Game/Buildings/Tex/T_TILE_Roof_TarGravel'))
    link(cu, side, 'UVs'); link(cu, top, 'UVs')
    nz = ex(m, unreal.MaterialExpressionComponentMask, -1000, 450, r=False, g=False, b=True, a=False)
    link(nrm, nz)
    up = ex(m, unreal.MaterialExpressionStep, -800, 450, const_y=0.5)
    link(nz, up, 'X')
    lp = ex(m, unreal.MaterialExpressionLinearInterpolate, -700, 100)
    link(side, lp, 'A', 'RGB'); link(top, lp, 'B', 'RGB'); link(up, lp, 'Alpha')
    tint = ex(m, unreal.MaterialExpressionVectorParameter, -700, 350, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    # the wall's own architecture, from its height above its base: a dark plinth, rising damp fading up the first
    # few metres, and a pale coping band with a shadow line under it at the parapet (sides only; roofs untouched)
    op = ex(m, unreal.MaterialExpressionObjectPositionWS, -1600, 600)
    ob = ex(m, unreal.MaterialExpressionObjectBounds, -1600, 700)
    sh = ex(m, unreal.MaterialExpressionCustom, -1000, 650, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
            code=('float h = P.z - (OP.z - OB.z * 0.5); float top = OB.z - h; '
                  'float s = lerp(0.6, 1.0, saturate((h - 45.0) * 0.2)) * lerp(0.78, 1.0, saturate(h / 380.0)); '
                  's *= top < 30.0 ? 1.14 : (top < 46.0 ? 0.72 : 1.0); '
                  'return lerp(1.0, s, step(abs(N.z), 0.5));'))
    ins2 = []
    for n in ('P', 'N', 'OP', 'OB'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins2.append(ci)
    sh.set_editor_property('inputs', ins2)
    link(wp, sh, 'P'); link(nrm, sh, 'N'); link(op, sh, 'OP'); link(ob, sh, 'OB')
    shaded = ex(m, unreal.MaterialExpressionMultiply, -600, 250)
    link(lp, shaded, 'A'); link(sh, shaded, 'B')
    base = ex(m, unreal.MaterialExpressionMultiply, -500, 150)
    link(shaded, base, 'A'); link(tint, base, 'B')
    mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = ex(m, unreal.MaterialExpressionScalarParameter, -500, 350, parameter_name='Glow', default_value=0.22)
    em = ex(m, unreal.MaterialExpressionMultiply, -300, 300)
    link(base, em, 'A'); link(glow, em, 'B')
    mel.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = ex(m, unreal.MaterialExpressionConstant, -300, 500, r=0.9)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    save(full, m)


def mask_placeholder():
    # a 4x4 linear (non-sRGB) black texture: the default for M_HLTerrain's Mask, which the game swaps for its own
    import struct, zlib, tempfile
    raw = b''.join(b'\x00' + b'\x00\x00\x00\x00' * 4 for _ in range(4))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    path = os.path.join(tempfile.gettempdir(), 'T_HLMaskDefault.png')
    if eal.does_asset_exist(f'{TEX}/T_HLMaskDefault'):
        eal.delete_asset(f'{TEX}/T_HLMaskDefault')
    open(path, 'wb').write(png)
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', path)
    t.set_editor_property('destination_path', TEX)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(f'{TEX}/T_HLMaskDefault')
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.log(f'HL_ASSET {TEX}/T_HLMaskDefault saved={eal.save_asset(f"{TEX}/T_HLMaskDefault", only_if_is_dirty=False)}')
    return tex


def make_terrain():
    full, m = fresh('M_HLTerrain')
    wp = ex(m, unreal.MaterialExpressionWorldPosition, -2400, 0)
    xy = ex(m, unreal.MaterialExpressionComponentMask, -2200, 0, r=True, g=True, b=False, a=False)
    link(wp, xy)

    def tiled(name, tex, default, y, tint):
        t = ex(m, unreal.MaterialExpressionScalarParameter, -2200, y + 100, parameter_name=f'{name}Tile', default_value=default)
        cm = ex(m, unreal.MaterialExpressionMultiply, -2000, y + 100, const_b=100.0)
        link(t, cm, 'A')
        uv = ex(m, unreal.MaterialExpressionDivide, -1850, y)
        link(xy, uv, 'A'); link(cm, uv, 'B')
        s = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -1650, y, parameter_name=name, texture=unreal.load_asset(f'{TEX}/{tex}'))
        link(uv, s, 'UVs')
        c = ex(m, unreal.MaterialExpressionVectorParameter, -1650, y + 200, parameter_name=f'{name}Tint', default_value=unreal.LinearColor(*tint, 1))
        mul = ex(m, unreal.MaterialExpressionMultiply, -1400, y + 50)
        link(s, mul, 'A', 'RGB'); link(c, mul, 'B')
        return s, mul

    lot_s, lot = tiled('Tex', 'T_GND_Lot', 9.0, -800, (0.6, 0.56, 0.47))
    _, grass = tiled('Tex2', 'T_GND_Grass', 6.0, -400, (0.56, 0.64, 0.46))
    _, road = tiled('Road', 'T_GND_Asphalt', 7.0, 0, (0.8, 0.8, 0.82))
    _, blight = tiled('Blight', 'T_GND_Blight', 10.0, 400, (0.9, 0.85, 0.9))
    # worn dirt: the meadow texture's own grain, desaturated to its luminance and tinted earth brown
    lum = ex(m, unreal.MaterialExpressionDesaturation, -1400, 800)
    link(lot_s, lum, 'Input', 'RGB')
    dt = ex(m, unreal.MaterialExpressionVectorParameter, -1400, 950, parameter_name='DirtTint', default_value=unreal.LinearColor(0.62, 0.5, 0.36, 1))
    dirt = ex(m, unreal.MaterialExpressionMultiply, -1200, 850)
    link(lum, dirt, 'A'); link(dt, dirt, 'B')
    # the mask: area-local UVs from MaskLo (x, y = world cm of the area's min corner) and MaskSize (x, y = its size in cm)
    # (a vector parameter's main output is RGB only, so the size is a second parameter)
    lo = ex(m, unreal.MaterialExpressionVectorParameter, -2400, 1200, parameter_name='MaskLo', default_value=unreal.LinearColor(0, 0, 0, 1))
    sz = ex(m, unreal.MaterialExpressionVectorParameter, -2400, 1400, parameter_name='MaskSize', default_value=unreal.LinearColor(100000, 100000, 1, 1))
    lo_xy = ex(m, unreal.MaterialExpressionComponentMask, -2200, 1200, r=True, g=True, b=False, a=False)
    lo_zw = ex(m, unreal.MaterialExpressionComponentMask, -2200, 1350, r=True, g=True, b=False, a=False)
    link(lo, lo_xy); link(sz, lo_zw)
    rel = ex(m, unreal.MaterialExpressionSubtract, -2000, 1200)
    link(xy, rel, 'A'); link(lo_xy, rel, 'B')
    muv = ex(m, unreal.MaterialExpressionDivide, -1850, 1250)
    link(rel, muv, 'A'); link(lo_zw, muv, 'B')
    mk = ex(m, unreal.MaterialExpressionTextureSampleParameter2D, -1650, 1200, parameter_name='Mask', texture=mask_placeholder(),
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(muv, mk, 'UVs')
    l1 = ex(m, unreal.MaterialExpressionLinearInterpolate, -1000, -600)
    link(lot, l1, 'A'); link(grass, l1, 'B'); link(mk, l1, 'Alpha', 'G')
    l2 = ex(m, unreal.MaterialExpressionLinearInterpolate, -800, -300)
    link(l1, l2, 'A'); link(dirt, l2, 'B'); link(mk, l2, 'Alpha', 'B')
    l3 = ex(m, unreal.MaterialExpressionLinearInterpolate, -600, 0)
    link(l2, l3, 'A'); link(blight, l3, 'B'); link(mk, l3, 'Alpha', 'A')
    l4 = ex(m, unreal.MaterialExpressionLinearInterpolate, -400, 200)
    link(l3, l4, 'A'); link(road, l4, 'B'); link(mk, l4, 'Alpha', 'R')
    mel.connect_material_property(l4, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = ex(m, unreal.MaterialExpressionScalarParameter, -400, 450, parameter_name='Glow', default_value=0.14)
    em = ex(m, unreal.MaterialExpressionMultiply, -200, 350)
    link(l4, em, 'A'); link(glow, em, 'B')
    mel.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = ex(m, unreal.MaterialExpressionConstant, -200, 550, r=0.95)
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
make_wall()
make_terrain()
make_sky()
unreal.log('HL_ASSETS_DONE')
