# One-off fix for already-imported clips (retarget_anims.py now does this on import): the retargeted mannequin fire and
# hit-react clips came in additive, which a single-node player applies to the bind pose (a skewed A-pose after shots).
import unreal
eal = unreal.EditorAssetLibrary
for p in eal.list_assets('/Game/Characters/Anim', recursive=False):
    a = unreal.load_asset(p.split('.')[0])
    if isinstance(a, unreal.AnimSequence) and a.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
        a.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_NONE)
        unreal.log(f'HL_ASSET {a.get_name()} additive off saved={eal.save_asset(a.get_path_name(), only_if_is_dirty=False)}')
