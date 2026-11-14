# Hemisphere ambient for the lit master materials (run in the editor commandlet: Scripts/PatchAmbient.ps1, and at the end
# of the import scripts that rebuild a master material).
#
# The web tier has no sky light, so every master material carries a flat ambient term: Emissive = BaseColor * Glow.
# This multiplies that term by a hemisphere tint read from /Game/HL/MPC_HLAmbient:
#     Emissive = BaseColor * Glow * lerp(GroundAmb, SkyAmb, saturate(NormalWS.z * 0.5 + 0.5))
# so faces turned to the sky get a cool fill and undersides a warmer, darker bounce. The game sets SkyAmb / GroundAmb by
# time of day (AHLWorldActor::TickSky). Defaults are white, so a material read without the game looks as before.
# Idempotent: a material that already has the collection node is left alone.
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MPC = '/Game/HL/MPC_HLAmbient'
MATERIALS = ['/Game/HL/M_HL', '/Game/HL/M_HLGround', '/Game/Characters/M_HLChar', '/Game/Characters/M_HLHair',
             '/Game/Characters/M_HLProp', '/Game/Enemies/M_HLEnemy', '/Game/Props/M_HLWorldProp', '/Game/Buildings/M_HLWorldProp', '/Game/Props/M_HLFoliage']


def collection():
    if eal.does_asset_exist(MPC):
        return eal.load_asset(MPC)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    c = tools.create_asset('MPC_HLAmbient', '/Game/HL', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name in ('SkyAmb', 'GroundAmb'):
        p = unreal.CollectionVectorParameter()
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
        params.append(p)
    c.set_editor_property('vector_parameters', params)
    unreal.log(f'HL_ASSET {MPC} saved={eal.save_asset(MPC, only_if_is_dirty=False)}')
    return c


def ex(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def patch(path, mpc):
    if not eal.does_asset_exist(path):
        unreal.log_warning(f'HL_AMBIENT missing {path}')
        return
    m = eal.load_asset(path)
    em = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if em is None:
        unreal.log_warning(f'HL_AMBIENT {path} has no emissive input')
        return
    if em.get_editor_property('desc') == 'HL_HEMI':
        unreal.log(f'HL_AMBIENT {path} already patched')
        return
    x, y = -300, 500
    sky = ex(m, unreal.MaterialExpressionCollectionParameter, x - 700, y, collection=mpc, parameter_name='SkyAmb')
    gnd = ex(m, unreal.MaterialExpressionCollectionParameter, x - 700, y + 120, collection=mpc, parameter_name='GroundAmb')
    nrm = ex(m, unreal.MaterialExpressionVertexNormalWS, x - 900, y + 260)
    nz = ex(m, unreal.MaterialExpressionComponentMask, x - 750, y + 260, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(nrm, '', nz, '')
    half = ex(m, unreal.MaterialExpressionMultiply, x - 620, y + 260, const_b=0.5)
    mel.connect_material_expressions(nz, '', half, 'A')
    add = ex(m, unreal.MaterialExpressionAdd, x - 500, y + 260, const_b=0.5)
    mel.connect_material_expressions(half, '', add, 'A')
    sat = ex(m, unreal.MaterialExpressionSaturate, x - 380, y + 260)
    mel.connect_material_expressions(add, '', sat, '')
    lerp = ex(m, unreal.MaterialExpressionLinearInterpolate, x - 250, y + 80)
    mel.connect_material_expressions(gnd, '', lerp, 'A')
    mel.connect_material_expressions(sky, '', lerp, 'B')
    mel.connect_material_expressions(sat, '', lerp, 'Alpha')
    rgb = ex(m, unreal.MaterialExpressionComponentMask, x - 120, y + 80, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(lerp, '', rgb, '')
    out = ex(m, unreal.MaterialExpressionMultiply, x, y, desc='HL_HEMI')
    mel.connect_material_expressions(em, '', out, 'A')
    mel.connect_material_expressions(rgb, '', out, 'B')
    mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    unreal.log(f'HL_AMBIENT {path} patched saved={eal.save_asset(path, only_if_is_dirty=False)}')


def main(paths=None):
    mpc = collection()
    for p in paths or MATERIALS:
        patch(p, mpc)


if __name__ == '__main__':
    main()
