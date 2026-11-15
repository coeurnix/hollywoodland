# Characters v2: plan and pipeline

The v1 characters (art/characters) paint every outfit onto a 2.8k-triangle bodysuit with 37 bones. v2 replaces them with
real garment geometry on full-resolution MakeHuman bodies, painted from FLUX 3 concept art. The game contract stays: two
sexes, 8 skin tones, 12 faces, 12 hairstyles and 12 hair colours per sex, 33 looks across 7 mixable slots (shirt,
trousers, shoes, coat, wrist, gloves, belt), 38 hats and 41 held items, plus every gameplay animation.

## Pipeline

1. **Concepts** (`tools/charv2/concepts.py`): FLUX 3 Klein front/back turnarounds per look and sex, setting the
   target for shapes and paint (`art/characters_v2/concepts/`).
2. **Bodies** (`tools/charv2/build_body.py`, Blender 5.3 + MPFB): full MakeHuman base mesh, heroic proportions (head
   ~1/7.5, hands and feet enlarged, broad male shoulders), MPFB `game_engine` skeleton (UE Mannequin names, full fingers),
   MakeHuman eyes, brows and lashes. Body split into hideable sections so garments can hide what they cover. The 12
   faces are morph targets (MakeHuman face modifiers), the 8 skins are tone variants of one painted skin.
3. **Garments**: MakeHuman CC0/CC-BY clothes (Toigo suits, shirts, trousers, dresses, skirts, shoes, gloves, caps)
   fitted with MHCLO in MPFB's native frame, reshaped in Blender where the period needs it, weights from the body. Extra
   pieces (armour, helmets, hats, bandoliers, packs) from Rodin with FLUX references, fitted and rigidly skinned.
4. **Paint**: each garment is rendered as clay front/back; FLUX 3 `edit-image` repaints that render as the look (concept
   sheet as reference, silhouette kept), and the painted views are projected back onto the garment UVs. Tint masks
   come from the paint's colour regions.
5. **Animation**: every clip (Mannequin stock and the commissioned v1 set) is retargeted to the new skeleton in UE.
6. **Runtime** (HLCharacter): one leader body plus a follower skeletal mesh per worn garment; body sections hidden
   per garment; tint via material parameters; hats and held items on sockets, held items gripped by real fingers.

Assets: MakeHuman community packs (CC0, some CC-BY; see `art/characters_v2/LICENSES.md`), cached by
`tools/charv2/fetch_mh_packs.py` under `tmp/character-art-source/mh_packs/` (never committed).
