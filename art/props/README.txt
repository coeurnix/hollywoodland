Unreal-ready props, optimized with glTF Transform 4.5.1 (66 models, one folder).

THE TWO SETS
- 01-... to 13-...  The 14 hand-picked Hollywood props from combined-models.zip.
- Everything else   52 models from Rodin Batch, renamed by what they are. Eleven source images
                    had been generated two to four times; only the best take of each is kept here
                    (judged by texture/lettering clarity and clean geometry). The 31 other takes
                    are outside the game folder, in outputs/alternate-takes.

WHAT CHANGED
- Geometry
    14-prop set: duplicate data removed, near-lossless simplify (max error ~2 mm).
    Batch set:   500,000 triangles each -> a web-friendly budget per type:
                 4k track pieces, 6-8k small props and signs, 12-15k lamps/hive pieces,
                 20-25k vehicles and fire escapes, 30k theater facade. The normal map keeps
                 the fine surface detail. Simplification is UV-aware, so textures don't smear.
- Textures: all 2048x2048 -> 1024x1024 JPEG (power of two, so Unreal mip-maps and streams them).
    Batch set keeps full PBR: base colour, metallic/roughness and normal map.
- Materials
    14-prop set: the painted texture was in the EMISSIVE slot with a black base colour (the props
                 would glow flat and ignore lighting). Now a normal lit, matte material.
    Batch set:   switched from two-sided to one-sided (cheaper to render; Rodin meshes are closed).
- Pivots baked into the mesh (glTF +Z is the front):
    floor (bottom centre):     almost everything
    ceiling (top centre):      03 ceiling hive cluster, 13 under-canopy marquee, hive-dripping-membrane
    wall (centre of the back): flat signs (08, 09, 10b, 11, 12, sign-starlight-drug)
    bracket signs:             10a blade tailor, sign-sunset-haberdashery, sign-silver-screen-cafe are
                               turned so the wall bracket is at the back, pivot on the wall side
- Names: unique SM_/M_/T_ names per model, so importing everything into one Content folder never
  makes two models share or overwrite a material or texture.
- No Draco, Meshopt, KTX2 or WebP: Unreal's glTF importer doesn't read those and compresses
  textures and meshes itself when it cooks the game.
- The validator notes "no tangents" on the batch models (same as the Rodin originals). Unreal
  generates tangents on import, so normal maps work.

IMPORT TIPS (UNREAL 5)
- Drag the .glb files into the Content Browser. Keep Nanite off for a web/mobile target.
- Static Mesh editor > LOD Settings: LOD Group "SmallProp" / "LargeProp" for automatic LODs.
- Add simple collision (Collision > Add Box / 10DOP) on props players bump into.
- Scale is real-world and baked into the mesh (1 glTF unit = 1 m; Unreal imports it as 100 cm),
  so place everything at scale 1.0. Rodin made every model about 1.9 m across; 35 were resized:
    02 red car             13 m long (the Pacific Electric streetcar, matching the 13 m placeholder;
                           Rodin's proportions make it ~3.9 m wide and ~6 m tall to the trolley pole)
    cars and sedans        3.8-4.9 m long
    vans and trucks        4.6-5.5 m long
    theater facade         9 m tall
    fire escapes           7.5-10 m tall, stairway landing 6 m, wooden staircase 4 m
    street lamp            4.5 m tall, trolley wire pole 7.5 m, fallen utility pole 6 m long
    shed 4 m, triage tent 5 m, stone wall 4 m long, sandbag wall 1.2 m high
    streetcar track        x3.5 (slab about 2.6-3.3 m wide, so the rails sit near standard gauge)
    US mailbox 1.25 m, coffee tins 0.2 m, film cans 0.5 m across
  Kept as made (already plausible, or no real-world size): signs, police call boxes, arc lamps,
  scrap piles, chandelier, rock, all hive pieces, and the other 14-prop set items.
  If something still reads wrong in the level, scaling the actor is fine; it doesn't cost anything.
