Hollywoodland world-piece models (34 models)

PIPELINE
  1. Reference image: FLUX 3 (fal.ai blackforestlabs/flux-3/text-to-image, 1.5k, white background).
     Prompts are in prompts.json.
  2. 3D model: Hyper3D Rodin Gen-2, PBR material, GLB (500k triangles, about 30 MB each, 1 GB total).
  3. Compression: Blender 5.1 collapse-decimate (UV-preserving) to a per-category triangle budget, then
     glTF Transform 4: dedup, flatten, weld, real-world scale with the pivot at bottom centre, textures resized to 1024 px JPEG,
     prune. Total 41 MB.
     Budgets: red car 40k, mounts and wrecks 25k, Egyptian interior 20k, sign letters 15k, interactables 8k.
  4. Previews: Blender EEVEE, 5 views per model (front-left, front-right, side, back, top).

CONVENTIONS (same as art/props in the repo)
  1 glTF unit = 1 m, Y up, front is +Z, pivot at the bottom centre. Unique SM_/M_/T_ names per model.
  No Draco, Meshopt or KTX2, because Unreal's glTF importer can't read them. Materials are one-sided. No tangents (Unreal generates them).

MODELS  (triangles, file size, width x height x length)
  red-car             redcar-pacific-electric           39980 tris   1918 KB  2.89 x 5.24 x 13 m
  mounts              mount-horse                       24996 tris   1258 KB  0.62 x 1.67 x 2.4 m
  mounts              mount-motorcycle-indian-scout     24989 tris   1542 KB  0.4 x 0.99 x 2.1 m
  mounts              mount-ford-model-a                24984 tris   1337 KB  1.53 x 1.82 x 4 m
  mounts              mount-ford-model-a-police         24990 tris   1382 KB  1.72 x 1.73 x 4 m
  wrecks              wreck-newsreel-truck              24988 tris   1501 KB  2.64 x 2.55 x 5.5 m
  wrecks              wreck-sedan-overturned            24997 tris   1457 KB  1.66 x 1.77 x 4.5 m
  wrecks              wreck-model-a-burned              24989 tris   1484 KB  1.88 x 1.84 x 4 m
  wrecks              wreck-delivery-truck-hive         24986 tris   1517 KB  2.11 x 1.84 x 5 m
  interactables       int-film-can-stack                 7998 tris    844 KB  0.56 x 0.5 x 0.36 m
  interactables       int-film-reel                      7992 tris   1145 KB  0.4 x 0.36 x 0.21 m
  interactables       int-coffee-tin                     7998 tris   1026 KB  0.16 x 0.2 x 0.16 m
  interactables       int-arc-lamp                       7998 tris    932 KB  0.8 x 2.2 x 1.22 m
  interactables       int-police-call-box                7995 tris    872 KB  0.28 x 1.6 x 0.28 m
  interactables       int-us-mailbox                     7996 tris    714 KB  0.59 x 1.25 x 0.51 m
  interactables       int-scrap-pile                     7998 tris    847 KB  1.2 x 0.34 x 0.94 m
  interactables       int-captured-usher-cocoon          7996 tris   1028 KB  0.95 x 2 x 0.64 m
  egyptian-interior   egy-lotus-column                  19996 tris   1246 KB  1.49 x 7 x 1.5 m
  egyptian-interior   egy-pharaoh-statue                19986 tris   1338 KB  1.48 x 3.5 x 2.14 m
  egyptian-interior   egy-sphinx                        19980 tris   1173 KB  1.02 x 1.74 x 3 m
  egyptian-interior   egy-ticket-booth                  19982 tris   1176 KB  1.86 x 3 x 1.77 m
  egyptian-interior   egy-seat-row                      19995 tris   1170 KB  4 x 1.03 x 0.73 m
  egyptian-interior   egy-projector                     19990 tris   1328 KB  0.53 x 1.9 x 1.02 m
  egyptian-interior   egy-proscenium-stage              19994 tris   1183 KB  12 x 7.3 x 6.29 m
  egyptian-interior   egy-hieroglyph-wall               19993 tris   1468 KB  4 x 3.29 x 1.28 m
  egyptian-interior   egy-pipe-organ-console            19990 tris   1271 KB  3.11 x 1.8 x 1.81 m
  hollywoodland-sign  sign-letter-H                     14992 tris   1384 KB  8.08 x 13.4 x 6.31 m
  hollywoodland-sign  sign-letter-O                     14994 tris   1229 KB  7.52 x 13.4 x 4.82 m
  hollywoodland-sign  sign-letter-L                     14987 tris   1372 KB  7.03 x 13.4 x 2.61 m
  hollywoodland-sign  sign-letter-Y                     14988 tris   1348 KB  7.62 x 13.4 x 7.54 m
  hollywoodland-sign  sign-letter-W                     14994 tris   1147 KB  11.81 x 13.4 x 7.82 m
  hollywoodland-sign  sign-letter-D                     14996 tris   1253 KB  9.03 x 13.4 x 5.6 m
  hollywoodland-sign  sign-letter-A                     14992 tris   1280 KB  9.57 x 13.4 x 2.59 m
  hollywoodland-sign  sign-letter-N                     14993 tris   1126 KB  7.77 x 13.4 x 1.75 m

NOTES
  - Red car: Rodin built a short, tall car. It is scaled to 13 m long (the placeholder), which makes it 5.2 m tall to the
    trolley pole and 2.9 m wide. In Unreal, scale Z down to about 0.75 if it looks too tall.
  - Mounts have no rider and are in a neutral standing pose. The horse is not rigged.
  - HOLLYWOODLAND sign: one model per unique letter (H O L Y W D A N). Place them in this order:
    H O L L Y W O O D L A N D, about 13.4 m tall and roughly 9-12 m apart. Letters O and D read less cleanly than the others.
  - Egyptian dungeon interior is a kit of modular pieces (columns, hieroglyph wall, pharaoh, sphinx, ticket booth, seat row,
    proscenium stage, projector, organ console), not whole rooms. The repo's zone generator still builds the room shells.
  - Painted lettering, such as on the coffee tin and the film cans, comes out garbled after the 3D step.

IN THE REPO
  art/world/models/<category>/*.glb, prompts.json, this README. previews/contact-sheet.png shows the front-left
  view of every model. The full 5-view EEVEE previews (170 PNGs, 88 MB) were left out of git to keep the repo small.
  Not imported into Unreal yet; the props pipeline (tools/propprep.py, Scripts/ImportProps.ps1) is the model to follow.
