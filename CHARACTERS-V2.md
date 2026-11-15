# Characters v2: pipeline

The v1 characters (art/characters) paint every outfit onto a 2.8k-triangle bodysuit with 37 bones. v2 replaces them with
real garment geometry on full-resolution MakeHuman bodies. The game contract is unchanged: two sexes, 8 skin tones, 12
faces, 12 hairstyles and 12 hair colours per sex, 33 looks across 7 mixable slots (shirt, trousers, shoes, coat, wrist,
gloves, belt), hats, held items, and every gameplay animation.

## Pipeline (tools/charv2, Blender 5.3 + MPFB, fal FLUX 3, Hyper3D Rodin)

1. **Concepts** (`concepts.py`): FLUX 3 Klein turnarounds per look and sex (`art/characters_v2/concepts/`).
2. **Bodies** (`build_body.py`): MakeHuman base mesh, heroic proportions, MPFB `game_engine` skeleton (UE Mannequin
   names, full fingers), MakeHuman eyes, brows and lashes. The 12 faces are shape keys `Face_01..12` built from
   MakeHuman face targets that leave the eyes in place (`FACES`); they export as morph targets. The female skin is
   MakeHuman's young_caucasian_female2 with painted makeup (`makeup.py`: lips, lash line, smoky lid, blush).
3. **Export** (`export_v2.py`): bakes each mesh, stamps a `Zone` vertex colour (head, neck, chest, belly, hips, arms,
   hands, legs, feet) from the nearest body vertex's dominant bone, poses the fingers into a relaxed grip (`GRIP`) and
   makes that the rest pose (clips without finger tracks then close the hand on a held item), converts to centimetres
   and writes the FBX plus the base-colour map. Face shape keys survive the bake as deltas.
4. **Wardrobe** (`wardrobe.py`, `build_wardrobe.py`): 45 MakeHuman CC0/CC-BY garment bases fitted by MHCLO per sex and
   split per slot, 12 hair styles per sex (`grey_hair.py` makes them tintable). `LOOKS` maps each look to a base per slot
   and a palette.
5. **Hats** (`hats.py`, `fit_hats.py`): MakeHuman hats by MHCLO, plus Rodin hats from FLUX references, seated by
   percentile width and rim depth under the crown (review renders in `review/hats/`). Every hair style also exports a
   hat-safe variant pressed under the crown (`<hair>hat`).
6. **Gear** (`gear.py`, `fit_gear.py`): Rodin belts (slim, utility), wrist pieces (watch cuff, bracer) and cuirasses
   (male, female) fitted to the body by region (waist band, forearm by its surface normals, torso), pushed clear of the
   body and skinned from the nearest body vertices. `wardrobe.gear_for` gives town looks a slim belt and watch, rugged
   looks a utility belt and bracers, steel looks a cuirass over the coat.
7. **Paint** (`retex.py`): FLUX 3 `edit-image` repaints each base's UV atlas per look from the palette, keeping the
   layout (320 garment maps, `--gear` for the 76 gear maps, shared between the sexes).
8. **JSON** (`make_json.py`): `hollywoodland-ue/Content/Data/characters_v2.json` (looks per sex and slot with mesh,
   texture, layer and covered zones; hair; skin tones; hats).
9. **Unreal** (`hollywoodland-ue/Scripts/import_characters_v2.py` via `ImportCharacters.ps1 -Script
   import_characters_v2.py [-Only 'mat,tex,mesh,anim']`): materials, only the textures and meshes the JSON names (with
   web size caps), and every clip retargeted from the Mannequin and the v1 skeleton. No import turn: the FBX comes in
   facing +Y like the Mannequin, and any yaw mirrors the retargeted motion front to back.

## Runtime (HLCharV2, HLCharacter::ApplyV2)

One leader body plus a follower skeletal mesh per worn piece. Each piece hides the zones covered by the layers above it,
and the body hides everything worn covers; hidden vertices sink 4.5 cm under the garment (M_HLCharV2 world position
offset). Trousers never hide a shirt's hips. Hats are static meshes on the head bone; a hat that covers the hair swaps
the hair to its hat-safe variant. Faces are morph targets, skin tones and hair colours material multipliers. Held items
are gripped across the palm of the posed hand. `-HLV2Test` and `-HLLineup` (`-HLLineupGuns`) are the visual checks;
`-HLV1Chars` falls back to v1.

Assets: MakeHuman community packs (CC0, some CC-BY), cached by `fetch_mh_packs.py` under
`tmp/character-art-source/mh_packs/` (never committed).
