# Character Art Order: Human Bodies, Clothing, Hair, Weapons

Work order for the agent or artist producing Hollywoodland's human character art. Each requirement below is either a hard requirement ("must") or a stated preference. The integrator imports the delivery mechanically from `manifest.json` (§10), so names, sizes and the UV layout must match exactly. If a requirement can't be met, follow §12 rather than improvising.

Context: a classic-MMO look (early World of Warcraft polygon budget and texturing method) for a WebGPU browser build that must draw **40+ avatars at 60 fps** on ordinary laptops. All players and human NPCs use this system. Insect mobs are a separate order.

## 1. Visual style

- **Era and setting:** Hollywood, 1930, under an alien insect occupation. Use period silhouettes: fedoras, newsboy caps and cloche hats; high-waisted trousers with braces; bias-cut dresses; work overalls; doughboy helmets. Hats are the defining silhouette element.
- **Proportions:** stylised and heroic like early WoW humans.
  - Head about 1/7 of body height.
  - Hands and feet 1.25× realistic size; male shoulders broad.
  - Silhouettes must read at about 120 px of screen height.
  - Male height 185 cm and female height 172 cm (soles to crown, no hat).
- **Texturing:** hand-painted albedo with light painted from above, painted ambient occlusion in folds, and visible but restrained brush texture. No normal, roughness or metal maps. The material is a simple lit shader, so painted shading carries the form.
- **Palette:** warm golden-hour base (cream, sepia, ochre, oxblood, bottle green, slate, teal accents), with saturation highest on accents.
  - Invader material **chitin** (tiers 5–6 and epics): glossy green-black with amber or violet rim highlights painted in.
  - Art-deco motifs (chevrons, sunbursts, stepped trim) mark higher tiers.
- **Faces:** must be original faces that do not resemble real people, with a simple painted expression (WoW-classic level of detail). Avoid caricatures of ethnicity: the 8 skin tones apply to any face.

## 2. Hard performance budgets

**Texture resolution update (user direction, 12 November 2026):** character and item textures must be **1K or 2K**, never 512² or smaller, and never 4K. Use 2048×2048 for body, skin, underwear, face, scalp and hair masters; use 1024×1024 for clothing layers, hats, held items and their masks. A clothing or item texture may use 2048×2048 when closeup detail warrants it. Masks match their associated layer's resolution. This supersedes all earlier lower-resolution targets. Preserve real source detail rather than enlarging a degraded 512² image.

| Item | Triangles (LOD0) | Texture |
|---|---|---|
| Base body (always-on pieces + default geosets, §5) | 2,000–3,000 | body atlas 2048×2048 (§4) |
| Each geoset variant | ≤ 400 additional | body atlas |
| Hairstyle | ≤ 400 | hair atlas 2048×2048 per sex, shared by all styles |
| Hat | ≤ 300 | 1024×1024 each; 2048×2048 when needed |
| Weapon, small (knife, fist, bible, bag) | ≤ 200 | 1024×1024; 2048×2048 when needed |
| Weapon, one-handed (club, tool, sabre, revolver, pistol) | ≤ 350 | 1024×1024; 2048×2048 when needed |
| Weapon, large (two-handed, rifle, shotgun, SMG, shield, lamp) | ≤ 550 | 1024×1024; 2048×2048 when needed |
| **Worst-case fully dressed and armed avatar** | **≤ 4,500** | — |

- Skeleton: **exactly the 37 bones in §3**, with no extras.
- Skin weights: at most 4 influences per vertex (prefer ≤ 2 on limbs), normalised, and no unweighted vertices.
- Mesh:
  - Triangulate on export.
  - No n-gons, zero-area faces, non-manifold edges, flipped normals or duplicate vertices.
  - Smooth normals, with hard edges only where the silhouette needs them (hat brims, blades).
- Materials:
  - One material slot named `M_Body` for the body and all geosets, and one named `M_Hair` for hair.
  - Each hat and weapon has a single slot named `M_<asset name>`.
- LODs: deliver LOD0 only (the integrator generates LOD1 at 50% and LOD2 at 25%). Keep the topology clean enough to decimate.
- Transparency: opaque everywhere. Masked (alpha-tested) cards are allowed only for hair fringes and veils, and only if needed. Use no translucency.

## 3. Skeleton (shared by both sexes)

Both bodies use **the same hierarchy and bone names** with different proportions, as UE5's Manny and Quinn do. The names are a strict subset of the UE5 Mannequin's, so stock UE5 animations retarget through IK Retargeter.

```
root                                  (at origin, on the ground)
└ pelvis
  ├ spine_01
  │ └ spine_02
  │   └ spine_03
  │     ├ neck_01
  │     │ └ head
  │     ├ clavicle_l
  │     │ └ upperarm_l
  │     │   └ lowerarm_l
  │     │     ├ lowerarm_twist_01_l
  │     │     └ hand_l
  │     │       ├ thumb_01_l └ thumb_02_l
  │     │       ├ index_01_l └ index_02_l
  │     │       └ middle_01_l └ middle_02_l     (middle also drives ring + pinky)
  │     └ clavicle_r … (mirror of the left side, suffix _r)
  ├ thigh_l
  │ └ calf_l
  │   └ foot_l
  │     └ ball_l
  └ thigh_r … (mirror)
```

That is 37 bones: 7 central, 11 per arm and 4 per leg.

- **Bind pose:** A-pose with the arms 45° below horizontal, palms facing down, and fingers straight with the thumb at 45°. Legs are 10 cm apart and feet point forward.
- **Ring and pinky fingers:** these stay in the mesh but are skinned to `middle_*`. Hands are simple mitten-plus-thumb forms with modelled finger grooves.
- **Wrist twist:** weight the forearm half-and-half between `lowerarm_l` and `lowerarm_twist_01_l` to prevent candy-wrapping.
- **Bone axes:** must mirror consistently left to right. Matching the UE5 Mannequin's local axes is not required.
- **Export content:** deform bones only. Do not export IK controls, constraints, custom properties or leaf/end bones.
- **Shape keys:** none.

## 4. Body texture atlas (2048×2048, shared logical layout)

Every body mesh and geoset is UV-mapped into these fixed regions on **both** sexes. This lets one clothing layer fit both bodies and lets layers be composited into a single texture per avatar at runtime.

The table below uses **logical coordinates on a 512×512 layout grid**, measured from the top-left. **512 is not a delivered image resolution.** Multiply every coordinate, dimension and padding value by **4** for a 2048² image, or by **2** for a 1024² clothing layer or mask. Normalized UVs are identical at both resolutions. Resize layers to the target body atlas resolution before runtime compositing; scale pixel rectangles accordingly.

| Region | x | y | w | h | Contents |
|---|---|---|---|---|---|
| ArmUpper | 0 | 0 | 256 | 128 | shoulder to elbow (left/right mirrored, share UVs) |
| ArmLower | 0 | 128 | 256 | 128 | elbow to wrist (mirrored) |
| Hand | 0 | 256 | 256 | 64 | hand (mirrored) |
| FaceUpper | 0 | 320 | 256 | 64 | scalp, brow, eyes, ears |
| FaceLower | 0 | 384 | 256 | 128 | nose, mouth, jaw, neck, back of head |
| TorsoUpper | 256 | 0 | 256 | 128 | chest and upper back |
| TorsoLower | 256 | 128 | 256 | 64 | waist and belt line |
| LegUpper | 256 | 192 | 256 | 128 | hip to knee (mirrored) |
| LegLower | 256 | 320 | 256 | 128 | knee to ankle (mirrored) |
| Foot | 256 | 448 | 256 | 64 | foot (mirrored) |

Rules:

- UV islands stay inside their region with ≥ 4 logical px padding to the region edge (16 actual px at 2K, 8 actual px at 1K), and texel density is roughly uniform.
- Limbs are mirrored, so limb textures are symmetric by design. Torso and head are unique, not mirrored.
- Seams fall on region boundaries where possible: neck, waist, mid-upper-arm, elbow, wrist, knee and ankle.
- Each geoset also maps into the regions of the slot it belongs to (§5, "UV regions").

**Slot → regions a layer may paint:**

| Layer | May paint |
|---|---|
| Skin, Underwear | all |
| Face, Scalp | FaceUpper, FaceLower |
| Shirt | TorsoUpper, TorsoLower, ArmUpper, ArmLower |
| Trousers | LegUpper, LegLower, TorsoLower |
| Shoes | Foot, LegLower |
| Coat | TorsoUpper, TorsoLower, ArmUpper, ArmLower, LegUpper, LegLower (long coats and skirts) |
| Wrist | ArmLower |
| Gloves | Hand, ArmLower |
| Belt | TorsoLower |

**Composite order (bottom to top):** Skin → Underwear → Face → Scalp → Shirt → Trousers → Shoes → Coat → Wrist → Gloves → Belt.

## 5. Geosets (swappable mesh pieces)

Each body FBX contains the base pieces and every geoset variant. Each piece is a separate mesh object, skinned to the same armature and named exactly `GEO_<Group>_<Variant>`. The game shows one variant per replace-group and any number of additive ones.

| Group | Variants | Kind | UV regions |
|---|---|---|---|
| Head | `GEO_Head_Base` | always | FaceUpper, FaceLower |
| Torso | `GEO_Torso_Base` (default), `GEO_Torso_Bulky` (padded vest or steel plate: +4 cm chest, shoulder pads) | replace | TorsoUpper, TorsoLower, ArmUpper |
| Arms | `GEO_Arms_Base` | always | ArmUpper, ArmLower |
| Hands | `GEO_Hands_Base`, `GEO_Hands_Gauntlet` (flared cuff to mid-forearm) | replace | Hand, ArmLower |
| Legs | `GEO_Legs_Base` (fitted trousers / bare), `GEO_Legs_Flare` (breeches, chaps: wide thighs), `GEO_Legs_Skirt` (dress or skirt to mid-calf, closed tube) | replace | LegUpper, LegLower |
| Feet | `GEO_Feet_Base` (shoe form, no toes), `GEO_Feet_Boot` (to mid-calf, turned cuff), `GEO_Feet_TallBoot` (to knee) | replace | Foot, LegLower |
| Coat | `GEO_Coat_Tails` (tailcoat tails or jacket skirt to upper thigh), `GEO_Coat_Long` (trench or duster to knee, open front) | additive | LegUpper, LegLower |
| Belt | `GEO_Belt_Heavy` (buckle, pouches, holster loop) | additive | TorsoLower |
| Collar | `GEO_Collar_Up` (turned-up coat collar) | additive | TorsoUpper |

Rules:

- **Seams:** replace-variants must share **identical seam rings** (vertex positions, normals and weights) with their neighbours at the neck, waist, wrists, knees and ankles, so there are no gaps whichever variants are combined.
- **Clipping:** additive geosets sit 0.5–1.5 cm off the surface they cover and must not clip through any replace-variant during the ROM animation (§9).
- **Skin weights:**
  - `GEO_Legs_Skirt` and `GEO_Coat_Long` are weighted to `pelvis` and the thighs, smoothly, so a running stride doesn't tear them; calves get ≤ 0.3.
  - Everything else follows the body's weights.
- **Feet:** `GEO_Feet_Base` is a plain shoe shape. The female version may build in a 3 cm period heel; the bind pose stays flat-footed.

## 6. Bodies, faces, skin, hair

For each sex (`Male`, `Female`):

- **Body FBX:** the armature plus every `GEO_*` mesh from §5.
- **Skin:** 8 skin-tone textures `T_<Sex>_Skin_01..08.png` (2048², RGB, no alpha). They are full-body bare skin and follow the same paintover; only the tone differs, from 01 lightest to 08 darkest, with varied undertones.
- **Underwear:** `T_<Sex>_Underwear.png` (2048² RGBA, alpha = coverage), modest 1930s undergarments.
  - Male: undershirt and knee-length shorts.
  - Female: camisole and slip to the knee.
- **Faces:** 12 face overlays `T_<Sex>_Face_01..12.png` (2048² RGBA, painted only in the face regions).
  - Contents: eyes, brows, lips, facial structure shading, and stubble or moustache on some male faces.
  - They must read correctly over all 8 skin tones, so paint them with tone-neutral shading (multiply-style darks, sparse lights).
- **Hair:** 12 hairstyles per sex. Each has:
  - mesh `SK_<Sex>_Hair_01..12` (rigid to `head`, 100% weight);
  - optional brim-split mesh `SK_<Sex>_Hair_NN_Top` for the part a hat hides (if a style has no `_Top`, the whole style shows under hats);
  - a scalp overlay `T_<Sex>_Scalp_NN.png` (2048² RGBA, face regions) for hairline and stubble.
- **Hair atlas:** all hair meshes share `T_<Sex>_Hair.png` (2048²). Hair and scalp are painted **greyscale** (mean value about 0.55, with painted highlights) because the game tints them to the chosen hair colour. A style may be scalp-only, with no mesh (crew cut, receding).
- **Styles, men:** slicked back, side part, pompadour, crew cut, centre part, wavy, receding, bald with stubble, curly, short afro, tousled, side-swept long.
- **Styles, women:** finger-wave bob, pin curls, sleek bob with bangs, Marcel waves, chignon, short curly crop, shoulder-length waves, braided crown, low bun, pageboy, short afro, wind-swept.

## 7. Clothing looks

There are **32 looks**: 24 (4 armour types × 6 tiers) plus 8 epics. A look is one outfit across the visible slots: Hat, Shirt, Coat, Wrist, Gloves, Belt, Trousers and Shoes. Each item in the game maps to one look, so every piece of a look must also stand alone, mixed with other looks.

Each look has, **per sex** (the female version re-interprets the outfit with period womenswear, e.g. a dress in place of trousers):

- one **layer texture per slot**: `T_<Look>_<Sex>_<Slot>.png` (1024² RGBA; 2048² if needed, painted only in that slot's regions from §4, with alpha = coverage);
- one **tint mask per slot**: `T_<Look>_<Sex>_<Slot>_Mask.png` (same size as its layer: 1024² or 2048² RGB, linear), where R = primary recolour area, G = secondary recolour area and B = unused (0). Blue-quality items and colour sets recolour these areas. Leave metal, buttons and skin at 0;
- a **geoset choice per slot** (from §5) and an optional **hat mesh** `SM_Hat_<Look>` (one mesh fits both sexes' heads, pivot at the crown where it sits on the head, with flag `hides_hair_top`).

Tiers map to levels as follows: T1 = 1–9, T2 = 10–19, T3 = 20–29, T4 = 30–39, T5 = 40–49 and T6 = 50–60. Chitin appears from T5. The base items each look must evoke come from `tools/gen/items.mjs`:

| Look | Male direction | Female direction |
|---|---|---|
| C1 | newsboy cap, shirt and vest with braces, wool trousers, canvas plimsolls | cloche, blouse and wool skirt, plimsolls |
| C2 | felt fedora, wool sack suit, flannel trousers, oxfords | felt cloche, belted day dress, oxfords |
| C3 | Stetson fedora, tailored three-piece suit, French cuffs, kid gloves | tilted brim hat, skirt suit, kid gloves |
| C4 | silk top hat, opening-night tailcoat, striped trousers, patent pumps, opera gloves | feathered headband, bias-cut evening gown (skirt), opera gloves |
| C5 | white dinner jacket with deco gold trim, sash | sequined deco gown, fur stole collar |
| C6 | black chitin-lined formal, iridescent lapels | chitin-beaded gown, chitin tiara-hat |
| L1 | leather flying cap, work jacket, leather gloves | same, fitted |
| L2 | horsehide jacket, riding breeches (Flare), riding boots | same |
| L3 | Stetson Boss of the Plains, chaps, gun belt (Gower Gulch cowboy) | cowgirl: hat, fringed jacket, split skirt |
| L4 | stunt rider's helmet and leggings, cavalry boots, roping cuffs | same |
| L5 | Pendleton-lined riding coat (Long), Sam Browne belt | same |
| L6 | chitin-studded riding leathers | same |
| P1 | canvas work cap, Carhartt chore coat, Levi's waist overalls, hobnail boots | same, rolled sleeves, headscarf option |
| P2 | lineman: tool belt (Heavy), lineman's gloves and boots | same |
| P3 | leather football helmet, shoulder pads (Bulky), quilted leggings | same |
| P4 | quilted aviator helmet and vest, logger boots | same |
| P5 | welder's gauntlets (Gauntlet), heavy canvas, goggles on hat | same |
| P6 | chitin-quilted padding | same |
| S1 | M1917 helmet, steel vest (Bulky), trench boots, cartridge belt | same |
| S2 | riveted helmet, plate-insert trench coat (Long), steel-kneed breeches | same |
| S3 | mail-backed gloves, plated gauntlets, plated belt | same |
| S4 | full studio-forged plate over uniform (Bulky, TallBoot) | same |
| S5 | blackened plate with brass deco trim | same |
| S6 | chitin-plated cuirass and greaves, chitin-crested helmet | same |
| E1–E8 | one epic per class: Veteran "Argonne Commander", Stuntman "Daredevil", Gunman "Lawman", Electrician "Klieg Rig" (cables, lamp pack), Powder Man "Pyrotechnician", Nurse "Red Cross Matron", Preacher "Revival Tent", Mechanic "Studio Grip" | same |

**Starter outfit** (`Starter` look, both sexes): a plain shirt or blouse, trousers or skirt, and shoes. Paint it in neutral colours with tint masks, because the 8 creation colour sets recolour it. Supply the 8 colour sets as `starter_colours` in the manifest (primary and secondary sRGB hex per set).

## 8. Weapons and held items (41 meshes)

Every weapon follows the same rules:

- **Static mesh** `SM_Wpn_<Name>`. The pivot is the centre of the grip where a closed hand holds it.
- **Axes:** +X runs along the blade or barrel, away from the hand; +Z is the weapon's top (sights, edge-up for sabres). One-handed items are modelled for the right hand. Shields and lamps are held in the left hand, with the pivot at the handle.
- **Scale:** real-world, enlarged 1.15× for readability.
- **Texture:** one texture per weapon (§2) and one material `M_Wpn_<Name>`, painted in the style of §1.

| Type | Meshes |
|---|---|
| Knife | Pocketknife, Straight Razor, Hunting Knife, M1905 Bayonet, Trench Knife |
| Fist | Taped Knuckles, Brass Knuckles, Weighted Gloves (pivot at the knuckle bar) |
| Club | Cane, Blackjack, Police Nightstick, Louisville Slugger |
| Tool | Pipe Wrench, Crowbar, Hatchet, Lineman's Wrench |
| Sabre | Prop Fencing Sabre, M1913 Cavalry Sabre |
| Heavy (2H) | Trench Shovel, Pickaxe, Fire Axe, Sledgehammer (pivot at the lower hand; add a locator `GRIP_L` at the upper-hand position) |
| Revolver | Colt Police Positive, S&W .38 Military & Police, Colt Single Action Army |
| Pistol | Colt M1911 |
| Rifle (2H) | Winchester 1894, Remington Model 8, Springfield M1903 (pivot at the trigger grip, `GRIP_L` at the fore-end) |
| Shotgun (2H) | Winchester 1897, Remington Model 11 (`GRIP_L` as for rifles) |
| SMG (2H) | Thompson M1928 (`GRIP_L` at the fore-grip) |
| Shield | Trash Can Lid, Riot Shield |
| Lamp | Work Lamp, Carbide Lamp, Arc Lamp (the bulb or lens is an emissive area in the mask's B channel) |
| Bag | Medical Bag, J&J First Aid Kit |
| Bible | Pocket Testament, Family Bible |

`GRIP_L` is an FBX empty (locator) child of the mesh, named exactly `GRIP_L`.

## 9. Animation

**Required:** one range-of-motion clip per sex, `A_<Sex>_ROM` (30 fps, about 300 frames), that rotates every bone through its working range, including a full run stride and arms raised overhead. The integrator uses it to check weights and geoset clipping.

**Optional**, in priority order. If these are not delivered, the integrator retargets UE5 Mannequin animations instead.

- **Clip format:**
  - 30 fps, in place, no root motion (`root` stays at the origin), no scale keys.
  - Loops are seamless with first frame equal to last.
  - Delivered per sex as `A_<Sex>_<Name>.fbx`, skeleton only.
  - Hands close on a grip for weapon stances.
- **Clips:**
  1. `Idle`, `Run`, `RunBack`, `StrafeL`, `StrafeR`, `Walk`, `JumpStart`, `JumpLoop`, `JumpLand`, `Death`, `DeadLoop`
  2. `Combat_Unarmed`, `Combat_1H`, `Combat_2H`, `Combat_Ranged1H`, `Combat_Rifle` (one ready-idle each), then the attacks `Attack_Unarmed`, `Attack_1H_A`, `Attack_1H_B`, `Attack_2H`, `Shoot_Pistol`, `Shoot_Rifle`, `CastLoop`, `CastRelease`, `HitReact`
  3. `SitDown`, `SitLoop`, `StandUp`, `Loot`, `Talk`
  4. Emotes: `Wave`, `Laugh`, `Cheer`, `Shrug`, `Point`, and `Dance` (a period dance: Charleston for women, Lindy-style or tap for men; distinct and fun)

## 10. Delivery layout and manifest

Put the delivery in the repo under `art/characters/`, under 200 MB in total. Put `.blend` sources under `art/characters/src/` only if each file is under 50 MB; otherwise keep them outside the repo.

```
art/characters/
  manifest.json
  bodies/   SK_Male.fbx  SK_Female.fbx  A_Male_ROM.fbx  A_Female_ROM.fbx
  skin/     T_Male_Skin_01..08.png  T_Male_Underwear.png  T_Male_Face_01..12.png  (same for Female)
  hair/     SK_Male_Hair_01..12[_Top].fbx  T_Male_Hair.png  T_Male_Scalp_01..12.png  (same for Female)
  looks/    T_<Look>_<Sex>_<Slot>[_Mask].png  SM_Hat_<Look>.fbx  T_Hat_<Look>.png
  weapons/  SM_Wpn_<Name>.fbx  T_Wpn_<Name>.png  [T_Wpn_<Name>_Mask.png]
  anim/     A_<Sex>_<Name>.fbx                         (optional)
  preview/  contact sheets (§11)
```

Use a single FBX convention throughout: binary FBX 7.4, Z-up, the character facing +Y, 1 unit = 1 cm in UE, all transforms applied, and scale 1.0. From Blender, use the default FBX export axes with "Apply Unit" checked, "Add Leaf Bones" off, and the armature object named `Armature`, which UE drops.

Textures are 8-bit PNG, sRGB except masks, which are linear. Sizes are 1024² or 2048² as specified above; no delivered character or item textures may be 512² or smaller. Record actual texture dimensions in the manifest. The logical 512-grid atlas table is a layout contract, not permission to deliver 512² textures.

`manifest.json` lists every look so that import needs no guesswork:

```json
{
  "version": 1,
  "starter_colours": [{"primary": "#7a3b2e", "secondary": "#d8c8a0"}],
  "looks": [{
    "id": "C1", "type": "cloth", "tier": 1, "name": "Street Clothes",
    "hat": {"mesh": "SM_Hat_C1", "hides_hair_top": true},
    "slots": {
      "coat":     {"Male": {"layer": "T_C1_Male_Coat", "geosets": []},          "Female": {"layer": "T_C1_Female_Coat", "geosets": []}},
      "trousers": {"Male": {"layer": "T_C1_Male_Trousers", "geosets": ["Legs_Base"]}, "Female": {"layer": "T_C1_Female_Trousers", "geosets": ["Legs_Skirt"]}},
      "shoes":    {"Male": {"layer": "T_C1_Male_Shoes", "geosets": ["Feet_Base"]}, "Female": {"layer": "T_C1_Female_Shoes", "geosets": ["Feet_Base"]}}
    }
  }],
  "hair": {"Male": [{"id": 1, "name": "Slicked Back", "mesh": "SK_Male_Hair_01", "top": "SK_Male_Hair_01_Top", "scalp": "T_Male_Scalp_01"}]},
  "weapons": [{"name": "Trench Shovel", "mesh": "SM_Wpn_TrenchShovel", "type": "heavy", "grip_l": true}]
}
```

- Omit a slot when a look has no piece for it, and omit a `_Mask` file when the piece has no recolour area. Masks follow the naming rule.
- Geosets in the manifest are named without the `GEO_` prefix.
- Weapon `name` matches the base names in `tools/gen/items.mjs` exactly.

## 11. Acceptance checklist (self-check before delivery)

- [ ] Bone names and hierarchy match §3 exactly in both bodies. Count = 37.
- [ ] Every delivered character/item texture is 1024² or 2048², with no 512² or smaller images; masks match their layers. Body/face/hair masters are 2048². The manifest records dimensions, and atlas coordinates/padding are scaled from the logical 512 grid.
- [ ] Triangle counts are within §2, including the worst case: the heaviest look plus hair, hat and a two-handed weapon is ≤ 4,500.
- [ ] Every vertex has 1–4 normalised weights. No vertex is unweighted.
- [ ] UVs follow §4: islands sit inside their regions with padding, and both sexes use the same layout.
- [ ] Every replace-geoset combination has no seam gaps, checked in the bind pose and in the ROM clip.
- [ ] The ROM clip shows no clipping and no candy-wrapping.
- [ ] Every face overlay reads correctly on all 8 skin tones (a 12×8 contact sheet in `preview/`).
- [ ] Every look is shown on both sexes, front and back, in `preview/looks_<Sex>.png`.
- [ ] Every weapon is shown held in the right hand in `preview/weapons.png`, confirming pivot and axes.
- [ ] The manifest is valid JSON, and every referenced file exists with exactly that name.
- [ ] All assets are original or released under licences compatible with a public MIT repository (CC0, CC-BY or own work). No marketplace content that forbids redistribution, and no likenesses of real people.

## 12. Fallbacks (only if the tooling can't meet a rule)

- **Can't paint into the shared atlas:** deliver each clothing piece as its own skinned mesh with its own 1024² texture (2048² if needed), shrink-wrapped to the body (≤ 600 triangles). The integrator will rebake it to §4 in Blender.
- **The auto-rigger's skeleton has extra bones:** keep it, but rename the bones in §3 to the exact names. The integrator will collapse the extras.
- **Can't match seam rings exactly:** keep vertex positions identical within 0.1 cm at the boundaries. The integrator will weld them.

## 13. Phasing (deliver in this order; each phase is usable on its own)

1. **Character creation:**
   - both bodies with every geoset, the skin tones, faces, hair, underwear, the starter outfit and the ROM clips;
   - looks **C1** and **S1** (S1 tests Bulky, Boot and the hat);
   - the 5 starter weapons: Trench Shovel, Pocketknife, Colt Police Positive, Pipe Wrench, Cane.
2. **Animations, optional:** priorities 1–2 from §9.
3. Looks C2–C4, L1–L4, P1–P4 and S2–S4 (playable levels 1–12 show tiers 1–2; the rest appear on high-level simulated players).
4. All remaining weapons.
5. Tiers 5–6, the epics, and animation priorities 3–4.
