# Retargets the engine template Mannequin's stock clips onto the Hollywoodland skeletons (both sexes): locomotion,
# jumping, death, unarmed attacks, pistol/rifle fire, hit react. The art package's own clips (melee swings, casting,
# sitting, emotes) import directly through import_characters.py.
# Needs Content/Characters/Mannequins (copied from Engine/../Templates/TemplateResources/High/Characters/Content/Mannequins;
# never cooked, see DefaultGame.ini). Run: Scripts\ImportCharacters.ps1 -Script retarget_anims.py
import os, shutil
import unreal

BASE = '/Game/Characters'
RIGS = f'{BASE}/Rigs'
MQ = f'{BASE}/Mannequins'
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# canonical clip name -> Mannequin source
CLIPS = {
    'Idle': 'Anims/Unarmed/MM_Idle',
    'Walk': 'Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd',
    'WalkBack': 'Anims/Unarmed/Walk/MF_Unarmed_Walk_Bwd',
    'Run': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd',
    'RunBack': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Bwd',
    'StrafeL': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Left',
    'StrafeR': 'Anims/Unarmed/Jog/MF_Unarmed_Jog_Right',
    'JumpStart': 'Anims/Unarmed/Jump/MM_Jump',
    'JumpLoop': 'Anims/Unarmed/Jump/MM_Fall_Loop',
    'JumpLand': 'Anims/Unarmed/Jump/MM_Land',
    'Death': 'Anims/Death/MM_Death_Front_01',
    'Death_B': 'Anims/Death/MM_Death_Back_01',
    'Attack_Unarmed': 'Anims/Unarmed/Attack/MM_Attack_01',
    'Attack_Unarmed_B': 'Anims/Unarmed/Attack/MM_Attack_02',
    'Attack_Unarmed_C': 'Anims/Unarmed/Attack/MM_Attack_03',
    'Shoot_Pistol': 'Anims/Pistol/MM_Pistol_Fire',
    'Combat_Ranged1H': 'Anims/Pistol/MF_Pistol_Idle_ADS',
    'Run_Pistol': 'Anims/Pistol/Jog/MF_Pistol_Jog_Fwd',
    'Shoot_Rifle': 'Anims/Rifle/MM_Rifle_Fire',
    'Combat_Rifle': 'Anims/Rifle/MF_Rifle_Idle_ADS',
    'Run_Rifle': 'Anims/Rifle/Jog/MF_Rifle_Jog_Fwd',
    'HitReact': 'Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01',
}
SOURCES = {'Male': 'Meshes/SKM_Manny_Simple', 'Female': 'Meshes/SKM_Quinn_Simple'}


def log(s):
    unreal.log('HL_ASSET ' + s)


def fresh(name, folder, cls, factory):
    full = f'{folder}/{name}'
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    return tools.create_asset(name, folder, cls, factory)


def ik_rig(name, mesh):
    rig = fresh(name, RIGS, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    c.apply_auto_generated_retarget_definition()
    chains = [str(x.get_editor_property('chain_name')) for x in c.get_retarget_chains()]
    log(f'{name}: root {c.get_retarget_root()} chains {chains}')
    eal.save_asset(f'{RIGS}/{name}', only_if_is_dirty=False)
    return rig


def main():
    if not eal.does_asset_exist(f'{MQ}/Meshes/SKM_Manny_Simple'):
        log('Mannequin content missing: copy Templates/TemplateResources/High/Characters/Content/Mannequins to Content/Characters/Mannequins')
        return
    for sex, src_path in SOURCES.items():
        src_mesh = unreal.load_asset(f'{MQ}/{src_path}')
        tgt_mesh = unreal.load_asset(f'{BASE}/Body/SK_{sex}_Default')
        src_rig = ik_rig(f'IK_Mannequin_{sex}', src_mesh)
        tgt_rig = ik_rig(f'IK_HL_{sex}', tgt_mesh)
        rtg = fresh(f'RTG_Mannequin_To_{sex}', RIGS, unreal.IKRetargeter, unreal.IKRetargetFactory())
        rc = unreal.IKRetargeterController.get_controller(rtg)
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
        rc.add_default_ops()
        rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
        rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
        rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        try:
            rc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
        except Exception as e:  # signature differs between versions
            log(f'auto_align_all_bones: {e}')
        eal.save_asset(f'{RIGS}/RTG_Mannequin_To_{sex}', only_if_is_dirty=False)
        assets = []
        for clip, path in CLIPS.items():
            ad = eal.find_asset_data(f'{MQ}/{path}')
            if ad.is_valid():
                assets.append(ad)
            else:
                log(f'missing source {path}')
        out_dir = f'{BASE}/Anim/Retarget_{sex}'
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(assets, src_mesh, tgt_mesh, rtg, '', '', f'RT_{sex}_', '', out_dir, False, False, True)
        log(f'{sex}: retargeted {len(res)} assets')
        # rename to canonical A_<Sex>_<Clip>
        for clip, path in CLIPS.items():
            src_name = path.split('/')[-1]
            have = f'{out_dir}/RT_{sex}_{src_name}'
            want = f'{BASE}/Anim/A_{sex}_{clip}'
            if not eal.does_asset_exist(have):
                log(f'no output for {clip} ({have})')
                continue
            if eal.does_asset_exist(want):
                eal.delete_asset(want)
            eal.rename_asset(have, want)
            seq = unreal.load_asset(want)
            if isinstance(seq, unreal.AnimSequence):
                seq.set_editor_property('enable_root_motion', False)
                seq.set_editor_property('force_root_lock', True)
            eal.save_asset(want, only_if_is_dirty=False)
        log(f'{sex}: clips done')
    eal.delete_directory(f'{BASE}/Anim/Retarget_Male')
    eal.delete_directory(f'{BASE}/Anim/Retarget_Female')


main()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('HL_ASSETS_DONE')
