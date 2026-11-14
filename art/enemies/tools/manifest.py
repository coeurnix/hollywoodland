# Writes art/enemies/enemies.json: every mob in Content/Data/mobs.json -> model, texture variant, clips and event frames.
# Run from art/enemies: python tools/manifest.py
import json, glob, os
mobs = {m['id']: m for m in json.load(open('../../hollywoodland-ue/Content/Data/mobs.json'))['mobs']}
rigs = {os.path.basename(os.path.dirname(f)): json.load(open(f)) for f in glob.glob('*/*.rig.json')}
FPS = 30
INSECT = dict(idle='Idle', walk='Walk', run='Run', attack='Attack', hit='Hit', death='Death')
QUAD = dict(idle='QIdle', walk='QWalk', run='QRun', attack='Bite', hit='QHit', death='QDeath')
FLIER = dict(idle='FlyIdle', walk='FlyForward', run='FlyForward', attack='Sting', hit='Hit', death='FlierDeath')
SAC = dict(idle='Pulse', hit='SacHit')
# gameplay moments inside a clip, as frame numbers at 30 fps counted from 0
EVENTS = {'Attack': {'hit': 12}, 'Bite': {'hit': 10}, 'Sting': {'hit': 11}, 'Spit': {'release': 14}, 'SpitPool': {'pool': 26},
          'Slam': {'impact': 20}, 'Dive': {'strike': 22}, 'Emerge': {'ambush_hit': 16}, 'Leap': {'land': 12},
          'Death': {'corpse': 'last frame'}, 'QDeath': {'corpse': 'last frame'}, 'FlierDeath': {'corpse': 'last frame'}}
ABILITY = {'ab_mob_acid_spit': 'Spit', 'ab_mob_acid_pool': 'SpitPool', 'ab_mob_knockback': 'Slam', 'ab_mob_charge': 'Charge'}
HUMAN = 'human: drawn by the existing character system (FHLChars looks); no mesh in art/enemies'
MAP = {
 'mob_invader_worker': ('InvaderWorker', None), 'mob_feral_dog': ('FeralDog', None), 'mob_coyote': ('Coyote', None),
 'mob_invader_drone': ('InvaderDrone', None), 'mob_invader_drone_hs': ('InvaderDrone', None), 'mob_egy_drone': ('InvaderDrone', None),
 'mob_invader_spitter': ('InvaderSpitter', None), 'mob_egy_spitter': ('InvaderSpitter', None),
 'mob_invader_broodling': ('InvaderBroodling', None), 'mob_egy_broodling': ('InvaderBroodling', None),
 'mob_invader_soldier': ('InvaderSoldier', None), 'mob_egy_soldier': ('InvaderSoldier', None),
 'mob_guard_cahuenga': ('InvaderSoldier', 'Hardened'), 'mob_pass_soldier': ('InvaderSoldier', 'Hardened'),
 'mob_wall_stalker': ('InvaderSoldier', 'WallStalker'),
 'mob_old_scar': ('OldScar', None), 'mob_selma_burrower': ('InvaderBurrower', None),
 'mob_ballroom_brute': ('InvaderBrute', None), 'mob_guard_gower': ('InvaderBrute', None), 'mob_guard_melrose': ('InvaderBrute', None),
 'mob_melrose_brute': ('InvaderBrute', None), 'mob_pass_flier': ('InvaderFlier', None), 'mob_gulch_coyote': ('HiveRiddenCoyote', None),
 'mob_boss_brood_keeper': ('BroodKeeper', None), 'mob_boss_hive_warden': ('HiveWarden', None),
 'mob_boss_matriarch': ('SpitterMatriarch', None), 'mob_boss_hive_mother': ('HiveMother', None),
 'mob_looter': (None, None), 'mob_looter_ringleader': (None, None), 'mob_guard_la_brea': (None, None), 'mob_wh_gunman': (None, None),
}
missing = set(mobs) - set(MAP)
if missing: raise SystemExit(f'mobs.json has mobs with no model mapping: {sorted(missing)}')
models = {}
for name, r in sorted(rigs.items()):
    roles = {'quad': QUAD, 'flier': FLIER, 'insect': INSECT, 'sac': SAC}[r['kind']]
    clips = {c: dict(anim=f'A_{name}_{c}', frames=v['frames'] + 1, seconds=round(v['frames'] / FPS, 3), loop=v['loop'],
                     **({'events': EVENTS[c]} if c in EVENTS else {})) for c, v in r['clips'].items()}
    models[name] = dict(kind=r['kind'], fbx=f'{name}/SK_{name}.fbx', glb=f'{name}/SK_{name}.glb',
                        textures={k: f'{name}/textures/T_{name}_{k}.png' for k in ('BaseColor', 'Normal', 'MetallicRoughness')},
                        size_m_at_scale_1=r['size_m'], triangles=r['tris'], bones=r['bones'],
                        roles={k: v for k, v in roles.items() if v in r['clips']}, clips=clips)
out_mobs = {}
for mid in sorted(MAP):
    model, variant = MAP[mid]; m = mobs.get(mid)
    if m is None: continue
    if model is None:
        out_mobs[mid] = dict(name=m['name'], rank=m['rank'], family=m['family'], model=None, note=HUMAN); continue
    sp = {ab: ABILITY[ab] for ab in m.get('abilities', []) if ab in ABILITY and ABILITY[ab] in models[model]['clips']}
    scr = m.get('script')
    if scr == 'burrower': sp.update(burrow='Burrow', burrowed_idle='BurrowedIdle', emerge='Emerge')
    if scr == 'flier': sp.update(dive='Dive')
    if scr == 'hive_mother': sp.update(summon_drones='Summon')
    if scr == 'hive_warden': sp.update(charge='Charge')
    if 'Howl' in models[model]['clips']: sp.update(howl='Howl')
    if 'Hatch' in models[model]['clips']: sp.update(hatch='Hatch', leap='Leap')
    if m['family'] == 'invader_worker': sp.update(flee='Run')
    out_mobs[mid] = dict(name=m['name'], rank=m['rank'], family=m['family'], scale=m['scale'], model=model,
                         base_color=f'{model}/textures/T_{model}_{variant}_BaseColor.png' if variant else models[model]['textures']['BaseColor'],
                         specials=sp)
structures = {
 'HiveEggSac': dict(live='HiveEggSac/SK_HiveEggSac.fbx', destroyed='HiveStructures/SM_HiveEggSac_Burned.fbx',
                    used_by='"Egg Sac" object on the Egyptian stage in the Hive Mother fight (must be burned)'),
 'HiveEggCluster': dict(live='../props/hive-egg-cluster.glb (already imported as a prop)', destroyed='HiveStructures/SM_HiveEggCluster_Destroyed.fbx',
                        used_by='egg clusters: q_hb_22, the Brood Keeper fight, hive spread'),
 'HiveSpire': dict(live='../props/hive-spire.glb (already imported as a prop)', destroyed='HiveStructures/SM_HiveSpire_Destroyed.fbx',
                   used_by='hive spires: q_hb_16 charges; spawn a Drone every 30 s until destroyed'),
}
json.dump(dict(version=1, fps=FPS, forward='+X', up='+Z', pivot='ground under the body centre',
               scale='mesh size at actor scale 1.0 is size_m_at_scale_1; multiply by the mob scale from mobs.json and nothing else',
               models=models, mobs=out_mobs, structures=structures), open('enemies.json', 'w'), indent=1)
print(f'{len(models)} models, {len(out_mobs)} mobs -> enemies.json')
