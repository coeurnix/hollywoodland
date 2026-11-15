# Points M_HLCharLayer's texture parameters at an engine texture instead of a v1 look mask, so the v1 look textures
# (/Game/Characters/Tex/Looks) can stay out of the web cook now that v2 dresses everyone (the layer material is only
# used with real textures set at run time). Run: Scripts\ImportCharacters.ps1 -Script unhook_v1_looks.py
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
path = '/Game/Characters/M_HLCharLayer'
m = eal.load_asset(path)
white = eal.load_asset('/Engine/EngineResources/WhiteSquareTexture')
n = 0
for e in unreal.MaterialEditingLibrary.get_material_expressions(m) if hasattr(mel, 'get_material_expressions') else []:
    if isinstance(e, unreal.MaterialExpressionTextureBase):
        t = e.get_editor_property('texture')
        if t and '/Game/Characters/Tex/Looks/' in t.get_path_name():
            # keep the sampler type valid: a mask sampler gets a linear engine texture
            e.set_editor_property('texture', white)
            n += 1
mel.recompile_material(m)
eal.save_asset(path, only_if_is_dirty=False)
unreal.log(f'HL_ASSET {path}: {n} texture defaults moved off the v1 looks')
unreal.log('HL_ASSETS_DONE')
