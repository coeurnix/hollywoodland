# Creates the only authored assets the game uses: flat gray-box materials in /Game/HL.
# Run: Scripts\MakeAssets.ps1 (UnrealEditor-Cmd -run=pythonscript). Idempotent.
#   M_HL    lit, BaseColor = Color, Emissive = Color * Glow (ambient term: the web tier has no sky light)
#   M_HLSky unlit, two-sided, Emissive = Color (sky dome, markers, selection ring)
import unreal

PATH = '/Game/HL'
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def make(name, unlit):
    full = f'{PATH}/{name}'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    m = tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 0)
    col.set_editor_property('parameter_name', 'Color')
    col.set_editor_property('default_value', unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    if unlit:
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        m.set_editor_property('two_sided', True)
        mel.connect_material_property(col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
        glow = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 300)
        glow.set_editor_property('parameter_name', 'Glow')
        glow.set_editor_property('default_value', 0.3)
        mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 200)
        mel.connect_material_expressions(col, '', mul, 'A')
        mel.connect_material_expressions(glow, '', mul, 'B')
        mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 400)
        rough.set_editor_property('r', 0.9)
        mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    ok = eal.save_asset(full, only_if_is_dirty=False)
    unreal.log(f'HL_ASSET {full} saved={ok}')


make('M_HL', False)
make('M_HLSky', True)
unreal.log('HL_ASSETS_DONE')
