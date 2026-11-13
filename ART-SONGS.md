# ART-SONGS.md

30 instrumental tracks generated with Lyria 3 Pro (`fal-ai/lyria3/pro`) via fal.ai, stored in `music/`. All target ~2:30, late-1920s / 1930 acoustic period style, no vocals.

| # | Title | File | Intended scene use | Style | BPM |
|---|---|---|---|---|---|
| 1 | Hollywoodland Overture | `music/01-hollywoodland-overture.mp3` | Main menu | Grand cinematic orchestra | 108 |
| 2 | Hollywood Boulevard, 1930 | `music/02-hollywood-boulevard-1930.mp3` | Starting zone | Upbeat hot jazz | 152 |
| 3 | Grauman's Chinese Theatre | `music/03-grauman-s-chinese-theatre.mp3` | Player spawn / social hub | Theatrical jazz orchestra | 116 |
| 4 | The Roosevelt Hotel | `music/04-the-roosevelt-hotel.mp3` | Inn / rest area | Elegant hotel jazz | 92 |
| 5 | Hollywood & Vine | `music/05-hollywood-vine.mp3` | Commercial hub / auction house | Brisk dance band | 164 |
| 6 | Brown Derby After Dark | `music/06-brown-derby-after-dark.mp3` | Restaurant / night exploration | Sultry jazz | 78 |
| 7 | The Red Car Express | `music/07-the-red-car-express.mp3` | Pacific Electric travel | Playful jazz | 136 |
| 8 | Picture Night at the Chinese | `music/08-picture-night-at-the-chinese.mp3` | Evening world event | Romantic cinema orchestra | 84 |
| 9 | Shadows Beneath the Marquee | `music/09-shadows-beneath-the-marquee.mp3` | Mystery exploration | Noir jazz thriller | 88 |
| 10 | The Dome Above Hollywood | `music/10-the-dome-above-hollywood.mp3` | Alien occupation / discovery | Ominous orchestral | 64 |
| 11 | The Selma Avenue Alleys | `music/11-the-selma-avenue-alleys.mp3` | Low-level questing | Sneaky jazz | 112 |
| 12 | The Hive Takes Root | `music/12-the-hive-takes-root.mp3` | Alien nest / Hollywood High | Horror orchestra | 72 |
| 13 | Cahuenga Rooftops | `music/13-cahuenga-rooftops.mp3` | Rooftop quests | Suspenseful jazz | 132 |
| 14 | The Hollywood Hotel Ballroom | `music/14-the-hollywood-hotel-ballroom.mp3` | Elite group quest | Haunted waltz (3/4) | 144 |
| 15 | The Projectionist's Secret | `music/15-the-projectionist-s-secret.mp3` | Investigation / film reels | Chamber thriller | 66 |
| 16 | Trouble on the Boulevard | `music/16-trouble-on-the-boulevard.mp3` | Standard combat | Fast orchestral jazz | 176 |
| 17 | Attack of the Invader Drones | `music/17-attack-of-the-invader-drones.mp3` | Alien combat | Ominous orchestral action | 168 |
| 18 | The Bootlegger's Run | `music/18-the-bootlegger-s-run.mp3` | Gangster encounters | Hot jazz chase | 190 |
| 19 | The Veteran's Last Stand | `music/19-the-veteran-s-last-stand.mp3` | Defensive combat | Heroic orchestral march | 124 |
| 20 | Lights, Camera, Dynamite! | `music/20-lights-camera-dynamite.mp3` | Demolition / action quests | Comedic action jazz | 174 |
| 21 | The Hive Surge | `music/21-the-hive-surge.mp3` | World event / hub defense | Large-scale orchestral combat | 158 |
| 22 | The Looter Ringleader | `music/22-the-looter-ringleader.mp3` | Named human enemy | Menacing gangster jazz | 142 |
| 23 | Enter the Egyptian | `music/23-enter-the-egyptian.mp3` | Dungeon entrance / lobby | Gothic movie orchestra | 76 |
| 24 | The Brood Keeper | `music/24-the-brood-keeper.mp3` | Dungeon boss 1 | Grotesque orchestral jazz | 164 |
| 25 | The Hive Warden | `music/25-the-hive-warden.mp3` | Dungeon boss 2 | Militaristic orchestral thriller | 152 |
| 26 | The Spitter Matriarch | `music/26-the-spitter-matriarch.mp3` | Dungeon boss 3 | Psychological horror orchestra | 138 |
| 27 | The Egyptian Hive Mother | `music/27-the-egyptian-hive-mother.mp3` | Final dungeon boss | Epic orchestral horror | 172 |
| 28 | The Last Reel | `music/28-the-last-reel.mp3` | Projection booth / dungeon mystery | Haunting chamber score | 58 |
| 29 | Victory at the Egyptian | `music/29-victory-at-the-egyptian.mp3` | Dungeon completion | Triumphant Hollywood orchestra | 118 |
| 30 | When Hollywood Falls Silent | `music/30-when-hollywood-falls-silent.mp3` | Night exploration / reflection | Melancholic jazz orchestra | 68 |

Sections: 1-8 city & glamour; 9-15 mystery & occupied Hollywood; 16-22 combat & pursuit; 23-29 Egyptian Theatre dungeon (entrance, four bosses, projection booth, victory); 30 night reflection.

Note: Lyria output length and tempo are guided by the prompt, not guaranteed; check each track before wiring it in. Tracks carry SynthID watermarks.

## In-game wiring (`hollywoodland-ue/Source/Hollywoodland/HLMusic.cpp`)

Priority: title (1, loop) > dungeon clear (29, once) > player dead (30, loop) > notable fight (loop, held 6 s after the last qualifying frame) > ambient.
- Fights: boss 24-27 by script; Knuckles Malone 22; Dragna Enforcer 18; Ballroom Brute 14; dungeon elite or 2+ mobs 19; 5+ insects 21; insect elite or 3+ insects 17; other elite/rare or 3+ mobs 16 (20 in Taft Back Lots). Single ordinary mobs keep the ambient track.
- Ambient (play once, 20-45 s silence, next pick; day / night 20:00-06:00 local): Chinese Forecourt 3,4,2 / 8,3,4; Hollywood & Highland 2,4 / 4,8,30; Hollywood & Vine 5,2 / 6,5; Police Station 9; Franklin Slopes 2,11,10 / 30,9,10; Selma Alleys 11,9; Cahuenga Rooftops 13; Taft Back Lots 15,20; Hollywood High 12; Hotel Ballroom 14; Egyptian (outside) 23; Sunset 2,7 / 6,30; Hive Wall 10,12; near a red car 7; elsewhere 2,11,9,10 / 30,6,9,10. Dungeon: Forecourt, Lobby 23; Auditorium, Stage 23,28; Projection Booth 28.
- Unused for now: none. Web streams `music/<file>` from the client Worker (R2 `music/`, `workers/deploy-music.mjs`); native plays `<ProjectDir>/Music` or `<repo>/music`.
