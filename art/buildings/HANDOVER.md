# Building art: handover (2026-11-14)

The building-art agent was stopped after phase 3 of `SPEC.md` §14: it was slow and expensive (32.5 Rodin credits in `rodin_ledger.json`, much of it on retries). This file is the entry point for whoever continues. `SPEC.md` is the original work order (still the reference for sizes, origins and budgets). `README.md` is the agent's own log: phase status, the pipeline decisions and the rebuild commands for its Blender scripts.

## State

**Done and in the game** (163 meshes, imported to `/Game/Buildings`, placed by `tools/gen/zone.mjs` + `tools/gen/facades.mjs`):

| Phase | What | Where it shows |
|---|---|---|
| 1–2 | Storefront kit: 4 families (BRK, SPN, DECO, MSN; 27 modules each), 12 damage/occupation dressing meshes (`DR_*`), 7 roof props (`RF_*`), 7 wall/roof tileables, the Drugstore landmark | every `shops()` box on the boulevard: `kitFront()`, `roofProps()` |
| 3 | Hub landmarks: `LM_Graumans` (+ forecourt walls), `LM_Roosevelt`, `LM_Egyptian` (+ wings, gateway), `LM_HollywoodHotel` (+ wings, courtyard walls), `LM_FirstNational`, `LM_Pantages`, `LM_Taft`, `LM_BrownDerby`, `LM_PoliceStation`, `LM_RedCarStop`, `LM_Kiosk_Auction`, and their 15 sign cards (+ 4 earlier) | `LM(...)` calls in `zone.mjs`; `landmark()` / `landmarkDeco()` in `facades.mjs` |

Check the landmarks with `hollywoodland-ue\Scripts\RunNative.ps1 -HLAuto=veteran -HLViews -HLViewSet=landmarks` (10 screenshots in `hollywoodland-ue/Saved/Screenshots/hl_view_NN.png`). Use `-HLViews` alone for the storefronts.

**Not done** (SPEC phases 4–9). Much of it is already covered by the other art pipelines, so the real list is shorter than the SPEC:

| SPEC | Item | Status / what is still needed |
|---|---|---|
| §8.1 street furniture | double-globe lamp, trolley pole, call box, mailbox, arc lamps, palms, rocks | **covered** by `art/props`, `art/world`, `art/foliage` (Rodin props) |
| | single-globe side lamp, semaphore signal, street-name post, hydrant, newspaper stand, bench, trash can, palm planter, kerb modules, crossing card | **needed** (small props: use the prop pipeline, below) |
| §8.2 vehicles | red car, Model A, Model T, Chevrolets, sedans, newsreel truck, delivery van, wrecks | **covered** (`redcar_pe`, `car_*`, `wreck_*`) |
| | 1920s taxi, Model A roadster | optional |
| §8.3 back lot | crates, scrap pile, film cans, coffee tins, sandbags, rocks | **covered** |
| | 2 film-lot flats, oil drums, wooden fence, chain-link fence, barricade | **needed** (props) |
| §8.4 hive kit | spires, egg clusters, egg sac, growth | **covered** (`art/enemies/HiveStructures`, `SK_HiveEggSac`) |
| | `HV_WallSeg` (8×18×16 m, tiling), `HV_WallEnd`, `HV_WallSeg_Outer` (26 m) | **done 2026-11-13** (Rodin full generation, `preview/HV_kit_contact.png`); placed by `hiveRun()` in `zone.mjs` (224 + 318 segments, 8 end posts). Not yet imported/screenshotted: see the session notes below |
| §7.1 remaining landmarks | Max Factor, Gun Shop, Haberdashery | currently kit storefronts + sign props: acceptable, upgrade optional |
| | `LM_HollywoodHigh` (66×8×9 north range) + `LM_HollywoodHigh_Wing` (8×52×9, mirrored) | **done 2026-11-14**: blocks + FLUX plates on `ATL_HH` (gym inside faces, hive corners), placed with `LM()`; `preview/CIVIC_contact.png` |
| | aid stations: Receiving Hospital, Fire Station No. 27, Blessed Sacrament church | **done 2026-11-14**: blocks + FLUX plates sharing `ATL_AID`, placed with `LM()` |
| §7.3 backdrops | 10 low-budget studio/landmark masses beyond the hive wall (Bowl, Paramount, Columbia, RKO, Chaplin, Poverty Row, Christie, cemetery, La Brea ×2) | **done 2026-11-14**: `BD_*` Rodin full generation, 3000 tris, four per 1024-quadrant atlas (`ATL_BD1–3`), placed with `LM()`; `preview/BD_contact.png`. The unlabelled side boxes next to them stay cubes |
| §11.1 Egyptian dungeon | dressed by the `art/world` Egyptian kit (sphinxes, columns, seats, stage, projector) | **done 2026-11-14**: `EGY_WallSeg` (+ `_Frieze`, `_Hive`) tile every wall box, `EGY_Pylon12` over the four doorways, `EGY_Colonnade` on the forecourt boxes (`ATL_EGY`, `preview/EGY_contact.png`) |
| §11.2 ballroom courtyard | the hotel's courtyard now has its landmark wings | bandstand, 6 tables (art/props has ballroom furniture and a chandelier) |
| §11.3 gym | | needed with Hollywood High |
| §10 | remaining sign/poster pages, emissive masks | optional |
| §10.2 ground tileables | | **covered** by `art/textures` (`T_GND_*`) |

Known loose ends:
- `SIGN_Graumans_Poster` is described on `LM_Graumans_ForecourtWall` but was never published as a card (the walls render without it).
- `zone.mjs` `shops()` leaves 0.5 m gaps between shops, so a frontage is 0.5 m narrower than its nominal width. `kitFront()` stretches bays to fit, so this is harmless.
- The Brown Derby's axis question (README) is resolved: its box turned to 24 × 30 m and it faces west (`'W'`) onto Vine.

## Integrating new pieces

1. Deliver to `art/buildings/` exactly as the SPEC says: FBX, metres, +Z up, −Y front, one material, triangulated. Use the footprint-centre-ground origin for buildings and front-plane-bottom-left for modules and sign cards. Add an `assets[]` entry to `manifest.json` with `id`, `kind`, `file`, `texture`, `nominal` [w, d, h] and `tris`, plus `attachments: [{asset, location, rotation?, scale?}]` for any sign cards (bottom-left of the card, in the landmark's Blender frame).
2. `python -I tools/buildprep.py`, then `hollywoodland-ue\Scripts\ImportBuildings.ps1` (about 2 min; prints `HL_ASSET meshes N/N`).
3. Place it in `tools/gen/zone.mjs`:
   - colliding landmark: `LM(box(x0, y0, x1, y1, h, C.landmark, 'Label'), face, 'LM_Id', { mirror })`;
   - non-colliding: `props.push(...landmarkDeco('LM_Id', x, y, face))`.

   `face` is the SPEC's front letter: `'S'` faces +y (buildings on the north side of the boulevard), `'N'` faces −y, `'W'` faces −x, `'E'` faces +x. The box should match `nominal`: `landmark()` stretches the mesh to the box and warns past 10%.
4. `node tools/gen-data.mjs` (must end in `validation ok`). Check visually with `-HLViews`; add a view to `Landmarks[]` in `HLWorldActor.cpp` if needed.
5. Small props (lamps, benches, fences...) go through the **prop** pipeline instead: put the GLB in `art/props` or `art/world`, register it in `tools/propprep.py`, then run `hollywoodland-ue\Scripts\ImportProps.ps1` and place it with `deco()` / `propBox()`.

## What is in git

Committed: `SPEC.md`, `README.md`, this file, `manifest.json`, `rodin_ledger.json`, `meshes/*.fbx` (the game meshes, 4 MB), `textures/`, `signs/`, `src/*.py` and the audit JSONs, `src/hub_*` (per-landmark `.blend` sources and review JSON), `src/concept_prompts/`, `src/fal_receipts/` (prompt records, no credentials), and the `preview/*_contact.png` / `KitProof_*` sheets.

Kept local (`.gitignore`; on the original workstation only, about 2.5 GB):
- `work/`: the agent's scratch;
- `concept/`: concept images;
- `src/rodin/`: the raw Rodin outputs the bakes came from;
- `meshes/*.glb`: Rodin shaded GLBs, 544 MB;
- the big kit sources `src/*_kit_textured.blend` and `src/kit_proof.blend` (up to 174 MB each, over GitHub's file limit);
- per-asset preview renders, and `*.blend1` backups.

`manifest.json` `files` still lists those paths. Nothing in the game needs them; they are only for re-baking the kit. If another machine needs them, zip them as a release asset or move them to Git LFS.

Credentials: every script reads `FAL_AI_API_KEY` / `RODIN_API_KEY` from the repo-root `.env` at runtime (gitignored). Nothing in this folder contains a key: the files were checked against the actual key values before commit.

## Prompt for the next agent

See the "Prompt" section of the 2026-11-14 handover reply, reproduced here:

```
You are continuing the building art for Hollywoodland (repo: C:\Users\theco\Projects\hollywoodland, UE 5.9 web game,
public MIT repo). Read art/buildings/HANDOVER.md first, then the SPEC.md sections it points to. The previous agent was
stopped for being slow and expensive; your job is to finish the "needed" rows of HANDOVER.md's table quickly and
cheaply, in this priority order:
  1. Hive wall kit: HV_WallSeg (8 x 18 x 16 m, tiles end to end), HV_WallEnd, HV_WallSeg_Outer (26 m high).
  2. LM_HollywoodHigh + LM_HollywoodHigh_Wing (mirrored), and the three aid stations (Receiving Hospital, Fire Station
     No. 27, Blessed Sacrament church), sized to their boxes in tools/gen/zone.mjs.
  3. The 10 backdrop landmarks of SPEC 7.3 (low budget: silhouette and colour, seen from 100-400 m).
  4. The missing street/back-lot props (HANDOVER table) and the Egyptian dungeon wall modules (SPEC 11.1).
  5. Ballroom (11.2) and gym (11.3) dressing.

Tools (both keys are in the repo-root .env; read them at runtime, never print, log or commit them, never put them in
filenames, URLs you log, or receipts):
- Concepts and textures: FLUX 3 Klein on fal.ai. Text-to-image: POST https://queue.fal.run/blackforestlabs/flux-3/klein/text-to-image,
  header "Authorization: Key $FAL_AI_API_KEY"; image-to-image for variations and projection views. It is fast (~5 s) and
  cheap: iterate here, not in Rodin. Working client: art/buildings/src/fal_image_client.py and tools/foliagegen.py
  (fal_image). Style: the painted, low-detail look of SPEC 1 (classic early World of Warcraft style, 1930 Hollywood).
  For a model reference, ask for a clean 3/4 view of one object on a plain white background.
- 3D: Hyper3D Rodin, https://api.hyper3d.com/api/v2/rodin (submit, multipart "images"), /status, /download, header
  "Authorization: Bearer $RODIN_API_KEY", tier Gen-2. Working client: tools/foliagegen.py rodin_model() (and
  art/buildings/src/rodin_client.py).
  * Props, hive kit, backdrops: full generation (geometry + texture, material PBR or Shaded, quality medium), from
    one FLUX reference image. Then fit and decimate it in Blender 5.3 (C:/Program Files/Blender Foundation/Blender
    5.3/blender.exe) to the SPEC 3 budget, or for props just drop the GLB in art/props or art/world and let
    tools/propprep.py simplify it.
  * Box-fitted buildings (Hollywood High, aid stations): block the mass in Blender by script to the exact box, then
    texture it by camera-projecting FLUX Klein views (front, side) and baking to one atlas (SPEC 15 fallback, which the
    previous agent used for half the landmarks). Use Rodin texture-only on a building only if projection clearly fails.
Credit discipline: budget 20 Rodin credits for the whole job. One Rodin job per asset plus at most one retry; if the
retry fails, fall back to projection or simplify the asset; never loop. Record every job (asset, task uuid, credits) in
art/buildings/rodin_ledger.json; do not store subscription keys or job tokens. Stop and report if you reach the budget.
Time discipline: no review renders beyond one contact sheet per batch; do not re-bake accepted assets; batch similar
assets; a backdrop should take minutes, not hours.

Delivery: follow SPEC 2 (metres, +Z up, -Y front, origins), 3 (budgets) and 12 (manifest). Add each asset to
art/buildings/manifest.json, then integrate it yourself as HANDOVER.md "Integrating new pieces" describes:
python -I tools/buildprep.py; hollywoodland-ue\Scripts\ImportBuildings.ps1; place it in tools/gen/zone.mjs with LM() or
landmarkDeco(); node tools/gen-data.mjs (must say "validation ok"); check with
hollywoodland-ue\Scripts\RunNative.ps1 -HLAuto=veteran -HLViews -HLViewSet=landmarks (add views as needed) and look at
the screenshots. Do not edit other game code. Do not run imports while a web build (Scripts\BuildWeb.ps1) is running.
Keep the web package in mind: no texture over 2048, prefer shared atlases.
Commit after each finished batch, staging explicit paths only (never git add -u / -A), with
git -c user.name=coeurnix -c user.email=coeurnix@users.noreply.github.com commit. Before committing anything under
art/buildings, scan the staged files for the key values from .env. Update the table in HANDOVER.md as rows are done.
```
