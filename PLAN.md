# Hollywoodland — Project Plan

Audience: development agents. Read with `DESIGN-BIBLE.md` (§ refs below) and `README.md` (build/deploy, engine constraints).
Status legend: **DONE** works in the build · **PROTO** simplified stand-in, playable · **DATA** rows/facade only · **TODO** absent.

## 1. Architecture (as built)

```
browser ── itch.io page (iframe) ── hollywoodland Worker (R2 stage, COOP/COEP) ── UE 5.9 Wasm client
                                                                                   │ fetch (text/plain JSON)
                                                             hollywoodland-server Worker ── Fireworks (Rune + DeepSeek), KV
```

- **Client = one C++ module, no Blueprints, no authored maps.** `AHLGameMode` spawns `AHLWorldActor`, which owns `FHLGame`
  (plain C++ simulation), builds gray-box geometry from `Content/Data/areas.json`, mirrors units into basic-shape actors, drives
  the camera and hosts `SHLGameUI` (one immediate-mode Slate leaf widget that paints all UI and handles all input).
- **Data-driven.** `tools/gen-data.mjs` → `Content/Data/*.json` (staged as loose UFS files). `FHLData` loads them. Change content
  by editing `tools/gen/*.mjs` and regenerating; C++ rarely needs to change for content.
- **Simulation split (bible §13).** Local C++ does movement, combat, goals, Census, templates/corpus chat. The Worker is called
  only for: lines addressed to the player (say/whisper/party/zone), invite decisions, party events, and a gossip batch every
  ~2.5 min. Rune (System One, ~0.3 s, input-only cost) answers typed questions (respond/ignore, accept/decline, liking score);
  DeepSeek flash (reasoning off, ~1 s) writes the lines. Every failure → corpus fallback; game is fully playable offline.
- **Guardrails are enforced locally**, not by the model: only `Local` sims within ±5 levels can join a party, whatever the model
  says (`FHLGame::InviteSim`).

### Source map (`hollywoodland-ue/Source/Hollywoodland/`)

| File | Contents |
|---|---|
| `HLBridge.*` | fetch / localStorage / console.log via EM_JS on the browser main thread; FHttpModule + files natively |
| `HLData.*` | row structs + JSON loaders (`FHLData::Get()`), `EquipPool(level, quality)` |
| `HLGame.h` | all simulation types and the `FHLGame` API (single header) |
| `HLGame.cpp` | `FHLNav` (2 m grid A*, LOS, slide), init, tick order, `MoveTo`, stat formulas (§5/§7), spawning |
| `HLCombat.cpp` | cast/execute abilities (data effects), attack table, damage/heal/threat, XP, death |
| `HLMobs.cpp` | mob AI (aggro, social pull, leash, flee, evade), boss scripts, loot, camp respawn, object interaction |
| `HLPlayer.cpp` | character creation, save/load JSON, inventory/equip/vendor, targeting, quests, auction house, loot rolls |
| `HLSim.cpp` | Census tick, sim brains (local/transient/idler/party), ability picker, invites, Director (first 15 min) |
| `HLMusic.*` | music director (title, subzone playlists day/night, notable-fight/boss/death/victory tracks); playback via `HLBridge::MusicPlay` (web `<audio>`, native MCI) |
| `HLChat.cpp` | ambient chat (templates + corpus), typing-style filter, slash commands, Worker client + action application |
| `HLWorldActor.*` | geometry build, unit visuals, camera, `-HLAuto` self-play test, `HL_STATUS` console line every 20 s |
| `HLUI.cpp`, `HLUIWindows.cpp` | toolkit (Rc/Tx/Btn/Win/ItemIcon), input, title screen, nameplates, HUD, chat, minimap, all windows |

### Verification loop

1. `node tools/gen-data.mjs` (validator must print `validation ok`).
2. `Scripts\BuildEditor.ps1`, then `Scripts\RunNative.ps1 -HLAuto=veteran -HLAutoQuit=90`: self-plays; read
   `Saved\Logs\Hollywoodland.log` for `HL_STATUS` and `Saved\Screenshots\hl_auto_*.png`.
3. `Scripts\BuildWeb.ps1`, `node workers/deploy-stage.mjs ... vN`, then
   `tools-venv python tools/probe.py https://hollywoodland.coeurnix.workers.dev/ --auto veteran --seconds 120 --shot out.png`.
4. `node tools/test-server.mjs` for the Worker.

## 2. Implementation status vs. the bible

| System (bible §) | Status | Notes / gap |
|---|---|---|
| Web build + R2/Worker hosting (§13) | DONE | Shipping Wasm, forward tier. Stage ≈ 115 MB incl. Brotli siblings (budget 500 MB). |
| itch.io embed (§13) | DONE (wrapper) | `itch/index.html` iframe; needs itch "SharedArrayBuffer support" on. See §6. |
| Windows standalone | DONE | `Scripts\BuildWindows.ps1` (Development) -> `win-dist-Development\Windows\Hollywoodland.exe`; verified with `-HLAuto` self-play. |
| Saves (§13) | DONE | localStorage JSON every 30 s + KV cloud copy every 2 min (`/v1/save/<player>`); cloud restore UI TODO. |
| Zone layout (§14) | PROTO | Gray boxes on a flat plane, real street grid compressed. Exact box collision for movement, line of sight and camera. No slopes, interiors, rooftops (Spitters are on the ground). |
| Exits / hive wall (§14) | DONE | Perimeter wall, 4 gaps, elite gap guards L22-34 (speed 8.5, leash 70). Beyond it a hostile band: the edges of Cahuenga Pass (30-40), Gower Gulch, Melrose and West Hollywood with landmarks and roaming elite patrols (speed 8.5-10, leash 140), sealed by a second hive ring. Aggro radius 14 m + level gap (4-40 m); hits from 5+ levels up Daze. Transients despawn at the outer edge. |
| Classes ×8, abilities to L12 (§5) | DONE | 7 abilities each, data effects. No ranks (scaled 8%/level instead). Icons and flavour text (tools/artgen). |
| Stats/formulas (§5, §6) | DONE | One-roll attack table: miss, dodge, parry, block (riot shields), crit. Ammo: pistol/rifle/shotgun calibres in boxes of 100 (gun shop, quartermaster, newsie), one round per auto-shot, adds dps. Durability: 10% per death, broken items give no stats, repair at vendors (1c x ilvl per 10%). |
| Mobs (§7) | DONE | 9 open-world templates, 2 rares, 2 elites, 4 gap guards, 4 dungeon trash, 4 bosses. Burrower/Flier behaviours TODO. |
| Death (§6) | DONE | Release as a ghost at the nearest aid station, run back to the corpse (marker, 1.25x speed) and revive at 50%, or revive at the station for -25% durability and 10 min sickness (-25% attributes). Dungeon deaths respawn at the portal. |
| Quests (§10, §14) | DONE 38 / PROTO 3 | Kill, drop-collect, world-collect, use, deliver, escort and craft objectives work. `q_hb_14` finds Whitey Nolan, `q_hb_14b` escorts him to the Pantages (follows the player, three ambushes, fails if he dies or is left 60 m behind). `q_hb_29` crafts 20 .38 rounds (handed in). Eight class quests at level 10 from the class trainers, each with a named blue class weapon. Breadcrumbs 30-32 accept-only. Counters only: quest items are not bag items. |
| Items (§8) | DONE ~3,960 rows | Generator + formulas. Base list is ~70 bases (target ~680). 1,447 painted icons and tooltip descriptions (tools/itemgen; gloves vision-checked). On-use pocket items, on-equip effects TODO. |
| Vendors, trainers, bank | DONE / PROTO | Buy/sell, class trainers, 24-slot bank. Bank slot purchase, inn rested XP are text only. |
| Auction house (§9) | DONE | 160 simulated listings following the population spread; name search, bids with rival bidders (outbid mail), buyout, 1/4/12 h listings, sim buyers and bidders, results by mail; your listings and bids persist in the save. Price history TODO. |
| Professions (§9) | DONE 2 / DATA rest | Salvaging 1-75 on wreck nodes. Gunsmithing 1-75 from Earl Tanner (level 5, 25c): six recipes (.38, birdshot, Zip Gun, .30-30, .45, Sawed-off Coach Gun) from Scrap Metal and Brass Casings (wrecks, looters, Earl's stock), workbench window (K), cast bar, orange/yellow/green/grey skill-ups. Other professions DATA. |
| Dungeon: Egyptian (§14) | PROTO | One persistent "instance" area at +3000 m. 4 bosses with simplified mechanics (eggs hatch, charge, acid pool with LOS, add waves + egg sac heal). Loot 2 blues/boss, need/greed rolls. Untuned: run `-HLAuto` party tests (WP7 target: 3 of 5 clears). |
| Simulated population (§11) | PROTO | 300 personas; ~40 locals + ≤8 transients + ≤6 idlers as full units (no crowd LOD yet; fine at this count). Census: logins, level-ups, dice dungeon runs → events. |
| Party (§6) | DONE | Invite either way, follow/assist/heal/taunt, loot rolls, leave after 15-40 min. Sim tanks/healers use generic priority picker. |
| Chat (§11) | DONE | 63 templates, ~790 corpus lines (237 hand-written + LLM cache), typing styles, item links, whisper-by-click, /who. |
| Worker (§13) | DONE | `/v1/session`, `/v1/sim` (reply/decide/gossip), `/v1/save`. Per-isolate rate limit 20/min, 400/h; daily budget switch. |
| Director (§14) | PROTO | 5 steps: invite, passing heal, Drone train, raid talk + red-car arrivals, buddy whisper at 5. Not randomised per play yet. |
| Honesty (§11) | DONE | Disclosure is the game's job: title notice, first system line, "Show simulated players" toggle. The characters stay in role (prompt + server-side guard that swaps fourth-wall breaks for in-character deflections). |
| Talents (§5) | DONE | 3 trees × 16 per class (384), 5 tiers (5 points each), passive modifiers applied in stats and combat; respec at the class trainer ($1 doubling to $50). Tiers 1-2 hand-made, 3-5 designed with Claude and validated (`tools/artgen design`). |
| World map / red car / inspect / who | DONE as facade | 13-zone map with blurbs and dungeons, route map, inspect shows persona gear. |
| World events (§10) | DONE (3 of 7) | `HLEvents.cpp`. Hive surge every 30-50 min on a hub (warning in zone chat, 10 min of waves, up to 6 local sims hold the line, reward for 3+ kills, hub merchants close 15 min if overrun); picture night every ~45 min at the Chinese (pre-1930 public-domain titles, sims sit in rows and chat, +10% kill XP for 30 min after 45 s watching); sims call rare spawns in zone chat. Hive spread, fight night, fishing derby, red-car breakdown TODO. `-HLEventsSoon` for tests. |
| Duels, trade, mail, friends (§6, §9, §11) | DONE | `HLSocial.cpp`. Duels outside hubs (loser stops at 1 hp, forfeit at 40 m, rivals challenge you); face-to-face trade window (sims accept fair offers and gifts); whisper a trade-chat seller "wtb" and they send it C.O.D.; mailbox with auction proceeds, returns, outbid refunds and letters; friends list (Who window tab) with online/offline notices. Guild UI TODO. |
| Mounts for transients | PROTO | A dark box under the rider, +60% speed. |
| Music | DONE | `HLMusic.cpp`; 30 tracks streamed from R2 `music/` on the web (not in the pak), copied to `<pkg>\Music` natively. Menu volume + `/music`. |
| Day/night, SFX/ambience, VFX, animation | TODO | None. |
| Interface (§1 WP10) | DONE | Alegreya Sans + Limelight (OFL), Art Deco frames, FLUX panel/header/title art, painted icons on bars, bags, spellbook, menu; quest goal markers (screen diamond, edge arrow, minimap, tracker distance); hostile-zone banner. |

## 3. TODO (ordered; each item is independently shippable)

### P0 — correctness and feel (do first)
1. **Tune the Egyptian.** Run 5-sim parties through it (extend `AutoTest` to form a party and path through portals); adjust
   `hp/dmg` in `tools/gen/zone.mjs` dungeon mobs until 3/5 clears with ~1 wipe at the Hive Mother.
2. **Tune 1-6 pacing** to 45 min with quests (bible §14 target). Levers: quest XP multiplier (`quests.mjs`), mob respawn (90 s),
   sim kill-stealing density (`MAX_LOCALS`, `PickCampFor`).
3. ~~Camera collision~~ (done: exact box ray). Click-to-move is absent.
4. **Mouse look on the web:** verify pointer-lock behaviour of `UseHighPrecisionMouseMovement` in the itch iframe; fall back to
   plain capture if the browser refuses the lock.
5. ~~Unit separation~~ (melee mobs spread around their target; sims step aside from each other, NPCs and the player).
6. Restore from cloud save when localStorage is empty (GET `/v1/save/<player>` needs the player id → show a code in the menu).

### P1 — systems the bible calls for in the slice
7. ~~Ghost run + durability + repair costs~~ (done).
8. ~~Parry/block, shields, ammo~~ (done).
9. ~~Escort quest type (follow-AI NPC, fail on death) for `q_hb_14`; class quests at 10; `q_hb_29` + Gunsmithing 1-75 with 6 recipes.~~ Done.
10. Crowd LOD (§11 levels 0-3): only needed when population on screen exceeds ~60; implement as sims without `FHLUnit` combat.
11. ~~World events: surge, rare calls, picture night~~ (done); hive spread, fight night (needs duels).
12. ~~Trade window, duels, mailbox, friends list~~ (done).
13. ~~Auction: bids, durations, search~~ (done); price history, sims undercutting the player.
14. ~~Talents~~ (done: all 5 tiers).
15. Interiors: Hollywood Hotel ballroom walls/roof, Egyptian rooms with height; rooftops for Spitters (needs Z in nav).

### P2 — population depth (the showcase, §11)
16. ~~Persistent relationships~~ (done: each save seeds 3 pals, a rival, a flirt, a mentor and a gossip from the player id; they whisper, mail gifts, befriend, challenge; bonded sims are likelier to log in and appear on the Boulevard). Original note: on first login pick ~8 personas as this save's friends/rivals/flirt/nemesis (seeded by player
    id), bias Director events and whispers toward them, surface "X has come online".
17. Sims use the auction house, trainers and vendors visibly (stand at NPC, emote), gather nodes, compete for quest objects.
18. Sim-formed dungeon groups: LFG shout → walk to the Egyptian → enter (despawn) → return 20 min later with boss loot equipped.
19. Per-play randomisation of Director events; add: duel outside the Roosevelt, dance party on the bank steps, AFK chains.
20. Typing indicators / delays by persona `reaction_s`; multi-line conversations between sims in zone chat (templated pairs).
21. Census: guild rosters, raid schedules by server clock, wipes/clears feeding guild chat; 30 "server history" events at start.
22. Worker: move rate limiting and the daily budget to a Durable Object; cache gossip per 2-min bucket in KV across players;
    add `kind:"decide"` for trades/duels; stream nothing (latency is hidden by typing time).
23. Worker prompt: send the persona's last 3 lines to keep voice; add a `goal` application path client-side (`FOLLOW`, `IDLE`).

### P3 — content and presentation
24. Art pass per `ASSETS.md` (10 landmarks first). Keep the data-driven box layout as collision; swap visuals per box label.
25. Items: grow base list to ~680 (bible §8), icons (~700), pocket items, epics set looks; validator rule for unique names.
26. Quest text to 60-120 words (LLM expand with cache, human spot-check 20). Unbuilt-zone quest titles to ~45/zone.
27. Audio (music done): SFX, ambience, 1925-and-earlier recordings on radios, UI clicks, combat hits. Public-domain film on the Chinese screen (§14 assets).
28. Day/night cycle; skybox with the dome; HOLLYWOODLAND sign on the north horizon.

## 4. Technical dependencies and risks

- **UE 5.9 Preview web target is experimental.** Keep to proven paths: no nav mesh, no Niagara, no skeletal meshes yet, no
  UMG. Skeletal characters need `ACLPlugin` (already in WebMinimal) and a recook; test one before committing to an art plan.
- **Stage size:** the engine wasm dominates (≈74 MB raw, 14 MB Brotli). Content budget left ≈ 380 MB.
- **Per-frame cost** is dominated by Slate text (every string is two draw elements for the shadow). If the web frame time
  suffers: cache `TW()` measurements, drop shadows on small text, cull nameplates beyond 40 m.
- **A\*** is capped at 40k expansions and runs synchronously; if many sims repath in one frame add a per-frame budget queue.
- **Save schema** is versioned (`"v":1`); bump and migrate in `LoadJson` when fields change.
- **Secrets:** `FIREWORKS_API_KEY`, `SESSION_SECRET` are Worker secrets. `tools/gen-corpus.mjs` reads the key from env only.

## 5. Data pipeline (bible §12) — status

| Step | Source file | Rows | Notes |
|---|---|---|---|
| Zones, dungeons, guilds, red car | `tools/gen/world.mjs` | 13 / 13 / 8 / 4 | hand-written; unbuilt zones carry mobs + 4-8 quest titles |
| Classes, abilities | `tools/gen/classes.mjs` | 8 / 61 | incl. 5 mob abilities |
| Mob templates | `tools/gen/zone.mjs` | 25 | only built ones; other zones list names only |
| Layout, camps, NPCs, objects | `tools/gen/zone.mjs` | 2 areas, 192 mobs, 34 NPCs, 75 objects | metres; boxes become collision + visuals |
| Items | `tools/gen/items.mjs` | 3,962 | formulas §8 |
| Quests | `tools/gen/quests.mjs` | 31 | |
| Personas | `tools/gen/personas.mjs` | 300 | seeded; gear matches inspect |
| Chat | `tools/gen/chat.mjs`, `tools/cache/corpus.json` | 63 + ~790 | cache from `tools/gen-corpus.mjs` |

Validator (end of `gen-data.mjs`): ID existence, camp/quest/object coverage. TODO: formula re-check, post-1930 brand reject,
quest-text name check, level-band rule for quest mobs.

## 6. Deployment state

| Thing | Value |
|---|---|
| GitHub | https://github.com/coeurnix/hollywoodland (public, MIT) |
| Client Worker | `hollywoodland` → https://hollywoodland.coeurnix.workers.dev/ ; R2 bucket `hollywoodland-stage`; live prefix = `DEPLOY` var in `workers/client/wrangler.jsonc` |
| Server Worker | `hollywoodland-server`; KV `HL_KV` (id in `workers/server/wrangler.jsonc`); secrets set |
| itch.io | Upload `itch/hollywoodland-itch.zip` (just `index.html`) as an HTML game, viewport 1280×720+, enable fullscreen button and **SharedArrayBuffer support**. Butler: `butler push itch/hollywoodland-itch.zip <user>/hollywoodland:html5`. |

Music: `cd workers; node deploy-music.mjs` uploads `music/*.mp3` to R2 `music/` (shared by all prefixes; skips unchanged).
Old R2 prefixes can be deleted once no tab runs them (`wrangler r2 object delete`, or bulk via dashboard).
