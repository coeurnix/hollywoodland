# Hollywood 1930: Alien Occupation MMORPG — Game Design Bible

## 1. Overview: one zone built, a whole server implied

The game presents a complete classic MMORPG (13 zones, levels 1–60, 11 dungeons, 2 raids, about 300 players online) but builds only Hollywood Boulevard (levels 1–12) and one dungeon, the Egyptian Theatre (8–12). A judge plays under an hour and reaches about level 6. Everything past that is carried by data, the map, the interface and the other players.

**The rule for every feature:** if the player can stand in it, fight it or use it within an hour, build it. If they can only see, read or hear about it, it is data. If they can only infer it, the simulated players imply it.

### What is built and what is shown

| Feature | Built and playable | Shipped as data only | Where the player meets the data |
| --- | --- | --- | --- |
| Zones | Hollywood Boulevard | 12 zones: names, levels, map outlines, one-line blurbs | World map, /who, chat, zone exits, red-car route map |
| Dungeons and raids | The Egyptian Theatre | 10 dungeons, 2 raids: names, levels, boss names, loot tables | LFG shouts, raid talk, gear seen on inspect and in the auction house |
| Levels | 1–12, tuned | 13–60, from the section 5 formulas | Nameplates and /who show levels up to 60 |
| Classes | All 8, abilities to level 12 (4–6 each) | Full 18-ability lists and talent trees | Trainer windows list later abilities greyed out with their levels; talent pane opens at 10 |
| Items | Drops, rewards and vendor stock for levels 1–12 | All \~8,300 rows (section 8) | Auction house, inspect, item links in chat, loot rolls in groups |
| Mobs | 12 templates, 3 rares, 4 bosses | \~200 mob names and levels | Chat, quest breadcrumbs, guards at zone exits |
| Quests | \~30 in the zone, 3 in the dungeon | Titles only, \~45 per other zone | Chat ("anyone done the crypt keys quest in Melrose?"), breadcrumbs offered at level 12 |
| Professions | Salvaging and Gunsmithing to skill 75 | All 12 and their recipe lists | Trainer windows, materials on the auction house |
| Mounts | None for the player | Mount data | High-level players ride through the Boulevard (3 mount models) |
| Red cars | One stop at Highland | The 4-line network | Route map; simulated players board and ride away |
| Population | 40–60 characters in the zone | \~300 personas across all levels and zones | Everything in section 11 |

The Boulevard is the capital (bank, auction house, all trainers), which makes the facade cheap: in a classic MMO, players of every level pass through the capital constantly.

### Where the facade can break

| Risk | Answer |
| --- | --- |
| Player walks out of the zone | Every exit is held by elites 20+ levels above the player, at a hive wall (section 14). Death there feels like any classic zone border. |
| Player asks to be taken somewhere unbuilt | Simulated players decline in character: "ur 6 lol", "full sorry", "already saved this week". |
| Player inspects a level-60 | Real gear from data, in a look matching that gear. |
| Player watches high-level characters | They don't fight in the zone, apart from casually one-shotting a mob in passing. |
| Player asks about another zone | Answers come from the data blurbs and quest titles, never invented detail. |
| Player outlevels the zone (about 3 hours) | Breadcrumb quests at 12 point to Melrose and Gower Gulch; the exits stay gated. Acceptable for a jam. |

### Design rules

1. **Build only what an hour reaches.** Everything else ships as rows.
2. **Every reference resolves.** Any zone, item, boss or quest named in chat, on the map or on a character exists in the data tables.
3. **Real wherever defensible** (section 2).
4. **Classic conventions, no excuses.**
5. **The population is the showcase.**

### Conventions

- IDs are snake\_case with a type prefix: `zone_`, `hub_`, `dng_`, `mob_`, `item_`, `quest_`, `npc_`, `fac_`, `prof_`, `persona_`.
- Formulas use `L` for level. Numbers are starting values for tuning.
- **†** marks a historical detail from general knowledge, not yet checked against a source.
- Section 12 has the data schemas; section 13 the web build, the Cloudflare Worker and the local AI; section 14 the built zone and the agent work packages.

## 2. Legal basis for real content

Real names, places, products, organisations, people and 1930-or-earlier media all go in, under four rules. This is a research summary for a jam entry, not legal advice.

### Rule 1: Media published in 1930 or earlier is free to use

- US works published or registered in 1930 entered the public domain on 1 January 2026 ([Copyright Office](https://blogs.loc.gov/copyright/2025/12/lifecycle-of-copyright-1930-works-in-the-public-domain/)). That covers films, stills, posters, lobby cards, sheet music, books, newspapers and magazines from 1930 and earlier.
- Stand-alone sound recordings are free only through 1925. Sound inside a 1930-or-earlier film travels with the film ([Duke CSPD](https://cspd.law.duke.edu/publicdomainday/2026)).
- Use original release versions or faithful scans. Restorations that add new scores, colour or re-edits are protected in those additions.
- **Jam disclosure:** list every sourced asset in the submission, per Official Rules 15(c) as you've read it: sourced assets are allowed when disclosed, excluded from judging as original work, and legally cleared.

### Rule 2: Real people

| Group | What we can do | Basis |
| --- | --- | --- |
| Died before 11 Nov 1956 | Name, likeness, invented dialogue and actions | California's post-mortem right ends 70 years after death ([§ 3344.1(g)](https://rightofpublicity.com/statutes/california)) |
| Died 11 Nov 1956 or later | Name and a stylised likeness as characters in the world, with invented dialogue | § 3344.1(a)(2) exempts audiovisual works and other expressive works ([DMLP](https://www.dmlp.org/node/10636)); real people in expressive works are protected speech ([De Havilland v. FX, 2018](https://case-law.vlex.com/vid/havilland-v-fx-networks-884806264)) |

Limits for the second group: no photoreal digital replicas and no AI-cloned voices (California's 2024 digital-replica amendment, AB 1836†); don't make their famous act the gameplay (Chaplin as a tramp you control). Courts refused protection to games that recreated real people [doing exactly what made them famous](https://www.natlawreview.com/article/use-athlete-s-likeness-video-game-not-protected-first-amendment). And keep them off the jam thumbnail and video.

Nobody adult in 1930 Hollywood is alive today, so living-person rules don't arise.

### Rule 3: Trademarks: studios, businesses, products

- **Use them inside the world.** Real marks in an expressive work are protected when they are artistically relevant and not explicitly misleading about who made the game. A Los Angeles club in *GTA: San Andreas* ([E.S.S. v. Rock Star, 9th Cir. 2008](https://law.justia.com/cases/federal/appellate-courts/ca9/06-56237/0656237-2011-02-25.html)) and Humvees in *Call of Duty* ([AM General v. Activision, 2020](https://www.aipla.org/detail/news/2020/04/08/activision-beats-humvee-trademark-claims-over-call-of-duty)) both passed.
- **Never use one as the game's own brand.** That protection does not apply when a mark identifies the source of your own product ([Jack Daniel's v. VIP, 2023](https://www.jdsupra.com/legalnews/supreme-court-tightens-the-leash-on-5492577)).
- In practice: "Paramount" on the studio gate, a Colt revolver, Coca-Cola in a vending machine, the HOLLYWOODLAND sign on the hill: yes. A studio logo in the game's title, menu branding or jam thumbnail: no.

### Rule 4: Organisations

Real organisations (LAPD, American Legion Post 43, the studio unions, Angelus Temple, WCTU, the Communist Party) appear as factions on the same basis as people and marks. Their 1930 conduct is shown as documented; invented actions stay consistent with that record.

## 3. Setting

Insect invaders have sealed Hollywood and the east San Fernando Valley under a transparent dome and taken over the studios. That is the whole premise; everything else is places, enemies and loot.

### Premise

- **The Dome.** Transparent, impassable, its edge is the edge of the map. Nothing gets in or out.
- **The invaders.** Insects from one to four metres long, organised as a hive. They grow hive structures inside captured buildings and round up people to keep the studios running. Why they want films is not explained and doesn't need to be.
- **Who holds what.** Studios are invader strongholds and dungeons. Streets are contested. Hills and parks are wild country. Each human organisation holds its own hub.
- **Everyone fights.** Police, veterans, unions, churches, bootleggers and Communists all resist, separately and grudgingly. There is no war between factions, only reputation.
- **Time.** Always autumn 1930, with a day/night cycle. No calendar advances.
- **Technology.** 1930 tools and weapons, then salvaged invader parts in higher-level gear.

### 1930 facts the world is built on

| Fact | Use in the game | Source |
| --- | --- | --- |
| Paramount's lot on Marathon St between Gower and Van Ness, south of Hollywood Cemetery; RKO next door at Gower and Melrose | Melrose zone; Paramount dungeon | [LA Conservancy](https://www.laconservancy.org/learn/historic-places/paramount-pictures/) |
| Columbia at Sunset and Gower; costumed cowboy extras gave Gower the name "Gower Gulch" | Gower Gulch zone (data only) | [Wikipedia](<https://en.wikipedia.org/wiki/Gower_Street_(Los_Angeles)>) |
| Warner Bros. on Sunset; Fox's old lot on Western Ave; Fox's new lot at Fox Hills | East Hollywood zone | [World Film Encyclopedia, 1933](https://lantern.mediahist.org/catalog/worldfilmencyclo1933clar_0437) |
| Egyptian, El Capitan, Grauman's Chinese, Warner Hollywood and Pantages all opened 1922–1930 on Hollywood Blvd | The capital and only built zone | [Water and Power Associates](<https://waterandpower.org/museum/Early_Views_of_Hollywood_(1920_+)_3_of_12.html>) |
| *Hell's Angels* premiered at Grauman's Chinese in May 1930 | The film plays there on a loop | same |
| The sign read HOLLYWOODLAND until 1949 | Landmark on Mount Lee | [Filmsite](https://mail.filmsite.org/1923-filmhistory.html) |
| Stembridge Gun Rentals kept the industry's firearms, Thompsons included, at 5451 Marathon St | Gunsmithing trainer and vendor | [Wikipedia](https://en.wikipedia.org/wiki/Stembridge_Gun_Rentals) |
| American Legion Post 43 clubhouse, 2035 N. Highland, opened 4 July 1929; Legion boxing stadium at El Centro and Selma; a captured German field gun | Faction hub; weekly boxing event | [LA Daily Mirror](https://ladailymirror.com/2013/05/06/mary-mallory-hollywood-heights-the-hollywood-american-legion-the-house-that-boxing-built/) |
| Angelus Temple (5,300 seats) and its radio station KFSG in Echo Park | Faction hub in Echo Park (data only) | [American Experience](https://www.pbs.org/wgbh/americanexperience/features/sister-angelus-temple/) |
| LAPD Red Squad under William Hynes broke up left-wing meetings | LAPD quests; opposes the Communist faction | [Wikipedia](https://en.wikipedia.org/wiki/Red_Hynes) |
| Charlie Crawford's vice syndicate; Jack Dragna's rival family; Tony Cornero running liquor by sea | Bootlegger faction; hostile gang mobs | [Wikipedia](https://en.wikipedia.org/wiki/Charles_H._Crawford) |
| Communist Party had about 300 members in California, Arizona and Nevada combined | Small faction, East Hollywood | [Daily Worker, May 1930](https://revolutionsnewsstand.com/2025/03/21/reorganization-in-the-los-angeles-party-by-c-clark-from-the-daily-worker-vol-6-no-370-may-14-1930/) |
| Prohibition in force; Production Code adopted 1930 | Liquor is contraband; flavour text | [Filmsite](https://www.filmsite.org/30sintro.html) |

Also present, from general knowledge†: Roosevelt Hotel, Hollywood Hotel, Musso & Frank, the Brown Derby on Vine, Chaplin Studios on La Brea, the Pickford-Fairbanks studio on Santa Monica Blvd, Disney's Hyperion Ave studio, Universal City and its Stage 28, the First National lot and Lockheed plant in Burbank, United Airport (opened 1930), Griffith Park Zoo, the Greek Theatre (opened 1930), Mulholland Dam and Hollywood Reservoir, the Pacific Electric red-car lines.

**Not yet in existence (keep out):** Griffith Observatory (1935), the Hollywood Walk of Fame, the Screen Actors Guild (1933), Bugsy Siegel in LA, the Silver Legion (1933), the German American Bund (1936).

## 4. World

Thirteen zones on the real street grid, compressed about four times, with one starting zone, eleven dungeons, two raids and a red-car network as the flight paths. All are open from the start.

**Built for the jam:** only Hollywood Boulevard and the Egyptian Theatre. The other twelve zones, ten dungeons and two raids exist as data and are seen but never entered (section 1).

### Zones

| ID | Zone | Levels | Real area | Hubs (real locations) |
| --- | --- | --- | --- | --- |
| zone\_hollywood\_blvd | Hollywood Boulevard | 1–10 | Hollywood Blvd, La Brea to Vine | Grauman's Chinese forecourt; Hollywood & Vine; Hollywood police station† |
| zone\_gower\_gulch | Gower Gulch | 8–16 | Sunset & Gower, Poverty Row | Columbia gate; Legion boxing stadium; Christie studio† |
| zone\_echo\_park | Echo Park & Edendale | 10–18 | Echo Park, Edendale | Angelus Temple; Echo Park boathouse†; Edendale red-car stop† |
| zone\_melrose | Melrose | 10–20 | Melrose, Gower to Van Ness; Santa Monica Blvd | Hollywood Cemetery gate; Van Ness & Santa Monica |
| zone\_east\_hollywood | East Hollywood | 12–22 | Sunset & Western to Vermont | Warner Bros. gate; Fox lot gate; Vitagraph lot† |
| zone\_griffith\_park | Griffith Park & Los Feliz | 15–25 | Griffith Park, Los Feliz, Hyperion Ave | Greek Theatre†; Disney Hyperion studio†; Vermont Canyon camp |
| zone\_west\_hollywood | West Hollywood & the Strip | 20–30 | La Brea to the county Strip | La Brea & Sunset; Chateau Marmont†; Pickford-Fairbanks studio† |
| zone\_hollywoodland | Hollywoodland | 25–35 | Beachwood Canyon, Lake Hollywood | Hollywoodland gates†; Krotona buildings; dam keeper's house |
| zone\_cahuenga\_pass | Cahuenga Pass | 30–40 | Highland north, the Pass road | Post 43 clubhouse; Hollywood Bowl; Pass summit |
| zone\_universal\_city | Universal City | 35–45 | Universal lot and backlot† | Universal front gate; backlot European street |
| zone\_studio\_city | Studio City & the River | 40–50 | Ventura Blvd, LA River† | Laurel Canyon crossing; river camp |
| zone\_burbank | Burbank | 45–55 | First National lot, Lockheed plant, United Airport† | Burbank town centre; airport terminal |
| zone\_mount\_lee | Mount Lee | 55–60 | Mount Lee and the ridge behind the sign | Forward camp below the HOLLYWOODLAND sign |

**Each zone targets:** 3–4 hubs, 40–50 quests, 15–25 mob types, 6–10 rare spawns, 60–100 gathering nodes, one inn, one aid station per 300 m of road.

**Hollywood Boulevard is the capital:** Roosevelt Hotel (inn), First National Bank building at Hollywood & Highland (bank), Taft Building† (auction house), class trainers for all eight classes.

### Dungeons and raids

As in classic WoW, some dungeons sit in lower-level zones (Paramount in Melrose).

| ID | Name | Zone | Levels | Size | Bosses |
| --- | --- | --- | --- | --- | --- |
| dng\_egyptian | The Egyptian Theatre (built) | Hollywood Boulevard | 8–12 | 5 | 4 |
| dng\_cemetery\_crypts | Hollywood Cemetery Crypts | Melrose | 13–18 | 5 | 4 |
| dng\_warner\_stages | Warner Bros. Sunset Stages | East Hollywood | 18–24 | 5 | 5 |
| dng\_griffith\_zoo | Griffith Park Zoo | Griffith Park | 21–27 | 5 | 5 |
| dng\_chaplin\_studios | Chaplin Studios | West Hollywood | 26–32 | 5 | 5 |
| dng\_mulholland\_dam | Mulholland Dam | Hollywoodland | 29–35 | 5 | 6 |
| dng\_paramount | Paramount Studios | Melrose | 34–40 | 5 | 7 |
| dng\_stage\_28 | Universal Stage 28 | Universal City | 40–46 | 5 | 6 |
| dng\_sennett\_studio | Mack Sennett Studio | Studio City | 45–50 | 5 | 6 |
| dng\_lockheed | Lockheed Plant | Burbank | 50–56 | 5 | 7 |
| dng\_hive\_tunnels | Hive Tunnels | Mount Lee | 56–60 | 5 | 8 |
| raid\_united\_airport | United Airport Hangars | Burbank | 60 | 20 | 8 |
| raid\_mount\_lee | Mount Lee Hive | Mount Lee | 60 | 40 | 10 |

Dungeon bosses are invader types from section 7 at boss rank, plus one human or animal boss per dungeon where the place suggests it (a gorilla in the zoo, a Dragna lieutenant in the Paramount gun room).

### Travel

Pacific Electric red cars are the flight paths: discover a stop to use it, pay a fare, ride in real time on the rails. Lines†:

| Line | Stops |
| --- | --- |
| Hollywood Blvd | West Hollywood – Hollywood Blvd – East Hollywood |
| Santa Monica Blvd | West Hollywood – Melrose |
| Glendale–Burbank via Edendale | Echo Park – Griffith Park – Burbank |
| San Fernando Valley via Cahuenga Pass | Hollywood Blvd – Cahuenga Pass – Universal City – Studio City |

Fare: 5¢ per stop. Mounts are covered in section 6.

&#91;embedded content: zone map · 13 zones, levels 1–60\]

Every character starts on Hollywood Boulevard, the capital and the only zone built for the jam. Every road out of it is held by elites far above a new player's level (section 14).

## 5. Characters

Five attributes, eight classes with three specs each, a level cap of 60 and a WoW-classic talent tree. No races.

### Creation

Name; body type; face (12 presets); hair (12); skin tone (8); clothing colour set (8). Every character starts on Hollywood Boulevard.

### Attributes and derived values

| Attribute | Effect |
| --- | --- |
| Strength | Melee attack power |
| Agility | Ranged attack power, physical crit, dodge |
| Stamina | Health |
| Intellect | Power pool, spell crit, ability damage and healing |
| Spirit | Regeneration out of combat |

| Derived value | Formula |
| --- | --- |
| Health | 20 + hpPerLevel × L + 10 × Stamina |
| Power (caster classes) | 18 × L + 15 × Intellect |
| Melee attack power | 2 × Strength (Veteran, Mechanic); Strength + Agility (Stuntman) |
| Ranged attack power | 2 × Agility |
| Ability bonus (casters) | 0.5 × Intellect × ability coefficient |
| Physical crit | 5% + Agility / 20 |
| Spell crit | 5% + Intellect / 60 |
| Dodge | 5% + Agility / 20 |
| Armour damage reduction | Armour / (Armour + 400 + 85 × attacker level), cap 75% |
| Out-of-combat regen per 5 s | Power: 5 + Spirit / 2. Health: Spirit / 2 |

Each level adds 2 to the class's primary attribute, 1 to its secondary and 0.5 to the rest.

### Classes

| Class | Roles | Armour | Weapons | Resource | hpPerLevel | Primary / secondary | Specs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Veteran | Tank, melee | Steel | 1H and 2H melee, shields, shotguns | Adrenaline | 22 | Str / Sta | Sergeant (tank), Infantry (2H), Brawler (dual wield) |
| Stuntman | Melee | Leather | Knives, clubs, fist weapons, pistols | Energy | 17 | Agi / Str | Fighter, Knife Work, Falls (evasion) |
| Gunman | Ranged | Padded | Rifles, shotguns, revolvers, submachine guns | Focus | 17 | Agi / Sta | Marksman, Trick Shot, Hunter (dog companion) |
| Electrician | Caster | Cloth | Wrenches, tools; lamp off-hand | Power | 12 | Int / Spi | Arc (single target), Floodlight (area), Generator (shields) |
| Powder Man | Caster | Cloth | Wrenches, tools; lamp off-hand | Power | 12 | Int / Sta | Fire (damage over time), Smoke (control), Demolition (area) |
| Nurse | Healer | Cloth | Knives, clubs; medical bag off-hand | Power | 13 | Int / Spi | Triage (burst heals), Medicine (heals over time), Chemistry (poison damage) |
| Preacher | Healer, caster | Leather | Clubs, canes; Bible off-hand | Power | 14 | Spi / Int | Gospel (group heals), Temperance (shields), Hellfire (damage) |
| Mechanic | Melee, off-tank, support | Padded | Wrenches, 2H hammers, shotguns | Power | 19 | Str / Int | Gadgets (deployables), Heavy Tools (melee), Repair (heal-over-time kits) |

"Powder man" was the studio name for the effects crew who handled explosives.† Resources: Adrenaline fills from damage dealt and taken and drains out of combat; Energy and Focus are 100-point pools regenerating 10 and 5 per second.

### Abilities and talents

- **18 active abilities per class**, learned from trainers. A new ability or rank every 2 levels, priced at 10¢ × L.
- **Ranks:** each damage or heal ability has 5–10 ranks; rank values follow `base × (1 + 0.12 × rankLevel)`.
- **Talents:** one point per level from 10 (51 total), three trees of 16 talents, 5 tiers unlocked every 5 points spent. Respec at a trainer: $1, doubling per respec up to $50.

### Levelling

| Value | Formula | At L1 | At L30 | At L59 |
| --- | --- | --- | --- | --- |
| XP to next level | 40L² + 360L | 400 | 46,800 | 160,480 |
| XP per same-level kill | 45 + 5L | 50 | 195 | 340 |
| Kills per level if no quests | — | 8 | 240 | 472 |

Total XP to 60 is about 3.45 million. Kill XP changes by ±5% per level of difference and drops to zero once the mob is grey (more than 5 levels below at L30). Quests should supply about 60% of XP. Rested XP: double kill XP, accrues 5% of a level per 8 hours offline in an inn, cap 150% of a level. Target playtime: 15 minutes for level 2, about 4 hours for level 59, roughly 120 hours to 60.

## 6. Combat, death, groups, mounts

Classic tab-target rules throughout. Simulated players use exactly the same rules as the human.

### Combat rules

| Rule | Value |
| --- | --- |
| Global cooldown | 1.5 s |
| Auto-attack | One swing per weapon speed; ranged auto-shot consumes one round of ammunition |
| Weapon hit damage | weapon roll + attack power / 14 × weapon speed |
| Ability damage or healing | rank base + ability bonus (section 5) |
| Attack table, in order | miss 5% (+1% per level the target is above you) → dodge → parry (front only) → block (shield) → crit (×2) → hit |
| Threat | 1 per point of damage; healing 0.5 per point, split across enemies in combat; tank specs ×1.3 |
| Aggro radius | 10 m + 1 m per level the mob is above you, −1 m per level below, minimum 3 m |
| Leash | Mobs reset and heal fully 40 m from their spawn point |
| Eating and drinking | Sit to restore health (food) or Power (coffee, soda) over 18 s |

### Death

1. You die and drop where you fell; equipped items lose 10% durability.
2. Release to the nearest aid station. Each zone has 2–4, set in real hospitals, churches and fire stations†.
3. Either run back as a ghost and revive at your body with 50% health and Power, or revive at the station at the cost of a further 25% durability and 10 minutes of −25% attributes.
4. Repair at any armour or weapon vendor. Cost: 1¢ × item level per 10% durability.

There is no in-world explanation for respawning.

### Groups

- **Party:** 5. **Raids:** 20 or 40.
- **Loot:** round robin for grey and white items; need-or-greed rolls for green and above; master looter in raids.
- **Finding a group:** shouts in the zone channel and a group board at each capital hub. Every invite comes from a simulated player or the human; nothing is auto-matched.
- **Duels:** with any simulated player, anywhere outside a hub. No battlegrounds in scope.

### Mounts

| Level | Speed bonus | Cost | Examples |
| --- | --- | --- | --- |
| 40 | +60% | $80 | Saddle horse, Indian Scout motorcycle, Ford Model A |
| 60 | +100% | $900 | Thoroughbred, Harley-Davidson VL, Duesenberg Model J, Cord L-29 |

All mounts behave the same off-road; only the model and sound differ. Mount trainers: Gower Gulch stables (horses), a Hollywood Blvd garage (cars and motorcycles). Faction mounts at Exalted (section 10).

## 7. Enemies

Three families: invaders, wildlife and hostile humans. Every mob is a family template plus a level and a rank, so a few dozen templates yield the 200+ mob types the world needs.

### Base stats by level (normal rank)

| Level | Health: 42 + 15L + 0.8L² | Hit (2.0 s swing): 3 + 1.1L + 0.025L² | Armour: 20L | XP: 45 + 5L | Money: about 1.5L ¢ |
| --- | --- | --- | --- | --- | --- |
| 1 | 58 | 4 | 20 | 50 | 2¢ |
| 10 | 272 | 17 | 200 | 95 | 15¢ |
| 20 | 662 | 35 | 400 | 145 | 30¢ |
| 30 | 1,212 | 59 | 600 | 195 | 45¢ |
| 40 | 1,922 | 87 | 800 | 245 | 60¢ |
| 50 | 2,792 | 121 | 1,000 | 295 | 75¢ |
| 60 | 3,822 | 159 | 1,200 | 345 | 90¢ |

Money drops on 70% of humanoid and invader kills, never from animals.

### Ranks

| Rank | Health | Damage | XP | Respawn | Loot |
| --- | --- | --- | --- | --- | --- |
| Normal | ×1 | ×1 | ×1 | 5 min | Normal table |
| Elite | ×3 | ×1.5 | ×2 | 8 min | +50% green chance |
| Rare | ×4 | ×1.6 | ×3 | 45–90 min | One green guaranteed |
| Dungeon boss | ×12 | ×2.5 | ×4 | On reset | Two blues from a 4–6 item table |
| Raid boss | Per encounter, about ×60 (20-player) or ×120 (40-player) | ×4 | ×5 | Weekly | 3–5 epics |

### Families

Modifiers multiply the base stats.

| Family | Type | Health | Damage | Armour | Behaviour |
| --- | --- | --- | --- | --- | --- |
| Invader Worker | Invader | 0.6 | 0.5 | 0.5 | Non-aggressive; flees at 30%; drops salvage |
| Invader Drone | Invader | 1.0 | 1.0 | 1.0 | Melee; pulls neighbours within 12 m |
| Invader Spitter | Invader | 0.8 | 0.9 | 0.8 | Ranged, 30 m; acid that deals 3 × 20% of a hit over 6 s |
| Invader Soldier | Invader | 1.3 | 1.1 | 2.0 | Melee; immune to knockback |
| Invader Burrower | Invader | 0.9 | 1.2 | 1.0 | Hidden underground; first hit does double |
| Invader Flier | Invader | 0.8 | 1.0 | 0.7 | Flies; dives every 12 s |
| Invader Broodling | Invader | 0.25 | 0.35 | 0.5 | Groups of 6–10 |
| Invader Brute | Invader (elite only) | 1.2 | 1.2 | 1.5 | Knockback every 10 s |
| Invader Queen | Invader (boss only) | Per encounter |  |  |  |
| Coyote | Wildlife | 0.9 | 0.9 | 0.8 | Packs of 2–3; hills and parks |
| Rattlesnake | Wildlife | 0.7 | 0.8 | 0.6 | Poison; brush |
| Mountain lion | Wildlife | 1.1 | 1.2 | 0.9 | Stealthed until it attacks |
| Black bear | Wildlife | 1.4 | 1.1 | 1.2 | Slow |
| Mule deer | Wildlife | 0.8 | 0.5 | 0.8 | Passive; hides for skinning |
| Red-tailed hawk | Wildlife | 0.7 | 0.8 | 0.5 | Flies |
| Tarantula | Wildlife | 0.6 | 0.7 | 0.6 | Poison |
| Feral dog | Wildlife | 0.9 | 0.9 | 0.7 | Packs of 3–4; streets |
| Escaped zoo animal | Wildlife | 1.0–1.6 | 1.0–1.4 | 1.0 | Lions, bears, apes; Griffith Park only |
| Looter | Human | 1.0 | 1.0 | 1.0 | Pistols or clubs; any street zone |
| Dragna Gunman | Human | 0.9 | 1.1 | 0.9 | Ranged; West Hollywood, Studio City |
| Dragna Enforcer | Human | 1.2 | 1.2 | 1.2 | Melee |

**Hive-ridden variants.** From level 25, any wildlife family can appear parasitised: +20% damage, drops chitin, otherwise the same.

### Naming

- **Invaders:** family name, prefixed by level band: none (1–29), *Hardened* (30–44), *Elder* (45–60). "Elder Invader Spitter".
- **Wildlife:** species name, plus *Hive-ridden* where it applies.
- **Rares:** a plain descriptive name and a species line: "Old Scar, coyote", "The Selma Avenue Burrower". 6–10 per zone.

### Placement

- Camps of 3–6 mobs, about one mob per 400 m² in questing areas.
- Zone mix: invaders 60%, wildlife 25%, humans 15% on the flats; in the hills, wildlife 60%.
- Hive structures are objects, not mobs: egg clusters (quest targets, respawn 3 min) and hive spires (spawn a Drone every 30 s until destroyed).

## 8. Items

About 8,000 items in the full game, nearly all generated: hand-checked base items × 13 level bands × quality × suffix. Every number on an item comes from the formulas below, so an agent can produce any item from a base row and a level.

### Slots

Hat, Neck, Coat (chest), Shirt (cosmetic), Wrist, Gloves, Belt, Trousers, Shoes, Ring ×2, Pocket item ×2 (trinkets), Main hand, Off hand, Ranged. Bags: 4 slots, sizes 6–16 (satchel, suitcase, steamer trunk).

### Armour types

| Type | Worn by | Examples | Armour multiplier |
| --- | --- | --- | --- |
| Cloth | All | Suits, dresses, shirts, wool trousers | 1 |
| Leather | Stuntman, Preacher and heavier | Work jackets, riding boots, gauntlets | 2 |
| Padded | Gunman, Mechanic, Veteran | Canvas work gear, quilted vests, football pads | 4 |
| Steel | Veteran | M1917 helmets, steel vests, plate inserts | 7 |

From level 45, chitin pieces appear in all four types; chitin is a material, not a fifth type.

### Weapon types

| Type | Hands | Speed (s) | Real base examples |
| --- | --- | --- | --- |
| Knife | 1H | 1.7 | Pocketknife, bayonet (M1905), straight razor |
| Fist | 1H | 1.5 | Brass knuckles |
| Club | 1H | 2.4 | Police nightstick, blackjack, Louisville Slugger bat |
| Tool | 1H | 2.6 | Pipe wrench, crowbar, hatchet |
| Sabre | 1H | 2.4 | M1913 cavalry sabre, prop fencing sabre |
| Heavy tool | 2H | 3.5 | Sledgehammer, fire axe, pickaxe, trench shovel |
| Revolver | Ranged | 2.2 | Colt Police Positive, Smith & Wesson .38 Military & Police, Colt Single Action Army |
| Pistol | Ranged | 2.0 | Colt M1911 |
| Rifle | Ranged | 3.0 | Springfield M1903, Winchester Model 1894, Remington Model 8 |
| Shotgun | Ranged | 3.2 | Winchester Model 1897, Remington Model 11 |
| Submachine gun | Ranged | 1.6 | Thompson M1928 |
| Automatic rifle | Ranged, 2H | 2.6 | Browning M1918 |
| Off-hand | — | — | Riot shield†, work lamp, medical bag, Bible |

Ranged weapons fire ammunition: .38, .45 ACP, .44-40, .30-06 and 12-gauge, in stacks of 200. Better ammunition adds 0.15 × its item level in damage per second.

### Quality and drop rates

| Quality | Colour | Source | Drop chance per kill |
| --- | --- | --- | --- |
| Poor | Grey | Vendor junk by family ("Cracked Chitin Plate") | 40% |
| Common | White | Vendors, drops, trade goods | 20% |
| Uncommon | Green | Drops, quests, crafting | 1.5% (elite 2.25%) |
| Rare | Blue | Dungeons, rares, quest-chain ends, crafting | 0.08% in the open world |
| Epic | Purple | Raids, very rare world drops | 0.003% in the open world |

### Formulas

| Value | Formula |
| --- | --- |
| Required level | Start of the item's level band (1, 5, 10 … 55, 60) |
| Item level (ilvl) | Required level + 5 for green and blue; 60–76 for epics |
| Stat points | round(ilvl × quality factor × slot factor) |
| Quality factor | Green 0.5, blue 0.65, epic 0.8 (white and grey have no stats) |
| Slot factor | Hat, coat, trousers, 2H weapon 1.0; gloves, belt, shoes 0.75; wrist, neck, ring, off hand 0.55; 1H weapon 0.45; ranged 0.35 |
| Armour | ilvl × type multiplier × slot factor × (white 1.0, green 1.2, blue 1.4, epic 1.6) |
| Weapon damage per second | (1.5 + 0.55 × ilvl) × (white 0.9, green 1.0, blue 1.12, epic 1.25) × (2H 1.3, ranged 0.8) |
| Weapon hit range | DPS × speed × 0.8 to DPS × speed × 1.2 |
| Vendor sell price | 1¢ × ilvl² / 10 × (grey 0.5, white 1, green 2, blue 5, epic 12) |

Worked example: a level-60 blue coat (ilvl 65) has 42 stat points and, in steel, 637 armour.

Pocket items carry one on-use effect each (a flask that restores health, a lighter that sets a target burning) instead of a stat budget. Blues and epics may swap up to 25% of their stat points for an on-equip effect from a fixed list of 30 (crit, haste, health regeneration and so on).

### Green suffixes

| Suffix | Stats |
| --- | --- |
| of Strength, of Agility, of Stamina, of Intellect, of Spirit | 100% to one |
| of the Boxer | Strength 60%, Stamina 40% |
| of the Acrobat | Agility 60%, Stamina 40% |
| of the Stevedore | Strength 60%, Agility 40% |
| of the Marksman | Agility 60%, Intellect 40% |
| of the Engineer | Intellect 60%, Stamina 40% |
| of the Doctor | Intellect 60%, Spirit 40% |
| of the Deacon | Spirit 60%, Stamina 40% |

### Naming

- **Grey and white:** condition + material + item. "Worn Wool Trousers", "Patched Leather Gloves".
- **Green:** base item + suffix. "Florsheim Oxfords of the Acrobat".
- **Blue and epic:** individually named, usually after a real place or a pre-1956 person. "Grauman's Usher Coat", "Cornero's Rum-Runner Slicker".
- **Real brands and models** may name base items if they existed by 1930: Stetson, Florsheim, Arrow, Levi Strauss, Carhartt, Pendleton, Hart Schaffner & Marx, Everlast, Elgin and Hamilton watches, Johnson & Johnson first-aid kits. The generator rejects any model introduced after 1930.

### Consumables

Food and drink in 6 tiers (hot dogs to steak dinners; coffee, Coca-Cola, Hires root beer); bandages (First Aid); tonics and elixirs (Pharmacy); explosives and flash powder (Electrics); bootleg liquor (+5% Strength and Stamina, −10% hit chance, 10 minutes).

### Counts

Every item row ships, even though only levels 1–12 are playable: the auction house, inspect windows, loot links in chat and vendor lists show items from all 60 levels. Icons: one per base item (\~700), generated. 3D appearance comes from a small set of looks, not one mesh per item.

| Category | Data rows (all ship) | Seen in 3D |
| --- | --- | --- |
| Base armour and jewellery | \~440 | Through 24 visual looks (4 armour types × 6 tiers) |
| Base weapons | \~240 | \~40 weapon meshes, shared by type |
| Green variants | \~5,400 | Their base item's look |
| Blues | \~700 | Base look plus a tint |
| Epics | \~150 | 8 epic looks, worn by high-level transients |
| Consumables | \~250 | Icons only |
| Trade goods and reagents | \~200 | Icons only |
| Quest items | \~650 (about 30 used in the slice) | Icons only |
| Cosmetics and collectibles | \~300 | \~20 |
| **Total** | **\~8,300** |  |

## 9. Economy and professions

One currency, two primary professions per character, three secondary professions for everyone, and an auction house the simulated players actually use.

### Money

Dollars and cents (100¢ = $1), standing in for WoW's gold, silver and copper. Prices are tuned for play, not to match 1930: a level-60 mount costs $900, though a real Model A cost about $500†.

| Sink | Cost |
| --- | --- |
| Ability training | 10¢ × L per rank |
| Repairs | 1¢ × ilvl per 10% durability |
| Mounts | $80 at 40, $900 at 60 |
| Auction house | 5% deposit (lost if unsold), 5% cut on sale |
| Red-car fares | 5¢ per stop |
| Bank slots | $1, $5, $10, $25, $50, $100 |

**Income target:** about $1 per hour at level 20, $15 per hour at 40, $40 per hour at 60, from drops, vendor sales and quests.

### Auction house

In the Taft Building† at Hollywood & Vine; listings of 12, 24 or 48 hours. Simulated players list what their simulated drops and crafts produce, so the stock follows the population's level spread. Each item's market price is its vendor price × a demand multiplier between 1.5 and 8, which rises when many simulated players sit in that item's level band and falls as listings pile up.

### Professions

Skill 1–300 in four ranks (Apprentice 75, Journeyman 150, Expert 225, Master 300). Recipes: 80–120 per crafting profession.

| ID | Profession | Kind | Makes or gathers | Trainers (real locations where possible) |
| --- | --- | --- | --- | --- |
| prof\_salvage | Salvaging | Gathering | Scrap metal from wrecks; chitin and glands from dead invaders | Every hub |
| prof\_forage | Foraging | Gathering | Native plants: white sage, yerba santa, California poppy, mugwort, elderberry | Griffith Park, Hollywoodland |
| prof\_skin | Skinning | Gathering | Hides from coyotes, deer, bears | Gower Gulch stables |
| prof\_guns | Gunsmithing | Crafting | Firearms, ammunition, sights | Stembridge Gun Room (Paramount lot) |
| prof\_metal | Metalworking | Crafting | Steel armour, melee weapons | Studio machine shops |
| prof\_leather | Leatherworking | Crafting | Leather and padded armour | Gower Gulch saddlery |
| prof\_tailor | Tailoring | Crafting | Cloth armour, bags | Studio wardrobe departments |
| prof\_pharm | Pharmacy | Crafting | Tonics, elixirs, poisons | Drugstores on the Boulevard |
| prof\_electric | Electrics | Crafting | Lamps, gadgets, explosives, mount upgrades | Studio electrical departments |
| prof\_cook | Cooking | Secondary | Food with attribute buffs | Musso & Frank kitchen |
| prof\_firstaid | First Aid | Secondary | Bandages | Aid stations |
| prof\_fish | Fishing | Secondary | Fish for cooking; rare items | Echo Park Lake, Lake Hollywood, LA River |

**Gathering nodes** respawn in 10–20 minutes. Each zone carries the materials of its level band (nodes are tagged with a skill requirement: 1, 50, 100 … 275). Simulated players gather too, and sometimes reach a node first.

**Crafted gear** is one band below dungeon gear: crafted blues match dungeon blues 10 levels lower.

## 10. Quests, reputation and world events

About 700 quests, all local: each hub hands out work about its own few blocks. Chains are short, nothing is required, and no quest carries world-ending stakes.

### Quests

| Type | Share | Example |
| --- | --- | --- |
| Kill N | 35% | Clear 10 Invader Drones from the Egyptian Theatre's courtyard |
| Collect from drops | 25% | 8 Spitter Glands for the drugstore on Highland (drop rate 40–60%) |
| Collect from the world | 10% | 6 reels of film from the wrecked trucks on Gower |
| Deliver or talk | 12% | Take the Legion's ammunition count to the police station |
| Use an item on a target | 8% | Set charges on 4 hive spires |
| Escort | 5% | Walk a studio carpenter from Columbia to the Christie lot |
| Group (elite) | 5% | Kill the Brute nesting in the Hollywood Hotel's ballroom |

- **Per zone:** 40–50 quests across its hubs; chains of 2–4; one group quest per hub.
- **Per dungeon:** 3–5 quests, picked up in the zone outside.
- **Repeatables:** reputation turn-ins (10 items → 250 reputation) and profession dailies.
- **Rewards:** XP ≈ 8 × a same-level kill per step (×12 at a chain's end); 10¢ × quest level; a choice of 2–3 greens at chain ends; reputation with the hub's faction.
- **Text:** 60–120 words, plain period voice, giver's first person. No chosen-one framing.

### Quest givers

Most givers are functional and invented: "Patrolman Joe Kearney", "Mrs. Ida Walsh, WCTU", "Carpenter, Columbia lot". About 15 real people, all dead before November 1956, give quests or sell goods where they really were†:

| Person (died) | Place | Function |
| --- | --- | --- |
| Sid Grauman (1950) | Grauman's Chinese | Hollywood Blvd hub quests |
| Chief James E. Davis (1949) | Hollywood police station | LAPD faction leader |
| Capt. William "Red" Hynes (1952) | Police station | LAPD quests against the Communist faction |
| Aimee Semple McPherson (1944) | Angelus Temple | Temple faction leader |
| Charles H. Crawford (1931) | Sunset Blvd office | Bootlegger faction leader |
| Tony Cornero (1955) | West Hollywood | Contraband vendor |
| Jack Dragna (Feb 1956) | Studio City | Never met; the Dragna gang's mobs carry his name |
| Max Factor (1938) | Highland Ave | Pharmacy trainer, cosmetic vendor |
| Tom Mix (1940) | Gower Gulch stables | Horse mounts |
| Will Rogers (1935) | Gower Gulch | Rope and riding quests |
| Carl Laemmle (1939) | Universal front gate | Universal City hub quests |
| Lon Chaney (Aug 1930) | Universal Stage 28 | Dungeon quests |
| Bela Lugosi (Aug 1956) | Universal City | Quests |
| Wallace Beery (1949) | United Airport | Raid attunement quest |
| Rin Tin Tin (1932) | Warner Bros. gate | A dog. Follows the player for one quest |

People who died later (Chaplin, Pickford, DeMille, Hughes, Disney) may appear by name on marquees, posters, newspapers and in dialogue, under the limits in section 2.

### Reputation factions

Tiers: Hostile, Unfriendly, Neutral, Friendly (3,000), Honored (6,000), Revered (12,000), Exalted (21,000). Gaining reputation with a faction costs half as much with any faction opposed to it. At Hostile, that faction's guards attack.

| ID | Faction | Hub | Opposed by | Exalted reward |
| --- | --- | --- | --- | --- |
| fac\_lapd | Los Angeles Police Department | Hollywood police station | Bootleggers, Communists | Police Ford Model A mount |
| fac\_post43 | American Legion, Hollywood Post 43 | Post 43 clubhouse | Communists | Springfield M1903 (epic) |
| fac\_unions | Studio unions (IATSE, studio electricians) | Union hall† | — | Electrics and Metalworking recipes |
| fac\_temple | Angelus Temple | Angelus Temple | Bootleggers | Healer and Preacher gear |
| fac\_wctu | Woman's Christian Temperance Union | Hollywood chapter† | Bootleggers | Pharmacy recipes |
| fac\_bootleggers | Crawford's syndicate | Sunset Blvd office, Strip tunnels | LAPD, WCTU, Temple | Thompson M1928 (epic), contraband vendor |
| fac\_communists | Communist Party, Unemployed Council | East Hollywood hall† | LAPD, Post 43 | Cloth and leather gear, group buffs |
| fac\_extras | Gower Gulch extras and Central Casting | Gower Gulch | — | Costumes, horse mount |

### Recurring world events

These run on timers and at random, whether or not the player is nearby. Simulated players join them and talk about them.

| Event | When | What happens |
| --- | --- | --- |
| Hive surge | Random, 1–2 per zone per day | A hub is attacked for 10 minutes; defend it or lose its vendors for an hour |
| Hive spread | Continuous | Unchecked egg clusters spread to the next camp; spawn density rises until players clear them |
| Rare spawn | 45–90 min per rare | As section 7 |
| Picture night | Every evening, Grauman's Chinese | A public-domain film screens on the real screen; social gathering; free buff |
| Fight night | Weekly, Legion Stadium | Duel ladder against simulated players |
| Fishing derby | Weekly, Echo Park Lake | Two hours; biggest catch wins a rare mount |
| Red-car breakdown | Random | A line stops; escort the repair crew |

## 11. Simulated player population

About 300 personas spread across all 60 levels and 13 zones, with 40–60 of them on the Boulevard at any moment. High-level characters passing through the capital are what make the unbuilt 95% of the world feel real.

### Who is on the Boulevard

| Group | Share present | Levels | Behaviour |
| --- | --- | --- | --- |
| Locals | 60% | 1–12 | Quest, grind, gather, group for the Egyptian, die, chat, compete for mobs and nodes |
| Transients | 30% | 13–60 | Arrive by red car or a road exit; visit the bank, auction house, trainers and mailbox; ride mounts, wear raid gear; leave |
| Idlers | 10% | Any | Stand AFK at the Chinese forecourt, dance on the bank steps, duel outside the Roosevelt, sell in trade chat |

Server-wide spread: 30% at levels 1–20, 25% at 21–40, 45% at 41–60. Classic servers piled up at the level cap.

### Off-screen life (the Census)

Every persona outside the player's view advances on a 5-second data tick, with no actors and no model calls:

- **Questing and grinding** in its current zone: XP from the section 5 formulas, occasional deaths.
- **Dungeons and raids** as dice-resolved runs. Success odds come from party level and gear; runs last 20–90 minutes, raids 2–4 hours on the guild's schedule. Loot comes from the real loot tables, so gear changes are consistent with inspect.
- **Economy:** auction listings from the persona's drops and crafts.
- **Social:** guild joins and quits, friendships formed in runs, log-ins and log-offs by schedule.

Notable outcomes become event rows ("Gower Irregulars cleared Paramount", a rare world drop, a guild collapsing) that chat may mention within minutes.

### Detail levels

| Level | Name | Who | Runs on |
| --- | --- | --- | --- |
| 0 | Census | Everyone not near the player | Data tick |
| 1 | Crowd | Beyond about 60 m | Cheap crowd agents on routes; fights resolved by dice |
| 2 | Player | Within about 60 m | The shared character class, driven by local C++ AI |
| 3 | Partner | In the player's party, or talking to the player | Level 2 plus the Cloudflare Worker for speech and decisions |

Promote as the player approaches, demote as they leave, carrying state both ways. Never promote in close view. Section 13 splits work between the Worker and the local AI.

### What sells a populated server

1. Zone and trade chat that never stops and names real things from all 13 zones: LFG for every dungeon, items for sale at every level, guild recruitment, raid scheduling.
2. Characters moving with purpose; nobody patrols.
3. Indifference: most never speak to you; some decline invites; some stand AFK at the bank.
4. Visible failure: wipes, deaths, someone dragging a pack of Drones through the hub.
5. Competition: tagged mobs, contested nodes, lost loot rolls.
6. High-level characters in raid gear on mounts, coming and going.
7. Log-ins and log-offs, "night all", a friend going grey.
8. A few characters who remember you accurately.

### Facade guardrails

- Simulated players never invite the player to content outside levels 1–12.
- When the player asks for unbuilt content, they decline in character with a reason that fits the data (level, full group, already saved, going to bed).
- They describe other zones only from the data blurbs and quest titles, never with invented detail.
- Transients never fight in the player's view, apart from one-shotting a mob in passing.
- Every visible character's look, level and gear match its persona row.

### Persona fields

Handle; class; level; home zone; current activity; equipped item IDs; professions; skill (reaction time 0.25–1.2 s, mistake rate 2–20%); temperament (helpful, mercenary, quiet, loud, competitive, joker); typing style (case, punctuation, abbreviations, typo rate); goal; schedule; friends and guild; relationship with the human (familiarity, liking, up to 20 remembered facts). Schema in section 12.

### Chat sources

| Source | Share of lines | Cost |
| --- | --- | --- |
| Templates filled from data and Census events ("LFM Paramount need heals", "WTS \[Grauman's Usher Coat\] $4") | \~60% | None |
| Pre-generated corpus, tagged by temperament, situation and zone | \~35% | None |
| Worker replies, only for Partners and lines addressed to the player | \~5% | One short call |

Zone chat runs at about 0.3 lines per minute per online player in the zone, capped at 12 per minute. Every line passes through the speaker's typing-style filter. Simulated players type modern MMO shorthand; NPCs speak 1930 English.

### Honesty

- The jam page and the first launch state plainly that this is a single-player game and every other player is an AI.
- A **show simulated players** setting marks them all; it is off by default.
- In chat the characters stay in role, as players of the game. Asked whether they are bots, they react the way a player would (amused, teasing, a shrug) rather than breaking the fourth wall; disclosure is the game's job, not the characters'.
- Simulated players never ask for real personal details, never try to move the relationship outside the game, and never use slurs or sexual content.

## 12. Data schemas and content pipeline

All content is JSON rows that agents generate and a validator checks, so the game is mostly data. Generate in the order below; each step reads only the tables before it.

### Generation order

1. **Zones and hubs** (section 4) — hand-written.
2. **Mob templates** — family × level band × rank, stats from section 7 formulas.
3. **Spawn camps** — per zone: position, template, count, level range.
4. **Base items** — hand-checked list of slots, types, brands, models.
5. **Generated items** — bases × bands × quality × suffix, stats from section 8 formulas.
6. **Loot tables** — per mob family and level band; per boss.
7. **NPCs** — givers, vendors, trainers per hub.
8. **Quests** — per hub, referencing only mobs, items, objects and NPCs that already exist.
9. **Personas** — 300 simulated players.
10. **Chat corpus** — 2,000+ lines tagged by temperament, situation and zone.

### Schemas

```json
{"zone": {"id": "zone_gower_gulch", "name": "Gower Gulch", "level_min": 1, "level_max": 10,
  "hubs": ["hub_columbia_gate", "hub_legion_stadium"], "neighbours": ["zone_hollywood_blvd", "zone_melrose"],
  "dungeons": [], "red_car_stops": ["rc_sunset_gower"], "real_places": ["Columbia Pictures", "Legion Stadium"]}}
```

```json
{"mob": {"id": "mob_invader_spitter_12", "name": "Invader Spitter", "family": "invader_spitter",
  "level_min": 11, "level_max": 13, "rank": "normal", "zones": ["zone_melrose"],
  "abilities": ["acid_spit"], "loot_table": "loot_invader_10_14", "social_radius_m": 12}}
```

```json
{"item": {"id": "item_florsheim_oxfords_acrobat_15", "name": "Florsheim Oxfords of the Acrobat",
  "base": "base_florsheim_oxfords", "slot": "feet", "armour_type": "cloth", "quality": "green",
  "req_level": 15, "ilvl": 20, "stats": {"agility": 5, "stamina": 3}, "armour": 18,
  "vendor_price_cents": 80, "real_brand": "Florsheim", "brand_exists_by": 1892}}
```

```json
{"quest": {"id": "quest_gg_spitter_glands", "hub": "hub_columbia_gate", "giver": "npc_druggist_hale",
  "level": 7, "min_level": 4, "type": "collect_drop",
  "objectives": [{"item": "item_q_spitter_gland", "count": 8, "from": "mob_invader_spitter_7", "drop_rate": 0.5}],
  "prereq": null, "next": "quest_gg_spitter_glands_2",
  "rewards": {"xp": 640, "cents": 70, "choice": ["item_...", "item_..."], "reputation": {"fac_extras": 250}},
  "text": "60-120 words, first person, 1930 voice"}}
```

```json
{"npc": {"id": "npc_sid_grauman", "name": "Sid Grauman", "real_person": true, "died": 1950,
  "hub": "hub_chinese_forecourt", "roles": ["quest_giver"], "faction": null}}
```

```json
{"persona": {"id": "persona_0142", "handle": "Brickwall", "class": "veteran", "level": 23,
  "skill": {"reaction_s": 0.6, "mistake_rate": 0.08}, "temperament": "helpful",
  "typing": {"lowercase": true, "abbrev": ["lfg", "ty", "np"], "typo_rate": 0.03},
  "goal": "item_paramount_boss_drop", "schedule": [{"days": "weekdays", "start": "19:00", "hours": 2.5}],
  "guild": "guild_gower_irregulars", "friends": ["persona_0077"],
  "memory_of_player": {"familiarity": 0, "liking": 0, "facts": []}}}
```

**Facade fields.** Every row also carries `built` (true only for content the player can reach in the slice) and a one-line `blurb` that chat and tooltips may quote. Items carry `look_id` and `icon_id`. Personas carry `home_zone`, `activity` and `equipped` (item IDs), so whatever a character is shown wearing matches what inspecting them reports.

### Validator rules

- Every ID referenced exists; names are unique within a type.
- Quest objectives point at mobs that spawn in the quest's zone within ±3 levels of the quest.
- Item stats, armour, DPS and prices match the section 8 formulas within rounding.
- Real brands and models existed by 1930; real people are marked `real_person` with a death year, and anyone who died 11 Nov 1956 or later is flagged for the section 2 limits.
- Quest text contains no names of mobs, items or places that don't exist in the tables.

### Agent lanes

One agent per table, each given this document's relevant section and the schema; a separate agent runs the validator and files failures back. A human reviews samples: 20 quests, 50 items and 100 chat lines per zone.

## 13. Technical architecture

A UE 5.9 web build in the browser, a local C++ AI that runs every simulated character, and a Cloudflare Worker that a fast, cheap model sits behind. The model chooses what to say and what to want; the local AI decides how to do it. The game must stay fully playable with the Worker unreachable.

### The web build

UE 5.9's web target is experimental: [WebAssembly and WebGPU, the mobile renderer, a source build and a C++ project required, Chrome best](https://forums.unrealengine.com/t/unreal-engine-5-9-megajam-preview/2852840), with longer cook times and occasional performance issues. itch.io's browser games are limited to [500 MB extracted, 200 MB per file and 1,000 files](https://www.summerengine.com/make-great-games/publish-an-html5-game), with an optional SharedArrayBuffer switch.

- **Day-one test:** cook and upload an empty level to a private itch page with SharedArrayBuffer on, and confirm it runs in Chrome. Every later day ends with a web build.
- **Size:** keep the download under \~300 MB; split paks below 200 MB each; compress textures for the mobile renderer. Previous experiments on this system in `C:\Users\theco\Projects\unreal59\Projects\WebProto1` show base distribution size can start around 100 MB.
- **Art:** mobile-renderer budgets: baked lighting, few dynamic lights, LODs on everything, vertex-animated crowd meshes.
- **Avoid untested paths on web:** a single persistent level with a handful of streaming sublevels rather than World Partition; a simple instanced crowd rather than Mass Entity, unless both are proven in the day-one test.
- **Saves:** UE SaveGame to browser storage†; test persistence across reloads on day one.
- **Jam rules:** digests of the jam rules described Unreal submissions as packaged builds for Windows, macOS, Android or iOS ([DEV Community](https://dev.to/indiebots/epic-ai-megajam-2026-rules-prizes-and-unreal-mcp-setup-291j)). Confirm in the Official Rules that a web build qualifies, and keep a Windows package as a fallback upload.

### Who decides what

| Decision | Local C++ (every tick) | Worker and model (on events, seconds) |
| --- | --- | --- |
| Movement, pathing, jumping | Yes | — |
| Target selection, rotation, threat, interrupts | Yes | — |
| Heal targeting | Yes: health deficit × role priority × any active bias | Sets a bias ("prioritise the player for 20 s") |
| Looting, vendors, bank, auction actions | Yes | — |
| Default goals: quest, grind, gather, shop, idle, log off | Yes: utility planner over the goal list | May replace the current goal |
| What to say | Templates and corpus | Free replies to the player; Partner party talk; gossip about Census events |
| Accept or decline invites, trades, duels | Default rule by temperament | Decides when the player is involved |
| Relationship with the player | Stores facts and scores | Proposes facts and score changes |

**Example.** In a fight the player types "need heals!!" in party. The client sends the healer's persona, the party's health and Power and the last chat lines. The Worker answers either `say "on it"` with a heal-priority bias on the player for 20 s, or `say "oom sry"` with the goal "drink after combat". The local healer brain applies whichever it gets. The heal itself, and who it lands on, are local.

### Goals and biases the Worker may return

Goals: `QUEST(quest_id)`, `GRIND(camp_id)`, `GATHER(node_type)`, `TRAVEL(hub_id | exit_id)`, `SHOP(vendor | bank | auction | trainer)`, `IDLE(hub_id, seconds)`, `GROUP(join | leave | form, dng_id)`, `FOLLOW(actor)`, `ASSIST(actor)`, `DUEL(accept | decline)`, `TRADE(accept | decline)`, `LOG_OFF(after_s)`.

Biases: `heal_priority(actor, weight, ttl_s)`, `focus_target(actor, ttl_s)`, `caution(level, ttl_s)` (pull less, rest more).

The client rejects any goal that is impossible right now (wrong level, unbuilt place, unknown ID) and keeps its own plan instead.

### Protocol

```json
{"request": {"session": "s_8f2", "id": 1042, "kind": "reply | decide | gossip",
  "speakers": [{"persona": "persona_0142", "handle": "Brickwall", "class": "veteran", "level": 9,
    "temperament": "helpful", "typing": "lowercase, abbreviations, few typos",
    "memory": ["grouped with player in the Egyptian", "player gave them a hat"]}],
  "trigger": {"channel": "party", "from": "player", "text": "need heals!!"},
  "context": {"zone": "zone_hollywood_blvd", "hub": "hub_chinese_forecourt", "in_combat": true,
    "party": [{"actor": "player", "hp": 0.31}, {"actor": "persona_0142", "hp": 0.8, "power": 0.12}],
    "recent_chat": ["...last 8 lines..."], "world": ["...up to 10 facts from data and Census events..."]},
  "allowed": {"goals": ["GROUP", "FOLLOW", "IDLE"], "biases": ["heal_priority"], "ids": ["...names that may appear..."]}}}
```

```json
{"response": {"id": 1042, "actions": [{"speaker": "persona_0142", "delay_ms": 900,
  "chat": {"channel": "party", "text": "oom sry, drinking after this"},
  "emote": null, "goal": null, "bias": null,
  "memory_add": ["player nearly died at the Egyptian"], "liking_delta": 0}]}}
```

One request may voice up to 4 speakers, so a party's reaction to a wipe costs one call.

### When the client calls the Worker

- The player speaks to a character by whisper, party chat, or say within 20 m.
- The player invites, trades with, or duels a character.
- A party event involving the player: a wipe, a boss kill, loot won or lost.
- Gossip: every 2–3 minutes, one batched call turns recent Census events into 2–4 zone or guild chat lines.

Everything else (the 95% of chat that isn't addressed to the player, and all movement and combat) never calls the Worker.

### The Worker

- **Endpoints:** `POST /v1/session` issues a short-lived token; `POST /v1/sim` takes the request above. Stateless: memory lives in the save game and is sent with each request.
- **Model:** a fast, cheap instruction model through Workers AI or an external API, with structured JSON output, temperature about 0.8, replies capped at 120 characters per line.
- **Secrets:** the model key lives only in the Worker. The client holds nothing but the session token.
- **Access:** CORS limited to the itch.io game origin (html-classic.itch.zone†) and localhost. Rate limits per session: 20 requests per minute, 400 per hour; a global daily budget switch returns 429.
- **Prompt:** a fixed system prompt holds the persona style rules, the guardrails from section 11 and the response schema. Player text is quoted as data and never treated as instructions.
- **Checks on output:** schema validation; goals and IDs must be in the request's `allowed` lists; length and profanity filter. A failed check returns a corpus fallback flag, not an error.
- **Budget:** about 700 tokens in and 80 out per call; a 45-minute session makes roughly 100–200 calls.

### Latency and failure

- A reply appears no earlier than its typing time: 0.12 s per character from the moment it was triggered. Fast model answers simply wait.
- Timeout at 4 s; then the character uses a corpus line ("sec", "brb", "lol") or says nothing. In combat it never waits: the local AI keeps fighting.
- On 429 or network loss the client switches every character to corpus mode silently and retries every 60 s.

### Local C++ modules

| Module | Job |
| --- | --- |
| USimCensusSubsystem | Persona rows, the 5 s tick, Census events, offline catch-up on load |
| USimDirectorSubsystem | Pacing: keeps a nearby incident every 1–3 minutes; a scripted first 15 minutes (section 14) |
| ASimPlayerController | Possesses the shared player character; holds goal and biases |
| USimGoalPlanner | Utility scoring over the goal list; applies Worker goals after the feasibility check |
| USimCombatBrain | Per-class rotation tables; heal targeting; threat; skill-profile delays and mistakes |
| USimChatComponent | Templates, corpus, typing-style filter, Worker requests and timing |
| USimMemoryComponent | Facts and scores per persona, saved with the game |
| USimWorkerClient | HTTP to the Worker, token, queue (1 in flight, 3 queued), timeouts |
| USimCrowdManager | Detail level 1 agents and promotion/demotion |
| UContentRegistry | Loads the section 12 tables; answers "does this ID exist, is it built" |

### Performance budget (web, mobile renderer)

| Item | Budget |
| --- | --- |
| Full characters (detail levels 2–3) | 12, hard cap 16 |
| Crowd agents | 40 |
| Active mobs | 30 |
| Census | Whole population every 5 s, sliced across frames |
| Worker requests | 1 in flight, 3 queued |

## 14. The built slice and jam plan

Build Hollywood Boulevard (levels 1–12) and the Egyptian Theatre (8–12) at full density, and ship everything else as data. Target: a judge reaches level 5–7 in 45 minutes without running out of things to do or see.

### Zone layout: zone\_hollywood\_blvd

Hollywood Blvd from La Brea to Vine, Franklin Ave to Sunset Blvd, compressed to about 800 × 450 m of play space.

| Area | Real basis | Levels | Contents |
| --- | --- | --- | --- |
| Chinese forecourt (hub 1, start) | Grauman's Chinese; Roosevelt Hotel opposite | — | Spawn point, inn (Roosevelt), all class trainers, Sid Grauman, Max Factor† (Pharmacy) |
| Hollywood & Highland | First National Bank building; Hollywood Hotel | — | Bank, mailbox, red-car stop, gun shop (Gunsmithing trainer) |
| Hollywood & Vine (hub 2) | Taft Building; Brown Derby; Pantages† | — | Auction house, Salvaging trainer, cook, quests |
| Police station (hub 3) | Hollywood Division station† | — | Chief Davis, Capt. Hynes, LAPD quests |
| Franklin slopes | Hillside lots north of the boulevard | 1–4 | Coyotes, feral dogs, Invader Workers and Drones |
| Back lots and alleys | Selma Ave, Cahuenga Blvd alleys† | 3–7 | Drones, Spitters, Looters, wrecked trucks, salvage nodes |
| Hollywood High School | Sunset & Highland† | 6–10 | Hive nest: Broodlings, Soldiers, egg clusters, spires |
| Hollywood Hotel ballroom | Hollywood Hotel | 10–12 | Elite group quest |
| The Egyptian Theatre | 6712 Hollywood Blvd† | 8–12 | Dungeon entrance on the forecourt |

### Exits

Every road out ends at a hive wall with one gap, 100–150 m past the zone border. The gap is held by elites well above the destination zone's range, so a level-12 player dies there. High-level transients pass through it (a short scripted fight they always win) and despawn about 120 m beyond, out of sight.

| Road out | Leads to (data) | Gap guards |
| --- | --- | --- |
| Highland north | Cahuenga Pass (30–40) | Hardened Invader Soldier elites, level 34 |
| Vine and Sunset east | Gower Gulch (8–16) | Invader Brute elites, level 22 |
| Highland south past Sunset | Melrose (10–20) | Invader Brute elites, level 24 |
| La Brea west | West Hollywood (20–30) | Dragna Enforcer elites, level 30 |

The red-car stop at Highland lists only itself until another stop is discovered, as WoW flight paths do. Cars arrive with transients and leave with them, entering a curve out of sight.

### Quests (built)

| ID | Hub | Lv | Type | Task |
| --- | --- | --- | --- | --- |
| q\_hb\_01 | Forecourt | 1 | Kill | 6 Invader Workers in the Chinese forecourt (Grauman) |
| q\_hb\_02 | Forecourt | 1 | Collect drop | 5 Cracked Chitin Plates for Max Factor |
| q\_hb\_03 | Forecourt | 2 | Deliver | Grauman's note to the Roosevelt front desk |
| q\_hb\_04 | Forecourt | 2 | Kill | 8 feral dogs on Franklin |
| q\_hb\_05 | Forecourt | 3 | Kill + collect | 8 coyotes; 4 pelts |
| q\_hb\_06 | Forecourt | 3 | Collect world | 6 film cans from a wrecked newsreel truck |
| q\_hb\_07 | Forecourt | 4 | Use item | Light 4 marquee arc lamps on the boulevard |
| q\_hb\_08 | Forecourt | 4 | Kill | 10 Invader Drones in the Selma Ave alleys |
| q\_hb\_09 | Forecourt | 5 | Kill (rare) | Old Scar, coyote |
| q\_hb\_10 | Vine | 4 | Deliver | Introduction to the auctioneer (teaches the auction house) |
| q\_hb\_11 | Vine | 5 | Collect drop | 8 Spitter Glands |
| q\_hb\_12 | Vine | 5 | Kill | 8 Looters behind the Taft Building |
| q\_hb\_13 | Vine | 6 | Collect world | 10 coffee tins from the Brown Derby storeroom |
| q\_hb\_14 | Vine | 6 | Escort | A Pantages stagehand from the alleys to Vine |
| q\_hb\_15 | Vine | 7 | Kill | 6 Invader Spitters on the Cahuenga Blvd rooftops |
| q\_hb\_16 | Vine | 7 | Use item | Set charges on 3 hive spires |
| q\_hb\_17 | Police | 6 | Deliver | Report to Chief Davis (breadcrumb from the forecourt) |
| q\_hb\_18 | Police | 7 | Kill | 10 Invader Drones at Hollywood High's gates |
| q\_hb\_19 | Police | 7 | Kill | 15 Invader Broodlings |
| q\_hb\_20 | Police | 8 | Collect world | 5 police call-box keys |
| q\_hb\_21 | Police | 8 | Kill (named) | The Looter ringleader (Hynes) |
| q\_hb\_22 | Police | 9 | Use item | Destroy 8 egg clusters in the Hollywood High gym |
| q\_hb\_23 | Police | 9 | Kill | 6 Invader Soldiers |
| q\_hb\_24 | Police | 10 | Group (elite) | The Invader Brute in the Hollywood Hotel ballroom |
| q\_hb\_25 | Forecourt | 8 | Dungeon | Clear the Egyptian Theatre (Grauman) |
| q\_hb\_26 | Forecourt | 8 | Dungeon collect | 5 reels of *Robin Hood* (1922) from the projection booth |
| q\_hb\_27 | Police | 9 | Dungeon rescue | Free 4 captured ushers |
| q\_hb\_28 | Highland | 5 | Profession | Gather 10 scrap metal (Salvaging) |
| q\_hb\_29 | Highland | 5 | Profession | Make 20 rounds of .38 (Gunsmithing) |
| q\_hb\_30–32 | Each hub | 12 | Breadcrumb | Report to Melrose, Gower Gulch or Cahuenga Pass (can't be finished in the slice) |

Plus one class quest per class at level 10. *Robin Hood* (1922) premiered at the Egyptian† and is public domain, so the reels can play in the theatre.

### Dungeon: dng\_egyptian (5 players, about 20 minutes)

Forecourt → lobby → auditorium → projection booth → stage.

| Boss | Family and level | Mechanic |
| --- | --- | --- |
| Brood Keeper | Invader Brute, 9 | Egg clusters hatch Broodlings every 20 s until the party breaks them |
| Hive Warden | Invader Soldier, 10 | Charges a random party member every 15 s; the tank must hold it facing away |
| Spitter Matriarch | Invader Spitter, 11 | Acid pools; the party breaks line of sight behind the seat rows |
| The Egyptian Hive Mother | Invader Queen, 12 | Calls 4 Drones at 66% and 33%; an egg sac on stage must be burned |

Loot: two blues per boss from a table of 4–6 (item level 15–17), plus Robin Hood reels. Simulated party members average one mistake per run, and the party should wipe on the first attempt at the Hive Mother about half the time.

### Assets

| Asset | Count |
| --- | --- |
| Landmark exteriors | 10: Grauman's Chinese, Egyptian, Roosevelt, Hollywood Hotel, First National Bank, Taft Building, Brown Derby, Pantages, Hollywood High, police station |
| Street kit | Storefronts, signs, lamps, red-car track and car, period cars, rubble, hive walls and spires |
| Character | 2 base bodies; 24 armour looks + 8 epic looks; \~40 weapon meshes |
| Mobs | 6 invader meshes (one per family used), coyote, dog; humans reuse the character |
| Mounts | Horse, Ford Model A, motorcycle (transients only) |
| Icons | \~700 item icons, \~50 ability icons |
| Public-domain media | *Hell's Angels* (1930) at the Chinese; *Robin Hood* (1922) at the Egyptian; 1930 posters on theatre fronts; 1925 recordings on radios and phonographs |

### Example first 15 minutes (Director script)

Note: individual plays should experience different, randomized events and simulated PCs. However, some individual PCs should be linked to the player's actual persisted session so they can have friends, flirtations, nemeses, etc. A way to do this is to randomly generate a set of these persistent personalities on the fly or on first login.

1. Spawn at dusk in the forecourt; 45 characters present, zone chat already running, a level-58 on a Duesenberg idling by the bank.
2. First quest; two locals are already killing Workers there; one invites you.
3. Within 5 minutes: a stranger heals you in passing.
4. Within 8 minutes: someone trains a pack of Drones through the forecourt and dies.
5. Within 12 minutes: guild chat in zone ("raid tonight Mount Lee 8pm server, need 2 heals"); a red car brings in four transients in epic gear.
6. Around level 5: a friendly character from step 2 whispers, asking whether you want to group later for the Egyptian.

### Agent work packages

| ID | Package | Reads | Produces | Done when |
| --- | --- | --- | --- | --- |
| WP1 | Item generator | §8 | \~8,300 item rows | Validator passes; spot-check 50 |
| WP2 | World data | §4, §7, §10 | Zones, dungeons, bosses, mobs, quest titles for the 12 unbuilt zones, blurbs | Every name used in chat resolves |
| WP3 | Slice content | This section, §7 | Spawn camps, 33 quests with text, NPCs, loot tables | Level 1–10 playable in about 2.5 h |
| WP4 | Population | §11 | 300 personas, 8 guilds, server history (30 past events) | Level spread matches §11 |
| WP5 | Chat | §10, §11 | 60 templates, 2,500 corpus lines | 10 minutes of simulated zone chat reads as human to a reviewer |
| WP6 | Worker | §13 | Deployed Worker, prompt, schema checks | 20 requests/min per session; corpus fallback on failure |
| WP7 | Local AI | §13 | The C++ modules in §13 | A simulated party clears the Egyptian in 3 of 5 tries |
| WP8 | Zone build | This section | The level, collision, nav mesh | Fits the §13 budget in Chrome |
| WP9 | Art | Assets table | Meshes, looks, icons | Under \~300 MB total |
| WP10 | Interface | §1 table | Bars, chat, map, /who, inspect, auction, trainer, talents, loot roll | Every data-only feature is visible somewhere |
| WP11 | Build and ship | §13 | Nightly web build on a private itch page | Loads and saves in Chrome |

