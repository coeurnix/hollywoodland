# Sets the skeletal-mesh usage flag on materials applied to skeletal meshes at run time (a cooked build without the flag
# draws the default material instead). Run: Scripts\ImportCharacters.ps1 -Script fix_usage.py
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
for path in ['/Game/HL/M_HLAdd']:
    m = eal.load_asset(path)
    if not m:
        unreal.log('HL_ASSET missing ' + path)
        continue
    m.set_editor_property('used_with_skeletal_mesh', True)
    mel.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    unreal.log(f'HL_ASSET {path} usage skeletal={m.get_editor_property("used_with_skeletal_mesh")}')
unreal.log('HL_ASSETS_DONE')
