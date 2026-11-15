# Halves every prop normal map (the web package budget: itch allows 500 MiB). import_props.py does the same on import.
# Run: Scripts\ImportCharacters.ps1 -Script cap_prop_normals.py
import unreal

eal = unreal.EditorAssetLibrary
n = 0
for p in eal.list_assets('/Game/Props/Tex', recursive=True):
    path = p.split('.')[0]
    if not path.endswith('_N'):
        continue
    tex = unreal.load_asset(path)
    if not isinstance(tex, unreal.Texture2D):
        continue
    size = max(tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    cap = max(256, size // 2)
    if tex.get_editor_property('max_texture_size') != cap:
        tex.set_editor_property('max_texture_size', cap)
        eal.save_asset(path, only_if_is_dirty=False)
        n += 1
unreal.log(f'HL_ASSET prop normal maps capped: {n}')
unreal.log('HL_ASSETS_DONE')
