# Building art production

**Work order in progress.** Import only the assets indexed in manifest.json.

Published: 163 meshes, 29 texture sets, 3 sign pages. Reviewed storefront families: BRK, SPN, DECO, MSN. The published unique meshes total 48,590 LOD0 triangles.
Rodin ledger: 32.5 credits, reconciled against account usage at 2026-11-14T05:24:35.470442+00:00. Submission reservations for failed jobs were corrected to actual billed usage.

## Phase status

1. Pipeline proof published: ground and upper BRK bays, Drugstore, correct-lettering cards and FBX round-trip checks.
2. Storefront kit complete: all108 family modules, twelve occupation-dressing meshes, seven roof props, matching wall/roof tileables and the120m proof. See preview/Phase2_contact.png and KitProof_street.png / KitProof_30m.png.
3. Hub landmarks: 16 of16 required meshes published after texture/box review. Fifteen hub sign cards are published on two additional pages; attachment transforms are resolved for published landmarks. The phase is complete.
4–8. Street kit, dungeon, remaining landmarks/interiors, hive kit and backdrops remain outstanding. Optional shop interior is also pending.

## Integration

Assets use metres, +Z up and −Y front. Building origins are footprint-centre-ground; modules are front-plane-bottom-left. Every delivered FBX has one material and triangular faces. Bounds and triangle counts were measured by importing the actual FBX.
The BRK release replaces the two earlier proof bays and their atlas: reimport both the meshes and texture together. Use the manifest Drugstore attachment transforms for its two sign cards. Sign rectangles are normalized from the top left.
The completed neutral atlas prioritizes legible notice lettering. Reimport all nineteen RF_/DR_ meshes and T_ATL_NEUTRAL.png together; their final UV packing supersedes the earlier seven-prop checkpoint.
Landmark attachments use the sign card bottom-left origin. Apply location, rotation in degrees, then local-axis scale from attachments; inherit the parent building transform. Sign pages are excluded from the texture-set budget. The Brown Derby card follows the hat silhouette and uses no transparency.
Each corner has a nominal 1 m wrap length: 0.5 m on each perpendicular face. Use recipe corner_front_m and corner_return_m rather than treating its front span as 1 m.
Roof/wall quads use the separately specified tileables; facade modules share their family atlas. Tile sizes in metres are recorded under tileables. No game code/data or SPEC.md was edited.

## Pipeline and quality

Initial concepts were generated with built-in imagegen. On2026-11-13 the user explicitly authorized fal.ai blackforestlabs/flux-3/klein/text-to-image and image-to-image as an alternative provider. Its first style test completed in4.96seconds and passed palette/painted-surface review; remaining concepts/projection views may use this route. This is a user-approved exception to SPEC9.1; SPEC.md remains unchanged. Original prompts/provider receipts are retained in the manifest and src/concept_prompts/ or src/fal_receipts/. Blender5.2 scripts author delivered meshes. Rodin remains texture-only for architecture.
Shaded was selected after comparing the first All outputs against PBR: clearer painted top-light and softer glass suit the simple lit game shader. Colour is baked with Cycles Selected-to-Active emission onto authored UVs. No Rodin geometry is delivered.
The combined BRK atlas trial was rejected. The released family uses Option B: independent representative textures are baked into disjoint regions, with repeated doors/windows/trim sharing UVs. Awnings, crests and damage no longer overwrite storefront regions.
BRK upper bays use concept projection after two failed Rodin jobs. Parapets use projection after two upload timeouts. Crest A uses projection after both texture attempts failed material/silhouette review. Neutral edge spill was painted as warm stone in Blender. These methods are marked per asset. Other representative outputs passed visual review.
Texture proxies may be tessellated to improve texture-service reliability. The final meshes retain their authored minimal topology. Original texture inputs and returned geometry/UV signatures are recorded in src/. Source Rodin outputs needed for rebuilding are retained in src/rodin/.
Exposed crests/parapets receive authored 0.25 m thickness and finished backs. Spanish parapets have curved silhouettes and painted red-tile coping. Bake misses are filled from adjacent painted stucco; back UVs stack the same ornament as the front.
DECO CrestB uses projection after two smeared Rodin results. Spanish and Mission representative windows/awnings passed their second-reference reviews. Neutral soot/cracks were graded to charcoal in Blender; sandbag contact rings were broadened while preserving UVs. The correctly generated mayor notice receives sixfold UV priority.
Eight phase-three landmarks received Rodin Shaded colour baked back to authored geometry/UVs. Eight use SPEC15 generated-view camera projection after two server-failed texture jobs each. The per-asset rodin method and source_checks/export_audit paths record which. Egyptian plaster and Chinese copper/tar roof roles were hand-fixed after multi-view review.
All runtime masters are 2048² 8-bit PNG with an sRGB chunk. Last published secret scan passed. Audits are src/BRK_kit_export_audit.json and src/BRK_option_b_source_checks.json; phase-one evidence is in src/phase1_checks.json and src/uv_density_audit.json.
The released meshes have no zero-area faces, loose vertices or edges shared by more than two faces. Open wall-facing boundaries are intentional. The132 published FBXs passed independent round-trip checks, including shared-edge orientation. The120m street proof passes its module-span audit. Full-order certification and the dressed-boulevard estimate remain pending.

## Specification questions and proposals

- §3 forbids non-manifold edges but also requires single-quad walls and removal of hidden backs/undersides. Interpret intentional boundary edges as permitted; reject branching edges and unintended holes.
- §6.2 floor counts leave parapets of 1.5, 1.0, 0.5 and 0.0 m at building heights 6, 9, 12 and 15 m. The last two conflict with the allowed 0.5–2.0 scaling of a 1.5 m parapet. Proposed integration exception: thinner/omitted parapets at 12/15 m. Exported module heights remain exact.
- zone.mjs shops() subtracts 0.5 m from frontage widths. The Drugstore is therefore 17.5 m in current data versus the specified 18 m. Its delivered primary size remains exactly 18 × 36 × 6 m. The integrator must resolve this mismatch; no collision data was changed. Other shop frontages need the same review.
- Where generated concepts add optional awnings/roof trim to a larger assembly, only the requested module is modelled. Awning geometry is separate. Roof and wall quads follow the required nominal geometry and tileable materials.

- Brown Derby has the prescribed local30×24m footprint with front−Y; turning it90degrees toward Vine swaps its world extents. The spec also calls w/d world east–west/north–south. Integrator must reconcile this axis ambiguity before collision placement; exported local nominal dimensions remain exact.
- Pantages frozen roof UV density is24px/m, slightly below the preferred25px/m roof target; its street faces exceed50px/m. No texture or triangle-budget exception is used.

## Rebuilding

src/rebuild_phase1.py --blender <executable> rebuilds the original checkpoint without API calls; it republishes only phase one and should not replace a later manifest.
Family pipeline: build_brk_kit.py -- FAMILY, prepare_brk_masters.py -- FAMILY, bake_brk_option_b.py -- FAMILY, render_brk_candidate.py -- FAMILY, contact_brk_candidate.py FAMILY, then publish_family.py FAMILY after review. Despite the historical filenames, these accept BRK, SPN, DECO and MSN. Retained accepted sources avoid repeat paid calls.
Family finish steps: SPN uses spn_parapet_shape.py before baking, then finish_facade_geometry.py -- SPN and spn_atlas_cleanup.py. DECO/MSN use deco_msn_build.py for source silhouettes, deco_msn_finish.py after baking, then finish_facade_geometry.py. BRK uses finish_facade_geometry.py; repair_brk_back_uvs.py is a one-time migration already applied to the retained source.
After any atlas paint cleanup, run sync_family_textures.py -- FAMILY before rendering/export publication. This explicitly refreshes packed Blender image bytes and embedded GLB images from the final PNG; packing an already packed image can retain stale bytes.
Final facade steps also include finish_awnings.py for canopy undersides, and spn_plain70_finish.py for the Spanish cap band. Neutral rebuild: roof_clutter_build.py, dressing_prepare_concepts.py, dressing_build.py, prepare_neutral_atlas.py, bake_neutral_atlas.py, finish_neutral.py, sync_family_textures.py -- NEUTRAL, render_brk_candidate.py -- NEUTRAL. Retained Rodin sources avoid repeat service calls. build_kit_proof.py imports published manifest FBXs only.
Parallel workers prepared roof clutter and occupation dressing. Their service usage limit interrupted final integration after saving source files; the root coordinator continues from those sources. On 2026-11-13 the user removed shared-ledger synchronization as a queue constraint. Concurrent Rodin jobs now use isolated per-job receipts in work/rodin_receipts, merged and reconciled after the legacy writer exits.
API details were checked in the official Generate Texture, Check Status, Download Results and Quick Start documentation on 2026-11-13. The key is read from the root .env only at runtime; no credentials are stored or printed. The client retains the two-attempt quality loop. Concurrent jobs write separate receipts; capacity rejections are deferred without counting a paid generation. Final credits are reconciled against account usage.

## Published triangle counts

| Asset | LOD0 triangles |
|---|---:|
| LM_Drugstore | 58 |
| SIGN_Drugstore | 2 |
| SIGN_SodaFountain | 2 |
| SIGN_Auction | 2 |
| SIGN_Haberdashery | 2 |
| BRK_G_Bay4A | 90 |
| BRK_G_Bay4B | 90 |
| BRK_G_Bay6A | 90 |
| BRK_G_Bay6B | 90 |
| BRK_G_Bay8A | 90 |
| BRK_G_Bay8B | 90 |
| BRK_G_Entry4 | 76 |
| BRK_U_Bay2A | 6 |
| BRK_U_Bay2B | 12 |
| BRK_U_Bay3A | 6 |
| BRK_U_Bay3B | 12 |
| BRK_U_Bay4A | 6 |
| BRK_U_Bay4B | 12 |
| BRK_Pilaster1_G | 2 |
| BRK_Corner_G | 4 |
| BRK_Pilaster1_U | 2 |
| BRK_Corner_U | 4 |
| BRK_Parapet2 | 26 |
| BRK_Parapet4 | 26 |
| BRK_CrestA | 42 |
| BRK_CrestB | 42 |
| BRK_Awning4 | 18 |
| BRK_Awning6 | 18 |
| BRK_Wall | 2 |
| BRK_Roof | 2 |
| BRK_Damaged_G_Bay6 | 94 |
| BRK_Damaged_U_Bay3 | 10 |
| DECO_G_Bay4A | 100 |
| DECO_G_Bay4B | 100 |
| DECO_G_Bay6A | 100 |
| DECO_G_Bay6B | 100 |
| DECO_G_Bay8A | 100 |
| DECO_G_Bay8B | 100 |
| DECO_G_Entry4 | 86 |
| DECO_U_Bay2A | 6 |
| DECO_U_Bay2B | 12 |
| DECO_U_Bay3A | 6 |
| DECO_U_Bay3B | 12 |
| DECO_U_Bay4A | 6 |
| DECO_U_Bay4B | 12 |
| DECO_Pilaster1_G | 2 |
| DECO_Corner_G | 4 |
| DECO_Pilaster1_U | 2 |
| DECO_Corner_U | 4 |
| DECO_Parapet2 | 26 |
| DECO_Parapet4 | 26 |
| DECO_CrestA | 42 |
| DECO_CrestB | 42 |
| DECO_Awning4 | 18 |
| DECO_Awning6 | 18 |
| DECO_Wall | 2 |
| DECO_Roof | 2 |
| DECO_Damaged_G_Bay6 | 94 |
| DECO_Damaged_U_Bay3 | 10 |
| MSN_G_Bay4A | 100 |
| MSN_G_Bay4B | 100 |
| MSN_G_Bay6A | 100 |
| MSN_G_Bay6B | 100 |
| MSN_G_Bay8A | 100 |
| MSN_G_Bay8B | 100 |
| MSN_G_Entry4 | 86 |
| MSN_U_Bay2A | 6 |
| MSN_U_Bay2B | 12 |
| MSN_U_Bay3A | 6 |
| MSN_U_Bay3B | 12 |
| MSN_U_Bay4A | 6 |
| MSN_U_Bay4B | 12 |
| MSN_Pilaster1_G | 2 |
| MSN_Corner_G | 4 |
| MSN_Pilaster1_U | 2 |
| MSN_Corner_U | 4 |
| MSN_Parapet2 | 22 |
| MSN_Parapet4 | 22 |
| MSN_CrestA | 42 |
| MSN_CrestB | 58 |
| MSN_Awning4 | 18 |
| MSN_Awning6 | 18 |
| MSN_Wall | 2 |
| MSN_Roof | 2 |
| MSN_Damaged_G_Bay6 | 94 |
| MSN_Damaged_U_Bay3 | 10 |
| SPN_G_Bay4A | 90 |
| SPN_G_Bay4B | 90 |
| SPN_G_Bay6A | 90 |
| SPN_G_Bay6B | 90 |
| SPN_G_Bay8A | 90 |
| SPN_G_Bay8B | 90 |
| SPN_G_Entry4 | 76 |
| SPN_U_Bay2A | 254 |
| SPN_U_Bay2B | 262 |
| SPN_U_Bay3A | 254 |
| SPN_U_Bay3B | 262 |
| SPN_U_Bay4A | 254 |
| SPN_U_Bay4B | 262 |
| SPN_Pilaster1_G | 2 |
| SPN_Corner_G | 4 |
| SPN_Pilaster1_U | 2 |
| SPN_Corner_U | 4 |
| SPN_Parapet2 | 38 |
| SPN_Parapet4 | 70 |
| SPN_CrestA | 50 |
| SPN_CrestB | 42 |
| SPN_Awning4 | 18 |
| SPN_Awning6 | 18 |
| SPN_Wall | 2 |
| SPN_Roof | 2 |
| SPN_Damaged_G_Bay6 | 94 |
| SPN_Damaged_U_Bay3 | 258 |
| RF_WaterTank | 1218 |
| RF_Billboard8x4 | 980 |
| RF_Billboard12x5 | 1274 |
| RF_StairBulkhead | 262 |
| RF_ChimneyBrick | 118 |
| RF_VentMetal | 262 |
| RF_Skylight | 434 |
| DR_BoardedWindow15 | 108 |
| DR_BoardedWindow23 | 174 |
| DR_SandbagStack3 | 440 |
| DR_SandbagDoorway4 | 902 |
| DR_FallenAwning | 48 |
| DR_SootA | 13 |
| DR_SootB | 11 |
| DR_Crack | 17 |
| DR_MayorNotice | 2 |
| DR_Rubble2 | 128 |
| DR_Rubble4 | 304 |
| DR_BrokenGlass | 26 |
| LM_Roosevelt | 9656 |
| LM_Pantages | 850 |
| LM_Taft | 3808 |
| LM_Egyptian_Gateway | 96 |
| LM_HollywoodHotel_CourtWall | 732 |
| LM_FirstNational | 1405 |
| LM_BrownDerby | 408 |
| LM_Egyptian | 1124 |
| LM_Egyptian_Wing | 1086 |
| LM_Graumans | 1924 |
| LM_RedCarStop | 964 |
| LM_Kiosk_Auction | 190 |
| LM_Graumans_ForecourtWall | 190 |
| LM_HollywoodHotel_Wing | 4300 |
| LM_HollywoodHotel | 8747 |
| LM_PoliceStation | 480 |
| SIGN_Pantages | 2 |
| SIGN_Graumans_Blade | 2 |
| SIGN_Egyptian | 2 |
| SIGN_Egyptian_Gate | 2 |
| SIGN_Roosevelt | 2 |
| SIGN_HollywoodHotel | 2 |
| SIGN_Graumans_Marquee | 2 |
| SIGN_Pantages_Marquee | 2 |
| SIGN_Taft | 2 |
| SIGN_FirstNational | 2 |
| SIGN_Police | 2 |
| SIGN_PacificElectric | 2 |
| SIGN_Egyptian_Marquee | 2 |
| SIGN_BrownDerby | 13 |
| SIGN_RedCarTimetable | 2 |

The full dressed-boulevard triangle estimate will be calculated from completed assets and placement counts. The current library total above counts each released mesh once.
