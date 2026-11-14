# Camera see-through for foliage (M_HLFoliage): trees between the camera and the player dither away in a cylinder
# around the line of sight, so a canopy never hides the player. The game puts the player's chest position (world cm)
# in MPC_HLAmbient's Focus every frame (AHLWorldActor::TickSky); with Focus unset (0,0,0) nothing fades.
#   add_camera_fade(m, alpha_expr, alpha_out)  multiplies the material's opacity mask by the fade
# Run on its own (UnrealEditor-Cmd -run=pythonscript -script=camera_fade.py) to patch the existing M_HLFoliage.
import unreal

MPC = '/Game/HL/MPC_HLAmbient'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

CODE = (
    'float3 d = Focus - Cam; float L = length(d);'
    'if (L < 1.0) return 1.0;'
    'float3 dir = d / L; float t = dot(P - Cam, dir);'
    'if (t < 0.0 || t > L - 120.0) return 1.0;'
    'float r = length(P - (Cam + dir * t));'
    'float k = lerp(0.12, 1.0, saturate((r - 250.0) / 250.0));'
    'float ign = frac(52.9829189 * frac(dot(SP, float2(0.06711056, 0.00583715))));'
    'return step(ign, k);'
)


def focus_param():
    c = eal.load_asset(MPC)
    params = list(c.get_editor_property('vector_parameters'))
    if not any(str(p.get_editor_property('parameter_name')) == 'Focus' for p in params):
        p = unreal.CollectionVectorParameter()
        p.set_editor_property('parameter_name', 'Focus')
        p.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
        params.append(p)
        c.set_editor_property('vector_parameters', params)
        unreal.log(f'HL_ASSET {MPC} Focus added saved={eal.save_asset(MPC, only_if_is_dirty=False)}')
    return c


def add_camera_fade(m, alpha_expr, alpha_out='A'):
    mpc = focus_param()
    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -700, 700)
    cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    cu.set_editor_property('code', CODE)
    cu.set_editor_property('description', 'HL_CAMFADE')
    ins = []
    for n in ('P', 'Cam', 'Focus', 'SP'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    cu.set_editor_property('inputs', ins)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1000, 650)
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, -1000, 720)
    foc = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -1000, 790)
    foc.set_editor_property('collection', mpc)
    foc.set_editor_property('parameter_name', 'Focus')
    sp = mel.create_material_expression(m, unreal.MaterialExpressionScreenPosition, -1000, 860)
    mel.connect_material_expressions(wp, '', cu, 'P')
    mel.connect_material_expressions(cam, '', cu, 'Cam')
    mel.connect_material_expressions(foc, '', cu, 'Focus')
    mel.connect_material_expressions(sp, 'PixelPosition', cu, 'SP')
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 600)
    mel.connect_material_expressions(alpha_expr, alpha_out, mul, 'A')
    mel.connect_material_expressions(cu, '', mul, 'B')
    mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_OPACITY_MASK)


def patch_foliage():
    path = '/Game/Props/M_HLFoliage'
    m = eal.load_asset(path)
    cur = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_OPACITY_MASK)
    if cur is not None and cur.get_class().get_name() == 'MaterialExpressionMultiply':
        unreal.log(f'HL_ASSET {path} already has the camera fade')
        return
    add_camera_fade(m, cur, 'A')
    mel.recompile_material(m)
    unreal.log(f'HL_ASSET {path} camera fade saved={eal.save_asset(path, only_if_is_dirty=False)}')


if __name__ == '__main__':
    patch_foliage()
