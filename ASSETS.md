# Placeholder asset inventory

Everything visible is a placeholder. Nothing below is final art. Counts follow `DESIGN-BIBLE.md` §8 and §14 "Assets". Human character art (bodies, looks, hair, weapons) is specified in `ART-CHARACTERS.md`.
"Where" = the code/data that must change when the real asset lands. Sourced public-domain media must be listed in the jam
submission (bible §2 rule 1).

## Present authored assets (all placeholder)

| Asset | Path | Purpose |
|---|---|---|
| `M_HL` | `Content/HL/M_HL.uasset` (made by `Scripts/make_assets.py`) | lit flat colour, params `Color`, `Glow` (emissive ambient) |
| `M_HLSky` | `Content/HL/M_HLSky.uasset` | unlit two-sided flat colour (sky shell, portal, target ring) |
| Engine basic shapes | `/Engine/BasicShapes/{Cube,Sphere,Cylinder,Cone}` | every mesh in the game |
| Roboto (engine default) | Slate core style | all UI text |

## 3D models

**Props (imported):** `art/props/` holds 66 Unreal-ready `.glb` props generated with Hyper3D Rodin (1 unit = 1 m, pivots baked in; notes in `art/props/README.txt`). `tools/propprep.py` simplifies each mesh with meshoptimizer through gltf-transform (`tools/props/optimize.mjs`: weld, then error-bounded simplification at 0.05% of the prop's size, so silhouettes and seams hold and no gaps open; the bound is raised at most 4x only for props over their soft triangle ceiling), resizes textures (base colour 1024, normal 512-1024, roughness/metal 256-512), then Blender 5.3 (`tools/propprep_blender.py`) fixes scale where the README sizes read wrong, turns the front to Unreal +X and stages FBX + PNG. `hollywoodland-ue/Scripts/ImportProps.ps1` imports them into `/Game/Props` (`SM_<Name>`, `MI_<Name>` on `M_HLWorldProp`, LOD1 50% under 20% of the screen / LOD2 25% under 7%, no collision) and writes their bounds into `Content/Data/props.json`. `tools/gen/props.mjs` places them: `tools/gen/zone.mjs` dresses the boulevard and the Egyptian (shop signs, lamps, trolley poles, marquees, cars and wrecks as solid boxes, fire escapes, sandbags, tents, rubble, hive spires, eggs and membranes) and gives every interactable object a prop mesh. `AHLWorldActor::AddProp` spawns them (cull distance from size; arc lamps get a light when lit). `/Game/Props` is always cooked.

**World pieces (imported):** `art/world/` holds 34 more Rodin `.glb` models (FLUX reference images, same conventions as `art/props`; sizes in `art/world/README.txt`). `tools/propprep.py` takes them as `world:<category>/<name>` (the real-world scale rides on the glTF node, which `bounds()` now applies) and they import with the props. In the game: the Pacific Electric car (`redcar_pe`, height 0.8) is the red-car object; the mounts sit under seated riders (`AHLWorldActor::MountSeat`, `HLMountKind` maps persona mount names); the four wrecks join the back-lot cover boxes; the interactables mix in with the older props (`OBJ_PROPS` in `tools/gen/zone.mjs`; the cocoon is the captured usher); the Egyptian kit dresses the dungeon (sphinx avenue, pharaohs, box office, lotus columns, hieroglyph panels, 60 seat rows, projector, organ, proscenium); the HOLLYWOODLAND letters stand 26.8 m tall on the north ridge past the hive ring, facing the city.

**Foliage (imported):** `tools/foliagegen.py` makes FLUX reference images (`art/foliage/src`, local) and Rodin models (`art/foliage/raw`, local, ~370 MB) for 11 plants and rocks (fan and canary palms, eucalyptus, pepper tree, chaparral, oleander, agave, yucca, prickly pear, boulder, flat rock); `tools/foliage_cards.py` keys six grass and wildflower images into alpha sprites and crossed-card GLBs (`art/foliage/sprites`, `art/foliage/models`). `tools/propprep.py` scales them to height (`('h', metres)`), re-pivots them to their base, and caps the generated meshes (meshoptimizer, then Blender collapse when the UV seams stall it: trees 30k, shrubs 6k, rocks 3k). Swaying ones get `MI_*` on `M_HLFoliage` (two-sided, masked by the base colour alpha, wind sway by height above the pivot, `Sway` per plant). Palms line the boulevard and Highland (`zone.mjs`); `AHLWorldActor::ScatterFoliage` scatters grass tufts, wildflower drifts, shrubs, rocks and trees from the terrain's wildness (dry grass and rocks only on the blight, a scrub woodland on the hills), one hierarchical instanced component per mesh, grass culled at 30-90 m by quality.

**Enemies (imported):** `art/enemies/` holds rigged, animated FBX + GLB models for every mob in the built slice, plus destroyed hive structures. `art/enemies/enemies.json` maps each mob id to its model and clips, and `art/enemies/README.md` is the integration guide. `hollywoodland-ue/Scripts/ImportEnemies.ps1` (`Scripts/import_enemies.py`) clears and re-imports `/Game/Enemies`: `SK_<Model>` with every take renamed `A_<Model>_<Clip>`, `MI_<Model>[_<Variant>]` on `M_HLEnemy` (base colour 1024, normal and metal/roughness 512, metal damped for the web tier), and the `HiveStructures` wrecks. `python -I tools/enemyprep.py` writes `Content/Data/enemies.json` (mob → model, variant material, specials; model → roles and clips). `HLEnemies.cpp` plays them; `-HLEnemyLineup` shows each model beside the player.

**Buildings (imported, in progress):** the building-art agent publishes reviewed checkpoints to `art/buildings/` (kept local; import only what `manifest.json` indexes). `python -I tools/buildprep.py` stages them (meshes as delivered; atlases and landmarks 2048, sign pages and tileables 1024) and writes `Content/Data/buildings.json`; `Scripts/ImportBuildings.ps1` runs the same importer into `/Game/Buildings` (painted, matte material, instancing-enabled). `tools/gen/facades.mjs` lays the BRK storefront family on the street face of every `shops()` box (corners, bays stretched between pilasters, upper rows per 3.5 m, parapet scaled to the remaining height, awnings, crests and damaged bays by seed); the Drugstore box draws as `LM_Drugstore` with its two lettering cards, the Haberdashery and the Taft's auction rooms get sign cards. `AHLWorldActor::BuildWorld` draws kit modules as one instanced component per mesh (about 1,000 pieces, 25 draws). `-HLViews` takes a screenshot tour of the fronts (`Saved/Screenshots/hl_view_NN.png`).

| Needed | Count | Current placeholder | Where |
|---|---|---|---|
| Landmark exteriors: Grauman's Chinese, Egyptian, Roosevelt, Hollywood Hotel, First National Bank, Taft Building, Brown Derby, Pantages, Hollywood High, police station | 10 | labelled coloured boxes (`label` on boxes in `tools/gen/zone.mjs`) | `AHLWorldActor::BuildWorld` (swap mesh by `FHLBox::Label`) |
| Aid stations: Receiving Hospital, Fire Station No. 27, Blessed Sacrament | 3 | boxes + red floor square | same |
| Street kit: storefronts (≈60 instances, 6-8 variants), signs, street lamps, red-car track, kerbs, period cars, rubble | 1 kit | IN PROGRESS: BRK storefront family on every shop front, Drugstore landmark, 4 sign cards (`tools/gen/facades.mjs`); SPN/DECO/MSN families, roof dressing and street kit pending from the building agent | `zone.mjs` `facades()`, `BuildWorld` |
| Hive kit: wall segments, gap, spire, egg cluster, egg sac | 5 | spires and egg clusters are props with wrecks from `art/enemies/HiveStructures`; the egg sac is the rigged `SK_HiveEggSac`; walls are still dark boxes | `BuildWorld` object switch (`O.Kind`) |
| Red car (Pacific Electric) | 1 | DONE: `redcar_pe` from `art/world` | object kind `redcar` |
| Wrecks: newsreel truck, cars | 3 | DONE: Rodin wrecks from `art/props` and `art/world` | `zone.mjs` wreck boxes |
| Player/NPC bodies | 2 base bodies | DONE: MakeHuman male/female, 37-bone rig, modular geosets (`Content/Characters/Body`) | `HLCharacter.cpp` (`FHLCharVisuals::Apply`) |
| Armour looks | 24 (4 types × 6 tiers) + 8 epic | DONE: 32 looks + starter, 38 hats; item → look by type letter, required level band and quality (`FHLChars::LookForItem`) | `Content/Data/characters.json` |
| Weapon meshes | ~40 | DONE: 41 held items (name match, else same type by hash) in the palm of either hand | `FHLChars::HeldForItem` |
| Invader meshes: Worker, Drone, Spitter, Soldier, Broodling, Brute, Burrower, Flier + bosses (Brood Keeper, Hive Warden, Spitter Matriarch, Hive Mother) | 12 | DONE: rigged models from `art/enemies` (gray-box shapes remain only as a fallback) | `FHLEnemyArt::Make` from `BuildUnit` |
| Wildlife: coyote, feral dog, Old Scar, hive-ridden coyote | 4 | DONE: rigged models from `art/enemies` | same |
| Humans: Looter, Dragna Enforcer | reuse character | DONE: character art (suits and blackjacks for the Dragna, work clothes and tools for looters) | `FHLChars::LookFor` |
| Mounts: horse, Ford Model A, motorcycle (+ 60 tier re-skins) | 3 | DONE: `art/world` mounts under seated riders (tier re-skins pending) | `SyncUnits` mount block |
| Interactables: film can, reel, coffee tin, arc lamp, call box, mailbox, scrap pile, captured usher | 8 | DONE: `art/props` and `art/world` props | `OBJ_PROPS` in `zone.mjs` |
| Dungeon interior: forecourt, lobby, auditorium (seat rows), projection booth, stage | 5 rooms | DRESSED: Egyptian kit from `art/world` inside the box rooms (wall shells still boxes) | `tools/gen/zone.mjs` dungeon section |
| Sky / dome | 1 | DONE: FLUX day and night panoramas on the 2.6 km shell (`M_HLSkyDome`), blended by the clock | `TickSky` |
| HOLLYWOODLAND sign on Mount Lee | 1 | DONE: `art/world` letters on the north ridge (HLTerrain hills past the hive ring) | `zone.mjs` dressing |

## Animation

| Needed | Current |
|---|---|
| Character: idle, run, jump, sit, melee swing, cast, shoot, hit react, death, loot, emotes (wave, dance, cheer) | DONE: 22 clips retargeted from the engine template Mannequin (`Scripts/retarget_anims.py`) + 19 authored for the rig (combat 1H/2H, cast, sit, loot, talk, wave, laugh, cheer, shrug, point, bow, dance); chosen per frame in `FHLCharVisuals::Drive` |
| Invaders, bosses, wildlife: idle, walk, run, attack, hit, death + specials | DONE: authored per model in `art/enemies` (see its README §6), driven by `FHLEnemyArt::Drive` |
| Mount ride | none |
| Crowd: vertex-animated variants (bible §13) | none |

## Textures / materials

| Needed | Current |
|---|---|
| Building facades (period signage: marquees, storefront lettering), road/sidewalk, hive resin, chitin | flat colours from data |
| Character skin/hair/clothing sets (12 faces, 12 hair, 8 skin, 8 clothing colours) | DONE: 8 skins, 12 painted faces and 12 hairstyles per sex, 8 hair colours, 8 starter dyes; one body atlas per look composed on the GPU |
| 1930 posters and lobby cards on theatre fronts (public domain, to be sourced and disclosed) | none |

## VFX

| Needed | Current |
|---|---|
| Ability effects per school (physical, electric, fire, acid, holy, heal), projectiles, auras (shield, DoT, stun) | none: floating numbers only |
| Acid pool telegraph, charge telegraph, egg hatch, spire explosion, portal | emote text in chat; portal = cyan cylinder |
| Level-up, quest complete, loot sparkle | banner text; "* loot *" label; "*" prefix on quest objects |
| Target ring | flat cylinder, colour by hostility |

## UI graphics

| Needed | Count | Current |
|---|---|---|
| Item icons | ~700 | square tinted by quality with 2-letter glyph (`SHLGameUI::ItemIcon`) |
| Ability icons | ~50 (61 abilities) | square tinted by school with the name as text |
| Class icons, faction crests, guild tabards | 8 / 8 / 8 | class colour only |
| Frames: unit frames, action bar, windows, tooltips, minimap ring, quest markers | 1 skin | flat panels + 1 px gold border; `!`/`?` are text |
| Fonts: period display face (title, zone banners) + readable body face | 2 | Roboto |
| World map illustration (13 zones under the dome), zone map | 2 | coloured rectangles from `world.json` `map`; boxes from layout |
| Red-car route map | 1 | text list |
| Loading screen / title art (no studio marks, bible §2 rule 3) | 1 | text title panel over a slow camera orbit |
| Cursor set (attack, loot, talk, vendor) | 5 | system arrow |

## Audio

| Needed | Current |
|---|---|
| Music: title, boulevard day/night, combat, dungeon, boss | DONE: 30 Lyria tracks in `music/` (`ART-SONGS.md`), wired by `HLMusic.cpp` |
| Ambience: street, crowd, hive drone, theatre interior | DONE: 19 seamless 22 s stereo beds (boulevard day/night, hills day/night, alley, rooftops, backlot, hub, hive wall, hive, dungeon, lobby, stage, ballroom, gym, police, pass, Fight Night crowd, rain), crossfaded per subzone by `HLSfx.cpp` |
| SFX: melee/gun/cast per school, hits, mob vocalisations ×9, death, loot, coin, UI clicks, level-up, quest accept/complete, red-car bell | DONE: 263 sounds / 500 variant files in `sfx/` (ElevenLabs SFX v3 via fal.ai, `tools/gen-sfx.mjs`; index `Content/Data/sfx.json`). Mastered by loudness (EBU R128): one-shots to -16 LUFS momentary max, loops/beds to -20 LUFS integrated, then a per-category mix gain (`CAT_GAIN`: ambience sits ~14 dB under combat): UI, footsteps ×6 surfaces, player grunts m/f, weapons and shots, all 61 abilities, cast loops per school, DoT ticks, aura sounds, aggro/attack/hurt/death for 13 creature families, boss specials, world objects, positional loops, event stingers |
| Period recordings (≤1925, public domain) on radios/phonographs | none |
| Voices: NPC greetings (15 real-person NPCs must not use cloned voices, bible §2) | DONE: 1,367 lines / 569 sounds (`vo_*.mp3` in `sfx/`, index `Content/Data/vo.json`): every NPC (hello, first greeting, goodbye, annoyed, vendor/trainer/bank/auction/inn/red-car lines), three patrolman voices with directions to every service, all 41 quests (offer, progress, complete), player barks and emote voices for 8 classes × 2 sexes. Script by Claude Opus 5.5, speech by ElevenLabs v4 (fal.ai) in stock library voices only, mastered to -17 LUFS. `tools/dialoguegen/` |

## Public-domain media (to source, verify, disclose)

| Item | Use | Status |
|---|---|---|
| *Hell's Angels* (1930) excerpt | loops on the Chinese screen / picture night | not sourced |
| *Robin Hood* (1922) excerpt | Egyptian stage after clear | not sourced (quest text references it) |
| 1930 posters, lobby cards, newspaper pages | facades, tooltips | not sourced |

## Text content still placeholder

- Quest text is 35-60 words (target 60-120, bible §10); breadcrumb destination NPCs are names only.
- NPC gossip greetings: 4 generic lines shared by all NPCs.
- Unbuilt zones carry 4-8 quest titles each (target ~45) and no mob level tables.
- LLM-generated corpus (`tools/cache/corpus.json`, 554 lines) has not had the human sample review the bible asks for.
