# Building Art Order: Exteriors, Street Kit, Interiors

This is the work order for the agent producing Hollywoodland's architecture. The integrator imports the delivery mechanically from `manifest.json` (§12), so names, units, origins and sizes must match exactly. Each requirement is either a hard requirement ("must") or a stated preference. If a requirement can't be met, follow §15 rather than improvising, and record the deviation in the report.

**Context.** Hollywoodland is a single-player, simulated MMORPG set in Hollywood in autumn 1930, under an alien insect occupation. It is built in Unreal Engine 5.9 and ships as a WebGPU/Wasm browser build. The playable slice is Hollywood Boulevard (levels 1–12, about 1080 × 730 m inside a hive wall) and the Egyptian Theatre dungeon.

Today every building is a coloured box. The boxes stay: they are the collision, line of sight and camera collision. Your meshes replace what the boxes look like. A building may never be bigger than its box (§4).

The camera is a third-person MMO camera, 3–40 m from the player at 0–60° pitch. **Roofs are seen constantly; backs and sides are seen often.** The character art (`art/characters`) is already in the game, so match its look (§1).

**Pipeline (mandatory for architecture).**
1. Generate a concept image of the asset.
2. Model an efficient low-poly mesh in Blender that follows the concept, with clean UVs.
3. Texture it with Hyper3D Rodin's texture-only endpoint (§9), using the concept image as the reference.

Do **not** use Rodin's full 3D generation for buildings; its meshes are far too heavy. Small organic props (§8.4) are the only exception.

---

## 1. Visual style

- **Look:** a classic-MMO look, matching the early World of Warcraft era. That means chunky, readable, slightly exaggerated shapes with hand-painted textures. The character art uses the same rules.
  - Proportions are pushed:
    - Cornices, parapets, marquees and signs are about 1.3× real thickness and size.
    - Window frames and sills are bold.
    - Silhouettes read at 100 m.
  - Prefer a few big readable forms over many small accurate ones.
- **Texturing:**
  - Hand-painted albedo, with light painted from above (warmer at the top, darker and cooler at the base).
  - Painted ambient occlusion in corners, under cornices and in window reveals.
  - Visible but restrained brush texture, and grime streaks under sills.
  - The game material is a simple lit shader: base colour plus a small emissive "glow". Painted shading carries the form, and normal/roughness/metal maps are optional and may be ignored.
- **Palette:** a warm golden-hour base: cream and ivory stucco, sand, terracotta roof tile, ochre, oxblood, bottle green, slate, sepia brick, and weathered teal and verdigris on metal. Saturation is highest on accents (marquee bulbs, awnings, signs, tile).
- **Architectural vocabulary (Hollywood, 1920–1930):**
  - Spanish Colonial Revival: white stucco, red tile roofs, wrought iron, arched windows.
  - Mission Revival.
  - Art Deco and Zigzag Moderne: stepped parapets, chevrons, sunbursts, vertical piers, terracotta panels.
  - Renaissance Revival: brick office blocks with stone trim and heavy cornices.
  - Egyptian Revival: the Egyptian Theatre.
  - Chinese "pagoda" fantasy: Grauman's.
  - Plain 1910s two-storey brick commercial blocks for filler.
- **The occupation:** this is a city under siege, not a ruin. Most buildings are intact with these touches:
  - **Most buildings:** boarded or taped windows on some floors, sandbags at a few doors, soot above some windows, faded paint and hand-painted notices.
  - **About 1 in 6 kit pieces:** a damaged variant (§6.3).
  - **Hive resin:** purple-black glossy growth with amber veins. It appears only in the dedicated hive kit (§8.4) and on deliberately "infested" variants, never as random noise.
- **Signage:**
  - Signs are period-correct: painted wall signs, neon tubes (1930 is early neon), bulb-chaser marquees and gilt lettering on glass.
  - Real 1930 business names are allowed and wanted on landmarks: Grauman's, Max Factor, Brown Derby, Pantages, Roosevelt, Security First National Bank.
  - Never use a brand, logo or product that did not exist by 1930. Never use modern typography (no sans-serif geometric fonts that postdate 1930 as a "look").
- **No people, no vehicles baked into building textures.** No sky or ground in textures (no backgrounds).

## 2. Units, axes, origins (hard rules)

- **Units:** metres. 1 Blender unit = 1 m. Apply all transforms before export (scale 1, rotation 0).
- **Up:** +Z. **Front** (the street face) points to **Blender −Y**, which is what Blender's Front view looks at. The integrator rotates each building to face its street.
- **Whole buildings (landmarks, §7):**
  - The origin is at the **centre of the footprint at ground level** (z = 0 is the pavement).
  - The footprint spans x ∈ [−w/2, w/2] and y ∈ [−d/2, d/2], with the front face at y = −d/2.
- **Kit modules (§6):**
  - The origin is at the **bottom-left corner of the front plane**, seen from the street: x = 0 is the module's left edge, y = 0 is the wall's front plane, z = 0 is the module's floor level.
  - The module extends +X to its nominal width and +Y (into the building) at most 1.0 m.
  - Projections in front of the wall plane (sills, cornices, awnings, signs) go to −Y, within the limits in §4.
- **Props:** the origin is at ground level under the object's centre, with the front to −Y.
- **Interior pieces:** the same as kit modules, but the "front" is the room-facing side.
- Nominal sizes are exact. A 6 m bay is 6.000 m wide between its left and right edges, so modules butt together without gaps or overlaps. Snap all module edges to a 0.25 m grid.

## 3. Hard performance budgets

The web build must hold 60 fps on ordinary laptops with the whole boulevard visible. Triangle counts are for LOD0 after triangulation. The integrator generates LOD1 (50%) and LOD2 (20%), so keep topology clean enough to decimate.

| Asset class | Triangles (LOD0) | Texture (master) |
|---|---|---|
| Facade kit module (bay, floor, parapet, corner, pilaster) | ≤ 600 | its family atlas or own 2048² (§9.4) |
| Side/back wall panel, roof cap | ≤ 60 | tileable 2048² per family |
| Hero landmark (Grauman's, the Egyptian, Roosevelt, Pantages, Taft) | ≤ 20,000 | 1–2 × 2048² |
| Other landmark (§7.1) | ≤ 10,000 | 1 × 2048² |
| Backdrop landmark (§7.3, beyond the hive wall, seen from 100 m+) | ≤ 3,000 | 1 × 2048² (may be shown at 1K) |
| Street prop, small (call box, hydrant, mailbox, sign post) | ≤ 300 | shared prop atlas 2048² |
| Street prop, large (street lamp, newsstand, car, truck, billboard) | ≤ 1,500 | shared prop atlas or own 2048² |
| Interior module (wall panel, column, doorway frame) | ≤ 800 | interior family atlas 2048² |
| Interior furniture (seat row 2 m, table, projector, bandstand) | ≤ 1,200 | interior family atlas |
| Sign card (flat sign, poster, marquee letters board) | ≤ 50 | sign atlas pages 2048² (§10) |

- **Textures:** masters are 2048×2048 PNG (sRGB, 8-bit). Never 4K in the delivery; Rodin may return 4K, so downsample it. Never smaller than 1024². The integrator chooses the in-game resolution per asset (often 1K) to fit the web download, so the masters must stay clean when halved.
- **Texture sets:** aim for **≤ 45 unique 2048² texture sets in total** across the whole order (excluding sign pages and the tileable ground set). Shared atlases are how you get there. Count them in the report.
- **Materials:** exactly **one material slot per mesh**, named `M_<asset id>` (or `M_<atlas id>` when the mesh uses a shared atlas). There is no transparency except alpha-tested (masked) cards for wrought iron, neon tubes and foliage, and only where needed. Use no translucency and no glass refraction: windows are painted (dark glass with painted reflections).
- **Mesh hygiene:**
  - Triangulate on export.
  - No n-gons, zero-area faces, non-manifold edges, flipped normals, loose vertices or interior faces.
  - Remove faces nobody can see: undersides, faces against the ground, faces inside the wall plane, and back faces of kit modules that sit against the building box.
  - Hard edges at 90° architectural corners; smooth only on curved forms.
- **No skeletal anything.** Static meshes only.
- **Whole-scene sanity target:** the boulevard fully dressed should come to about 500,000 triangles or fewer in total. Report your estimate (§13).

## 4. Fitting the collision boxes (hard rules)

Each building is placed in an axis-aligned box `w × d × h` (metres) from the game data. The player collides with the box, not your mesh.

- The **main mass** must fill the box: the front face lies on the box's front plane (±0.15 m), and the overall height reaches `h` (±0.3 m, roof props excluded).
  - If the mesh is noticeably smaller than its box, players bump into invisible walls.
  - If it is bigger, players see their character clip into a building.
- **Permitted protrusions beyond the box** (non-colliding, purely visual):
  - **Toward the street:** awnings, marquees, blade signs and cornices, up to 1.5 m. The sidewalk is 4 m wide, and these must clear 2.6 m above the pavement.
  - **On the roof:** water tanks, signs, chimneys, billboards and spires. These can go above `h` freely.
  - **At the base:** steps, kerb-level planters and similar may protrude 0.3 m.
- **Recesses** (entrances, a forecourt, loggias) may cut into the box by up to 2 m. Anything deeper needs the integrator to change the collision, so list it in the manifest as `recess` with its rectangle.
- Landmark sizes are fixed (§7). A kit-assembled building can be any size and is fitted by the assembly rules (§6.2).

## 5. Deliverable list at a glance

1. **Storefront kit:** four style families of facade modules plus walls and roofs (§6), so the integrator can procedurally build the 60+ unnamed commercial buildings that line the boulevard. **Highest value.**
2. **Landmarks:** 17 in or by the playable streets plus 10 backdrop buildings (§7).
3. **Street kit** (§8): lamps, poles, signs, kerbs, the Pacific Electric red car, period cars and trucks, wrecks, rubble, sandbags, barricades, and the hive kit.
4. **Interiors** (§11): the Egyptian Theatre dungeon kit (mandatory), the Hollywood Hotel ballroom courtyard, and the Hollywood High gym.
5. **Tileable ground and wall materials** (§10.2): road, tram rails, sidewalk, brick, stucco, roof, carpet, marble, parquet, gym boards, blighted earth.
6. **Sign and poster pages** (§10.1).

## 6. The storefront kit (procedural buildings)

About 60 commercial buildings line the boulevard. They are unlabelled boxes made by `shops()` in `tools/gen/zone.mjs`, and they all share these sizes:

- **Frontage width:** any integer from 14 to 25 m, sometimes as narrow as 6 m at a row end. Frontages are separated by a 0.5 m gap.
- **Depth:** 36 m (occasionally 38 m).
- **Height:** 6, 9, 12 or 15 m.
- **Facing:** the north-side rows face south onto the boulevard, and the south-side rows face north. Ends of rows face the cross streets (Highland, Cahuenga, Wilcox, Vine…), so **corner buildings show a second facade**.

Some of these boxes carry shop names (`Max Factor Salon`, `Hollywood Gun Shop`, `Haberdashery`, `Drugstore`). The integrator builds them from the kit and adds the named sign cards from §10.1.

### 6.1 Families

Deliver **four families**, each with its own colour range, details and ground-floor storefront style:

| Family id | Style | Ground floor | Upper floors | Top |
|---|---|---|---|---|
| `SPN` | Spanish Colonial Revival | arched openings, tiled bulkheads, iron lamps | stucco, arched or deep-set windows, wrought-iron balconettes | red barrel-tile roof edge (pent roof) or curved parapet |
| `DECO` | Art Deco / Zigzag Moderne | black glass/marble bulkheads, chrome trim, transom grilles | vertical piers, spandrel panels with chevrons | stepped parapet with sunburst or zigzag crest |
| `BRK` | 1910s–20s brick commercial block | large plate windows, recessed doors, canvas awnings, transom glass | double-hung windows in brick with stone sills | corbelled brick cornice, parapet with painted wall sign |
| `MSN` | Mission Revival / plain stucco | simple storefronts, wooden awnings | small windows, stucco, exposed rafter tails | shaped (scalloped) Mission parapet |

Every module in a family uses the family's atlas (§9.4), so a whole building costs one material.

### 6.2 Modules per family (all mandatory)

Heights are fixed: **ground floor 4.5 m, each upper floor 3.5 m**.

| Module id pattern | Nominal size (W × H) | Count per family | Notes |
|---|---|---|---|
| `<FAM>_G_Bay4` / `_Bay6` / `_Bay8` | 4 / 6 / 8 × 4.5 | 2 variants of each width (A, B) | shop window(s) + door; awning is a separate module |
| `<FAM>_G_Entry4` | 4 × 4.5 | 1 | recessed entrance (≤ 1.5 m deep) or a stair-door to the upper floors |
| `<FAM>_U_Bay2` / `_Bay3` / `_Bay4` | 2 / 3 / 4 × 3.5 | 2 variants each (A, B) | upper-floor window bays |
| `<FAM>_Pilaster1` | 1 × 4.5 and 1 × 3.5 | 1 each | vertical pier/filler that absorbs odd widths |
| `<FAM>_Corner` | 1 × 4.5 and 1 × 3.5 | 1 each | outside corner, wraps 90° (0.5 m on each face) |
| `<FAM>_Parapet` | 2 / 4 × 1.5 | 1 each | top band. The integrator may **scale it vertically 0.5–2.0×**, so keep its lower 70% plain |
| `<FAM>_Crest` | 4–8 × 1.5–4 | 2 | centrepiece above the parapet (sign board, stepped deco crest, Mission gable); placed once per front |
| `<FAM>_Awning4` / `_Awning6` | 4 / 6 wide | 1 each | projects 1.2–1.5 m, bottom edge at 2.7 m |
| `<FAM>_Wall` | tileable 4 × 3.5 panel texture + a 1-quad mesh | 1 | side and back walls, tiles in both directions (texture in §10.2 style) |
| `<FAM>_Roof` | tileable roof texture + quad | 1 | flat tar-and-gravel roof with the family's coping colour |
| `<FAM>_Damaged_*` | any 2 of the above | 2 | boarded windows, cracked stucco or brick, soot (§6.3) |

Also deliver this **family-neutral roof clutter**, shared by all families:
- a water tank on legs;
- 2 rooftop billboard frames (8 × 4 m and 12 × 5 m) that take sign-page posters (§10.1);
- a stair bulkhead;
- 2 chimney/vent stacks;
- a skylight.

**How the integrator assembles a building (so you can see the modules fit):**

1. Floors are `n = max(0, round((h − 4.5 − 1.5) / 3.5))` upper floors on one ground floor. The parapet height absorbs the remainder: 6 m → 0 upper floors, 9 m → 1, 12 m → 2, 15 m → 3.
2. The front width is filled greedily left to right with ground bays (8, 6, 4), fixed up with pilasters. Upper bays are aligned to the ground bays.
3. A parapet runs along the top, and one crest is centred on fronts of 12 m or more.
4. Side and back walls are `<FAM>_Wall` quads with world-scaled UVs; the roof is a `<FAM>_Roof` quad, plus 1–3 random roof props.
5. A deterministic seed per building picks the family, variants, awnings and damage.

Deliver the module set so that this recipe produces a convincing street (§13 asks for a proof render).

### 6.3 Damage and occupation dressing (shared)

These are separate meshes, placed on top of any facade or wall and on the family-neutral atlas:
- boarded-window overlays at 1.5 × 1.5 m and 2 × 3 m;
- a sandbag stack (3 m), a sandbag doorway (4 m), and a fallen awning;
- 2 soot/scorch decal cards;
- a crack decal card;
- a "CLOSED BY ORDER OF THE MAYOR" notice card;
- 2 rubble piles (2 m and 4 m);
- a broken shopfront glass card.

## 7. Landmarks

These replace labelled boxes. Sizes are the boxes' `w` (east–west, x) × `d` (north–south, y) × `h`. **Front** is the face the integrator turns to Blender −Y; "also" means a second street facade that must be finished too.

Research each building's real 1930 appearance where it existed (reference photos are fine to look at; do not copy a photograph into a texture). Then stylise it per §1 and fit it to the box.

### 7.1 Playable-area landmarks (all mandatory)

| Asset id | Box label | w × d × h (m) | Front | Brief |
|---|---|---|---|---|
| `LM_Graumans` (hero) | Grauman's Chinese Theatre | 82 × 58 × 16 | S (forecourt) | Pagoda roof with verdigris copper, two coral-red columns, a stone "Heaven Dog" pair, and the famous forecourt. The box is the theatre mass; the forecourt walls are separate boxes `6 × 20 × 5` (deliver `LM_Graumans_ForecourtWall`, fits both, mirrored). A marquee reading *Hell's Angels* goes on a sign card. |
| `LM_Egyptian` (hero) | The Egyptian Theatre | 82 × 50 × 15 | N | The auditorium block, approached by a long courtyard. Its two courtyard wings are boxes `26 × 28 × 7` (deliver `LM_Egyptian_Wing`, mirrored): colonnades with fat papyrus columns, hieroglyph friezes and palms in pots. Also deliver `LM_Egyptian_Gateway` (a non-colliding pylon gate across the courtyard mouth at the boulevard, 30 m wide, 10 m tall, open in the middle 12 m). The portal to the dungeon is at the auditorium door: make the door big and dark. |
| `LM_Roosevelt` (hero) | Roosevelt Hotel | 74 × 58 × 24 | N | Spanish Colonial Revival, 12 storeys real (stylise to 7 readable floors), a roof sign "HOTEL ROOSEVELT" (sign card), arched ground-floor arcade. It is the inn, so it needs a welcoming lit entrance. |
| `LM_Pantages` (hero) | Pantages Theatre | 56 × 58 × 18 | S | Art Deco (opened 1930): vertical piers, a big blade sign "PANTAGES" (sign card) and a bulb marquee. |
| `LM_Taft` (hero) | Taft Building | 42 × 48 × 36 | N, also W (Vine) | 12-storey Renaissance Revival office block in buff brick, a heavy cornice and a ground-floor arcade of shops. It is the tallest thing in the zone, so it must look good from every side and from above. |
| `LM_HollywoodHotel` | Hollywood Hotel | 116 × 26 × 14 (main range) | S | A rambling Mission Revival hotel with long verandas and green-striped awnings. It is a **U-shaped courtyard building**: the main range plus two north wings `24 × 52 × 12` (`LM_HollywoodHotel_Wing`, mirrored) and a north courtyard wall with a 16 m gap, made of two `26 × 6 × 6` boxes (`LM_HollywoodHotel_CourtWall`). The courtyard is the ballroom area (§11.2). |
| `LM_FirstNational` | First National Bank | 42 × 38 × 22 | S, also W (Highland) | Security First National Bank at Hollywood & Highland: a Gothic-flavoured tower block, a banking-hall ground floor with tall arched windows, and a corner clock. |
| `LM_PoliceStation` | Hollywood Police Station | 46 × 34 × 10 | N | A civic two-storey building (brick and stone), "POLICE" lamps on posts flanking the door, barred lower windows, and a flagpole. |
| `LM_BrownDerby` | Brown Derby | 30 × 24 × 8 | W (Vine) | The 1929 Vine Street Brown Derby: a Spanish Mission restaurant with a derby-hat roof sign (sign card). It is **not** the hat-shaped Wilshire building. |
| `LM_MaxFactor` | Max Factor Salon | 14 × 36 × 12 | S | A small, elegant Hollywood Regency salon in pale pink and cream, with gilt lettering. (Optional: if time is short, build it from `DECO` kit modules plus a unique crest and sign.) |
| `LM_GunShop` | Hollywood Gun Shop | 22 × 36 × 15 | S | A `BRK` kit building with a unique ground floor: barred windows and a big painted "GUNS · AMMUNITION · REPAIRS" wall sign. A kit variant with unique pieces is fine. |
| `LM_Haberdashery` | Haberdashery | 24 × 36 × 12 | S | A `DECO` or `BRK` kit building with a unique hat-shop window display and sign. |
| `LM_Drugstore` | Drugstore | 18 × 36 × 6 | S | A one-storey corner drugstore with a soda-fountain sign, `SPN` or `BRK`. |
| `LM_HollywoodHigh` | Hollywood High School | 66 × 8 × 9 (north range) | N | A brick and cast-stone Classical/Moderne school front, entrance steps and a school name frieze. It is the **gym, a U open to the south**: the north range plus two side wings `8 × 52 × 9` (`LM_HollywoodHigh_Wing`, mirrored). The inside faces are the gym interior (§11.3); infested variant welcome (hive resin creeping from the gym). |
| `LM_ReceivingHospital` | Hollywood Receiving Hospital | 24 × 18 × 8 | N | A civic aid station: white stucco, red cross lamp, ambulance doors. |
| `LM_FireStation27` | Fire Station No. 27 | 20 × 24 × 9 | W (Cahuenga) | 1930 brick firehouse with two big red engine doors and a hose-drying tower. |
| `LM_BlessedSacrament` | Blessed Sacrament Church | 26 × 30 × 12 | N (Sunset) | A Spanish Renaissance church front, with a tower allowed above `h`. |

### 7.2 Hub dressing pieces (mandatory)

- `LM_RedCarStop`: a Pacific Electric red-car stop shelter at Hollywood & Highland, about 8 × 3 m with a bench and timetable board. It is non-colliding.
- `LM_Kiosk_Auction`: an auction-house booth at Vine, about 4 × 4 × 3.5 m. It is a wooden kiosk with a striped canopy and an "AUCTION" board.

### 7.3 Backdrop landmarks (beyond the inner hive wall; low budget, §3)

These sit in the blighted band between the two hive walls and are mostly seen from 100–400 m. Silhouette and colour matter most; keep detail painted.

| Asset id | Box label | w × d × h | Brief |
|---|---|---|---|
| `BD_HollywoodBowl` | Hollywood Bowl | 100 × 32 × 14 | The 1929 concentric-arch band shell set into a hillside |
| `BD_Paramount` | Paramount Pictures | 130 × 60 × 18 | Bronson Gate (a wrought-iron arch) in front of sound-stage blocks and a water tower |
| `BD_Columbia` | Columbia Pictures | 80 × 64 × 16 | Sound stages and an office block with a painted name band |
| `BD_RKO` | RKO Radio Pictures | 100 × 70 × 14 | Stages and a radio-tower crest |
| `BD_Chaplin` | Chaplin Studios | 90 × 70 × 14 | Tudor-revival cottage row fronting stages (as on La Brea) |
| `BD_PovertyRow` | Poverty Row | 30 × 44 × 7 | Shabby low stucco studios with hand-lettered signs |
| `BD_Christie` | Christie Studio | 40 × 40 × 9 | A small studio lot, offices and a stage |
| `BD_Cemetery` | Hollywood Cemetery | 70 × 12 × 8 | A gate, wall and mausoleum roofline (box is the wall strip) |
| `BD_LaBrea` | La Brea Avenue | 60 × 60 × 9 | A generic shop block cluster (can be kit-assembled; then deliver only a recipe note) |
| `BD_SunsetLaBrea` | Sunset & La Brea | 80 × 50 × 10 | Same as above, with a gas station canopy |

## 8. Street kit

Single-material props on the shared prop atlas unless noted.

### 8.1 Street furniture (mandatory)
- **Lighting:**
  - an ornamental double-globe boulevard street lamp (about 6 m);
  - a single-globe side-street lamp;
  - a trolley wire pole with a span-wire bracket for the red-car line.
- **Signals and signs:**
  - an "Acme"-style semaphore traffic signal (period);
  - a street-name sign post (blank blade, so the integrator adds names from the sign pages).
- **Kerbside:**
  - a fire hydrant;
  - a police call box on a post (also used as the quest object `callbox`);
  - a US Mail box on a post (the quest object `mailbox`);
  - a newspaper stand;
  - a bench;
  - a trash can (galvanised);
  - a sidewalk planter with a palm;
  - a standalone tall palm (Hollywood's palms are a key silhouette: 12 m, alpha cards for the fronds allowed).
- **Lights at night:** a marquee **arc lamp** on a tripod (the quest object `arc_lamp`). Deliver it with an emissive mask (§9.5).
- **Kerbs:** a kerb and gutter strip module (4 m), a corner kerb (radius 3 m) and a pedestrian crossing paint card.

### 8.2 Vehicles (mandatory)
- **Pacific Electric red car** (`ST_RedCar`):
  - a 1920s interurban car, about **13 × 2.6 × 3.2 m** to match its box;
  - Pacific Electric red with cream lettering;
  - trolley pole up; the front is −Y.
- **Period cars** (non-colliding parked dressing): Ford Model A sedan, Ford Model A roadster and a 1920s taxi.
- **Trucks:** a newsreel truck (the quest location of the film cans) and a delivery truck.
- **Wrecks:** wrecked or burned versions of a car and a truck that fit the wreck boxes, **7 × 3 × 2.6 m**. These are cover in the back lots: crushed, rusted, with chitin scratches.

### 8.3 Back-lot and hill dressing (mandatory)
- **Film-lot cover:**
  - 2 film-lot flats (painted scenery flats on braces) at 5 × 5 m;
  - a junk pile (a scrap node: quest object `scrap`);
  - a stack of crates;
  - 2 oil drums;
  - a coffee-tin crate (quest object `coffee_tin`);
  - a film can stack (quest object `film_can`).
- **Fences and barriers:** a wooden fence 4 m, a chain-link fence 4 m (alpha) and a barricade (sawhorse plus sandbags).
- **Franklin hillside:** 3 hillside rocks/outcrops for boxes `5 × 5 × 2–4` and 2 scrub bushes (alpha).

### 8.4 Hive kit (mandatory, organic: Rodin full generation allowed here, then decimate to budget)
- **Hive wall:**
  - `HV_WallSeg`: a hive wall segment, **8 m thick × 18 m high × 16 m long**, tiling end to end. It is resin and chitin plates fused with debris (car doors, beams), and has a jagged top.
  - `HV_WallEnd`: a wall end post for the edges of gaps.
  - A version of the segment for the outer ring, 26 m high (`HV_WallSeg_Outer`).
- `HV_Spire`: a hive spire, 5 × 5 × 22 m (the quest object `spire`, and the hill spires).
- `HV_EggCluster`: a cluster of 3–6 glossy eggs (quest objects `egg` and `boss_egg`).
- `HV_EggSac`: a big pulsing sac for the Egyptian stage boss fight, about 4 m.
- `HV_Growth`: 3 resin growth clumps (1, 3 and 6 m) for dressing doorways and walls, one of which fits the `12 × 2 × 5` growth box at the Egyptian door.

## 9. Production method (mandatory, per asset)

### 9.1 Concept image
- Generate one concept image per asset with your built-in image generation, in the §1 style:
  - for buildings, a clean **3/4 view on a plain neutral background** showing the front and one side;
  - plus a **straight front elevation** for facades and kit modules.
- Save them as `concept/<asset id>_34.png` and `concept/<asset id>_front.png`.
- Keep prompts in the manifest. Iterate until the concept is on-style and its proportions fit the box (§7) before modelling.

### 9.2 Blender model
- Build the low-poly mesh by hand (scripts are welcome) to the budget in §3, following the concept's proportions.
- Recess windows and doors by geometry only where it changes the silhouette or the roofline. Otherwise leave them flat and let the texture paint them.
- UV-unwrap with sensible texel density:
  - kit modules about 200 px/m;
  - landmark street faces at least 50 px/m (roofs and backs may be half that);
  - backdrops about 15 px/m;
  - props as large as the atlas allows.
- Avoid stretching. Mirror and stack identical parts (columns, repeated windows) in UV space to save room. Keep 8 px padding between islands.

### 9.3 Rodin texturing (texture-only)
- **Endpoint:** Hyper3D Rodin **Generate Texture**, `POST https://api.hyper3d.com/api/v2/rodin_texture_only`.
  - It takes a multipart upload of exactly one reference `image` (the concept) and one `model` (GLB recommended).
  - Options: `material` = `PBR` | `Shaded` | `All`, and `resolution` = `Basic` (2K) | `High` (4K).
  - The response has a top-level `uuid` (download) and `jobs.subscription_key` (status).
  - Poll the status endpoint, then download the result.
- Before writing the client, check the current request and response details against the official docs (`https://docs.hyper3d.ai/en/api-specification/generate-texture` and the Quick Start's status/download pages).
- **Key:** read `RODIN_API_KEY` from the repo-root `.env` at runtime.
  - **Never print it, log it, or write it to any file, manifest, script or command line that is saved.**
  - Use `Authorization: Bearer <key>` in memory only.
- **Material:** request `All` on your first assets, compare the two outputs, and record which fits the hand-painted look better:
  - `Shaded` has lighting baked in, which may suit our simple lit shader;
  - otherwise take PBR base colour and paint in the top-light and AO yourself.
  - Then standardise.
- **Geometry check (first asset, then every result):** the returned model may be re-meshed or re-unwrapped. Compare its vertex/face count and UVs with what you uploaded.
  - **If Rodin's geometry or UVs differ, bake** its texture back onto *your* mesh and *your* UVs in Blender: Cycles, Selected-to-Active, emit/diffuse colour only, with a small cage/extrusion.
  - **The delivered mesh is always your mesh.**
- **Quality loop:** inspect every textured result from the front, side, back and top.
  - Bad side, back or roof (one reference image can't show them) → texture again with a second reference image (side elevation or roof view) and merge the bakes by face masks; or hand-fix in Blender texture paint.
  - Two failed attempts on an asset → §15.
- **Credits:** keep a ledger, `rodin_ledger.json`, listing asset, attempt, task uuid, options and credits consumed. Base texture runs are 0.5 credits per the docs. **Stop and report if the ledger passes 200 credits** before finishing.

### 9.4 Shared atlases for kits
- A kit family's modules should share **one 2048² atlas** (two at most).
- **Option A:** lay the modules out together in Blender, with their UVs packed into one 0–1 space, and texture the combined mesh in one Rodin run (with a front-elevation reference sheet of the family). Then split it back into modules, keeping the UVs.
- **Option B:** texture modules separately and repack their textures into an atlas by baking.
- Either is fine. Record which you used.
- Tileable textures (walls, roofs, ground; §10.2) are separate square tiling textures, not atlas parts. Make them tileable by painting or generation plus seam fixing; Rodin is not needed for these.

### 9.5 Emissive masks (optional but wanted)
For night (a day/night cycle is planned), deliver a grayscale `<asset>_E.png` mask (white = glows) for:
- marquee bulbs, neon and lit windows (sparse: about 15% of upper windows);
- street lamp globes, the arc lamp head and the red car's headlamp;
- the hive spire and eggs (amber veins).

It has the same UVs as the base colour. The integrator multiplies it by a colour per asset.

## 10. Signs, posters and tileable materials

### 10.1 Sign and poster pages
Lettering must be **legible and spelled correctly**, which Rodin can't guarantee. Signs are therefore **flat or simple cards** whose textures come straight from generated images. Compose them on 2048² pages, 4–12 signs per page.

- **Lettering:** if image generation garbles text, compose the lettering with the project's OFL fonts:
  - `hollywoodland-ue/Content/Data/Fonts/Limelight-Regular.ttf` (Art Deco display);
  - Alegreya Sans (Regular, Bold, Italic) in the same folder;
  - any other 1920s-appropriate OFL font you add to `art/buildings/fonts/` with its licence.
- **Mandatory signs:**
  - GRAUMAN'S CHINESE (vertical blade) and a marquee board "HELL'S ANGELS";
  - EGYPTIAN (pylon panel) and a marquee "ROBIN HOOD · DOUGLAS FAIRBANKS";
  - HOTEL ROOSEVELT (roof letters);
  - PANTAGES (blade);
  - HOLLYWOOD HOTEL;
  - SECURITY FIRST NATIONAL BANK;
  - TAFT BUILDING;
  - THE BROWN DERBY;
  - MAX FACTOR;
  - HOLLYWOOD GUN SHOP;
  - HABERDASHERY;
  - DRUGSTORE · SODA FOUNTAIN;
  - HOLLYWOOD POLICE;
  - RECEIVING HOSPITAL;
  - ENGINE CO. 27;
  - HOLLYWOOD HIGH SCHOOL;
  - PACIFIC ELECTRIC;
  - AUCTION.
- **Street-name blades:** Hollywood Blvd, Highland Ave, Cahuenga Blvd, Wilcox Ave, Vine St, Franklin Ave, Selma Ave and Sunset Blvd.
- **Generic shop signs (16):** for example CAFÉ, BARBER, SHOES, RADIO REPAIR, BAKERY, CIGARS, JEWELER, NOTIONS, HARDWARE, FLORIST, TAILOR, BOOKS, LUNCH, HOTEL ROOMS, PHOTOGRAPHER and DRY GOODS. Use plausible invented proprietor names; no real modern brands.
- **Posters:** 8 period posters for walls and the rooftop billboard frames, styled as 1930 film-poster and product ads. **Only films released in 1930 or earlier, and only products that existed by 1930.** Original artwork; do not reproduce a real poster's image. Add 2 occupation notices ("CURFEW AT DUSK", "REPORT EGG CLUSTERS TO THE LAPD").
- **Marquee letter board:** one blank marquee board (white panel with bulb borders) so the integrator can set custom text later.

### 10.2 Tileable materials (2048², seamless)
Each is a base colour PNG, with an optional emissive mask:
- road asphalt;
- road with **two red-car rails** (a 4 m tiling strip, rails 1.44 m gauge);
- sidewalk concrete slabs;
- kerb top;
- brick (two colours);
- white stucco, cream stucco and buff brick;
- tar-and-gravel roof;
- red barrel tile;
- blighted earth (purple-grey ash with resin flecks);
- hillside dry grass;
- Egyptian lobby marble;
- red theatre carpet with an Egyptian border;
- ballroom parquet;
- gym boards (court lines painted, as a separate 1:1 card);
- hive resin.

State each one's **real-world tile size** in metres in the manifest (`tile_m`).

## 11. Navigable interiors

The game's movement is 2D. Rooms are built from collision boxes in data on one floor (there is no Z in navigation), so **no walkable stairs, upper floors or ramps**. Stairs, balconies and galleries may exist as non-walkable decoration against walls.

Interiors have **no ceilings**, because the camera floats above. Walls should therefore look good from above (finished wall tops) and on both faces where both are visible. You may deliver hanging pieces (chandeliers, a projector beam card) as separate meshes.

Each interior **wall module is a facing**, 0.3–0.6 m thick. It lines the inner face of a collision box (the boxes are 4 m thick in the Egyptian). Origin rule as in §2, with the room-facing side toward −Y.

### 11.1 The Egyptian Theatre dungeon (mandatory, highest interior priority)

Layout from `tools/gen/zone.mjs`: a 120 × 280 m area. The playable rooms are 80 m wide (x 20–100) and are separated by divider walls with a **12 m doorway** in the middle. The walls are **9 m high**.

| Room | Size (w × d) | Contents to dress |
|---|---|---|
| Forecourt | 80 × 42 | Colonnades on both sides (boxes 4 × 32 × 6 at the room's edges), palms in pots, a ticket booth, hive growth |
| Lobby | 80 × 40 | Marble floor, papyrus columns, a grand staircase as wall decoration, a concession counter, the Brood Keeper's egg clusters |
| Auditorium | 80 × 80 | **5 seat rows on each side of a 12 m centre aisle.** Each row is a box **26 × 3 × 1.4 m**. Deliver `IN_EGY_SeatRow2`, a 2 m repeating seat-row module (fits 13× per row), plus a row-end cap. Also a side box, wall sconces and a balcony front decoration high on the back wall |
| Projection booth | 80 × 30 | Projectors (2), film reel racks, rewind bench, porthole wall (the booth faces the stage end), reel cans (quest object `reel`) |
| Stage | 80 × 48 | A raised stage platform **60 × 16 × 0.3 m**, a proscenium arch with Egyptian decoration across the back, footlights, a torn curtain, and the Hive Mother's egg sac (`HV_EggSac`) |

**Interior modules** (family `EGY`, sharing one atlas plus tileables):
- wall panels 4 × 9 m (plain, and with a relief frieze);
- a pilaster/column 1 × 9 m;
- a 12 m doorway frame (an Egyptian pylon portal) for the divider openings;
- the 4 × 32 × 6 colonnade module for the forecourt;
- an outer corner and an inner corner;
- a captured usher stand (a small resin cocoon on a post, the quest object `usher`).

**Floors:** the room floor is a ground strip that the integrator textures with the tileables (marble, carpet, boards). A red carpet runner 12 m wide runs the length of the centre aisle.

### 11.2 Hollywood Hotel ballroom courtyard (mandatory)
The U-courtyard of `LM_HollywoodHotel` (§7.1) is the open-air "ballroom" where an elite group fight happens. Deliver:
- inner courtyard facades of the main range and wings, using courtyard-facing modules (`IN_HHB_*`): veranda arcades, French doors and string lights;
- a bandstand (6 × 4 m);
- 6 round tables with chairs (one piece each, 2 m);
- a dance-floor parquet card (tileable from §10.2);
- 2 potted palms and a broken chandelier on the floor (hit by the Brute).

### 11.3 Hollywood High gym (mandatory)
The U of `LM_HollywoodHigh` is the gym where egg clusters are destroyed. Deliver:
- inner faces of the three wings (`IN_GYM_*` wall modules 4 × 9 m, with high windows and a scoreboard);
- 2 basketball backboards on wall brackets;
- folding bleachers (non-walkable, 8 m);
- a gym floor court-lines card;
- hive-infested variants of the wall module with resin and egg clusters in the corners.

### 11.4 Generic shop interior (optional, last)
A counter, two shelf units, a display case and a cash register: for future enterable vendors.

## 12. Delivery layout and manifest (hard rules)

All work stays inside `art/buildings/`:

```
art/buildings/
  SPEC.md               this file (do not edit; write questions/deviations in README.md)
  README.md             report (§13), pipeline notes, deviations
  manifest.json         machine-readable index (below)
  rodin_ledger.json     Rodin calls and credits (§9.3), no secrets
  concept/              generated concept images (§9.1)
  meshes/               <asset id>.fbx  (FBX binary 7.4, meters, Z up, -Y front, one mesh, one material slot)
                        <asset id>.glb  (same mesh, for previews; optional)
  textures/             T_<asset id or atlas id>.png (base colour), _E.png (emissive mask), tileables T_TILE_<name>.png
  signs/                sign/poster pages T_SIGN_<page>.png + per-sign rectangles in the manifest
  preview/              renders (§13)
  src/                  .blend sources and every script needed to rebuild the outputs
  fonts/                any added OFL fonts + licences
  work/                 scratch (may be deleted by the integrator; keep large intermediates here)
```

**`manifest.json`:**

```json
{
  "version": 1,
  "units": "meters", "up": "+Z", "front": "-Y",
  "texture_sets": 0,
  "atlases": { "ATL_SPN": {"texture": "textures/T_ATL_SPN.png", "emissive": "textures/T_ATL_SPN_E.png"} },
  "tileables": { "TILE_Brick_Red": {"texture": "textures/T_TILE_Brick_Red.png", "tile_m": [4.0, 4.0]} },
  "assets": [
    {
      "id": "SPN_G_Bay6A",
      "kind": "module",              // module | wall | roof | landmark | backdrop | prop | interior | dressing | sign_card | hive
      "family": "SPN",               // kits and interiors only
      "file": "meshes/SPN_G_Bay6A.fbx",
      "material": "M_ATL_SPN",
      "texture": "atlas:ATL_SPN",    // or "textures/T_<id>.png"
      "emissive": null,
      "nominal": [6.0, 4.5],         // modules: width x height; landmarks/props: [w, d, h]
      "bounds": [[0.0, -1.4, 0.0], [6.0, 0.6, 4.5]],   // measured min/max after export
      "tris": 412,
      "origin": "front-plane-bottom-left",   // or "footprint-centre-ground" / "ground-centre"
      "fits_box": null,              // landmarks: the exact box label, e.g. "Grauman's Chinese Theatre"
      "also_fits": [],               // extra box sizes it fits (mirrored wings etc.) as {"size":[w,d,h],"mirror":true}
      "recess": [],                  // §4 recesses deeper than 2 m: [x0, y0, x1, y1] in asset space
      "quest_object": null,          // e.g. "callbox", "arc_lamp", "egg", "reel"
      "concept": ["concept/SPN_G_Bay6A_front.png"],
      "rodin": {"used": true, "material": "Shaded", "baked_to_own_uvs": true},
      "notes": ""
    }
  ],
  "signs": [ {"id": "SIGN_Pantages", "page": "signs/T_SIGN_01.png", "rect": [0.0, 0.0, 0.25, 0.5], "text": "PANTAGES", "card_mesh": "meshes/SIGN_Pantages.fbx"} ],
  "recipes": {
    "SPN": {"ground": ["SPN_G_Bay8A", "..."], "upper": ["..."], "pilaster": "...", "corner": "...", "parapet": ["..."], "crest": ["..."], "awning": ["..."], "wall": "SPN_Wall", "roof": "SPN_Roof", "damaged": ["..."]}
  }
}
```

(JSON has no comments; the comments above only explain the fields.) Every file in the tree must be listed, and every manifest path must exist. Measure `bounds` and `tris` from the exported files, not from intentions.

## 13. Report, previews and self-check

**`preview/`:**
- One contact sheet per phase, showing each asset from the front 3/4, back 3/4 and top-down at 45°, in flat neutral light with its texture.
- **Kit proof render (mandatory):**
  - assemble a 120 m stretch of street with the §6.2 recipe from your manifest only (a script in `src/`), mixing all four families at heights 6, 9, 12 and 15 with a corner building;
  - render it from a street-level MMO camera (3 m behind a 1.8 m figure, 20° down) and from 30 m up.
- A side-by-side of each landmark next to a 1.8 m human silhouette and its collision box wireframe.

**`README.md` report (final section "Final report"):**
- what was delivered and what wasn't;
- texture-set count;
- per-asset triangle counts;
- the estimated triangle total for the dressed boulevard;
- Rodin credits used;
- the Shaded vs PBR decision;
- any deviations (§15);
- anything the integrator must know.

**Self-check before delivery (each must be true):**
- [ ] Every mesh: one material slot, triangulated, transforms applied, metres, Z-up, −Y front, origin per §2.
- [ ] Every mesh within §3 triangle budgets and §4 box fits (with measured bounds in the manifest).
- [ ] No texture over 2048² or under 1024² (sign pages and tileables included); all PNG sRGB.
- [ ] Kit modules butt together on the 0.25 m grid without gaps (checked in the kit proof render).
- [ ] Every sign spelled correctly; no post-1930 brands or films; no real photograph copied into a texture.
- [ ] No secrets anywhere in `art/buildings/` (search the tree for the key's first characters before finishing).
- [ ] Every manifest path exists, and every file is in the manifest; `src/` can rebuild the outputs.

## 14. Phasing (deliver in this order; each phase is usable on its own)

After each phase, update `manifest.json`, the previews and the README, so the integrator can start while you continue.

1. **Pipeline proof:** one `BRK` ground bay, one `BRK` upper bay and `LM_Drugstore`. Settle Shaded vs PBR, the bake-back step and the ledger. Write the decision into the README.
2. **Storefront kit:** all four families (§6.2), damage dressing (§6.3), roof clutter, the matching tileable walls and roofs, and the kit proof render.
3. **Hub landmarks:** `LM_Graumans`, `LM_Roosevelt`, `LM_Egyptian` (plus wings and gateway), `LM_HollywoodHotel` (plus wings and court wall), `LM_FirstNational`, `LM_Pantages`, `LM_Taft`, `LM_BrownDerby`, `LM_PoliceStation`, `LM_RedCarStop` and `LM_Kiosk_Auction`, with their sign cards.
4. **Street kit** (§8.1–8.3) and the red car, plus the remaining tileable ground set (§10.2).
5. **Egyptian dungeon interior** (§11.1).
6. **Remaining landmarks:** Max Factor, Gun Shop, Haberdashery, Hollywood High (plus wings), hospital, fire station, church, and the ballroom courtyard and gym interiors (§11.2–11.3).
7. **Hive kit** (§8.4).
8. **Backdrop landmarks** (§7.3), the remaining sign and poster pages, and emissive masks.
9. Optional: the shop interior (§11.4).

## 15. Fallbacks (only if the tooling can't meet a rule)

- **Rodin unavailable, failing or out of credits:**
  - Texture by **camera-projecting the generated concept images** (front, side and a roof view) onto your UVs in Blender and baking, then hand-fixing the seams in texture paint.
  - Mark the asset `"rodin": {"used": false, "method": "projection"}`.
- **Image generation produces wrong proportions:** model to the box (§7), not the image. The box wins.
- **An asset can't meet its triangle budget without losing its identity:** exceed it by at most 25%, and list it in the report with the reason.
- **A landmark can't fit its box convincingly:** propose a recess or a box change in the README (with exact numbers). Do not change any game data yourself.
- **Never** edit files outside `art/buildings/`, change game code or data, or commit to git. Questions and proposals go in the README.
