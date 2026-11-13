// Sound effects: generate every SFX with fal.ai ElevenLabs Sound Effects v3, master them with ffmpeg and write the index.
//   node tools/gen-sfx.mjs [--only <regex>] [--redo] [--dry] [--jobs N]
// Reads FAL_AI_API_KEY from <repo>/.env (never printed). Masters (16-bit WAV straight from the API) go to <repo>/sfx-src/
// (gitignored, kept so re-mastering needs no new calls); shipped files are <repo>/sfx/<id>_<n>.mp3 and the runtime index is
// hollywoodland-ue/Content/Data/sfx.json. Existing masters are reused unless --redo (with --only, the matching ids only).
//
// Catalogue row: [id, category, seconds, variants, prompt, opts]
//   opts.loop     seamless loop (ElevenLabs loop mode; kept untrimmed, stereo for ambience)
//   opts.vol      runtime gain 0..1 (every file is peak-normalised, so loudness lives here)
//   opts.range    metres at which a positional sound fades out (0 = 2D: UI, the player's own sounds, ambience)
//   opts.pitch    random pitch spread per play (0.06 = +-6%)
//   opts.inf      prompt_influence (default 0.45)
//   opts.stereo   keep stereo (loops default to stereo when range is 0)
import { readFileSync, writeFileSync, existsSync, mkdirSync, readdirSync, unlinkSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFile } from 'node:child_process';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SRC = join(ROOT, 'sfx-src'), OUT = join(ROOT, 'sfx'), INDEX = join(ROOT, 'hollywoodland-ue/Content/Data/sfx.json');
const args = process.argv.slice(2);
const opt = (k) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : undefined; };
const ONLY = opt('--only') ? new RegExp(opt('--only')) : null, REDO = args.includes('--redo'), DRY = args.includes('--dry');
const JOBS = Number(opt('--jobs') || 4);

// ---------------------------------------------------------------------------------------------------------------- catalogue
const ERA = '1920s';
const C = [];
const add = (id, cat, dur, n, prompt, o = {}) => C.push([id, cat, dur, n, prompt, o]);

// UI (2D)
const ui = { range: 0, pitch: 0.03 };
add('ui_click', 'ui', 0.5, 3, 'Soft click of an old brass typewriter key, single short mechanical tick, dry', { ...ui, vol: 0.35 });
add('ui_open', 'ui', 0.6, 2, 'Leather-bound ledger book opening, short paper flutter and soft cover thump', { ...ui, vol: 0.45 });
add('ui_close', 'ui', 0.6, 2, 'Leather-bound book closing with a soft thump', { ...ui, vol: 0.45 });
add('ui_page', 'ui', 0.6, 3, 'Single page of old paper turned in a book, crisp rustle', { ...ui, vol: 0.45 });
add('ui_bag', 'ui', 0.8, 2, 'Canvas duffel bag opened, buckle jingle and fabric rustle', { ...ui, vol: 0.5 });
add('ui_error', 'ui', 0.5, 1, 'Short dull wooden knock, a muted negative thunk, interface error', { ...ui, vol: 0.55 });
add('ui_coins', 'ui', 0.9, 3, 'A few silver coins dropped into a hand and jingling', { ...ui, vol: 0.6 });
add('ui_cash', 'ui', 1.2, 1, 'Antique brass cash register ka-ching with bell and drawer sliding open', { ...ui, vol: 0.6 });
add('ui_loot', 'ui', 0.7, 3, 'Picking up a small item, quick cloth rustle and light clink', { ...ui, vol: 0.55 });
add('ui_equip_cloth', 'ui', 0.8, 2, 'Putting on a wool coat, fabric rustle and shrug', { ...ui, vol: 0.55 });
add('ui_equip_metal', 'ui', 0.8, 2, 'Buckling on metal armour plates, clank and leather strap tightened', { ...ui, vol: 0.55 });
add('ui_equip_weapon', 'ui', 0.8, 2, 'Weapon drawn and readied, metal slide and grip', { ...ui, vol: 0.55 });
add('ui_quest_accept', 'ui', 1.5, 1, 'Rubber stamp thumped onto paper followed by a short bright two-note muted trumpet flourish', { ...ui, vol: 0.6, inf: 0.5 });
add('ui_quest_done', 'ui', 2.5, 1, 'Triumphant short brass band fanfare sting, 1920s jazz orchestra, bright and celebratory', { ...ui, vol: 0.65, inf: 0.5 });
add('ui_levelup', 'ui', 3.5, 1, 'Rising magical shimmer with a swelling 1920s Hollywood orchestra chord and harp glissando, triumphant level up', { ...ui, vol: 0.75, inf: 0.5 });
add('ui_learn', 'ui', 1.5, 1, 'Pencil scribbling quickly on paper then a soft bright chime', { ...ui, vol: 0.55 });
add('ui_whisper', 'ui', 0.8, 1, 'Soft airy whoosh with a tiny bell tinkle, a private message notification', { ...ui, vol: 0.45 });
add('ui_mail', 'ui', 1.0, 1, 'Envelope slid out of a brass mail slot, paper rustle', { ...ui, vol: 0.55 });
add('ui_sold', 'ui', 1.2, 1, 'Auctioneer gavel banged once on a wooden block, then a coin jingle', { ...ui, vol: 0.6 });
add('ui_invite', 'ui', 1.0, 1, 'Old bicycle bell ringing twice, friendly', { ...ui, vol: 0.5 });
add('ui_trade', 'ui', 1.0, 1, 'Handshake slap and coins sliding across a wooden counter', { ...ui, vol: 0.55 });
add('ui_talents', 'ui', 1.5, 1, 'Mechanical gears clicking into place, then a soft resonant chime', { ...ui, vol: 0.55 });
add('ui_rested', 'ui', 1.5, 1, 'Gentle yawn-like soft harp arpeggio, calm and restful', { ...ui, vol: 0.45 });
add('ui_dice', 'ui', 1.0, 1, 'Two dice rolled on a wooden table', { ...ui, vol: 0.55 });
add('ui_photo', 'ui', 1.0, 1, 'Vintage magnesium flash camera, shutter click and flash pop', { ...ui, vol: 0.55 });

// player (2D for the player; positional for others)
const pl = { range: 30, pitch: 0.05 };
add('pl_hurt_m', 'player', 0.6, 4, 'Adult man grunting in pain from a punch, short non-verbal', { ...pl, vol: 0.6 });
add('pl_hurt_f', 'player', 0.6, 4, 'Adult woman grunting in pain from a hit, short non-verbal', { ...pl, vol: 0.6 });
add('pl_death_m', 'player', 1.5, 2, 'Man groaning and collapsing to the ground, dying, non-verbal', { ...pl, vol: 0.75 });
add('pl_death_f', 'player', 1.5, 2, 'Woman gasping and collapsing to the ground, dying, non-verbal', { ...pl, vol: 0.75 });
add('pl_jump_m', 'player', 0.5, 2, 'Man short effort exhale while jumping, hup', { ...pl, vol: 0.4 });
add('pl_jump_f', 'player', 0.5, 2, 'Woman short effort exhale while jumping, hup', { ...pl, vol: 0.4 });
add('pl_land', 'player', 0.5, 3, 'Leather shoes landing on pavement after a small jump, thud', { ...pl, vol: 0.45 });
add('pl_rez', 'player', 3.0, 1, 'Deep inhale gasp of someone coming back to life, with ethereal shimmering choir swell', { range: 0, vol: 0.7 });
add('pl_spirit', 'player', 3.0, 1, 'Eerie ghostly wind whoosh and distant soft choir, entering the afterlife', { range: 0, vol: 0.6 });
add('pl_eat', 'player', 1.5, 2, 'Person biting and chewing a sandwich', { ...pl, range: 15, vol: 0.5 });
add('pl_drink', 'player', 1.5, 2, 'Person gulping a drink from a glass bottle, swallow and ahh', { ...pl, range: 15, vol: 0.5 });
add('pl_liquor', 'player', 1.5, 1, 'Cork pulled from a whiskey flask, a swig and a rough throat clear', { ...pl, range: 15, vol: 0.5 });
add('pl_bandage', 'player', 1.5, 2, 'Cloth bandage unrolled and torn, wrapped tightly around an arm', { ...pl, range: 15, vol: 0.5 });
const steps = { range: 18, pitch: 0.08, vol: 0.28 };
add('step_street', 'player', 0.5, 4, 'Single footstep of a leather dress shoe on a concrete sidewalk', steps);
add('step_dirt', 'player', 0.5, 4, 'Single footstep of a boot on dry dirt and gravel', steps);
add('step_wood', 'player', 0.5, 4, 'Single footstep of a shoe on old hollow wooden floorboards', steps);
add('step_carpet', 'player', 0.5, 4, 'Single soft muffled footstep on thick carpet', { ...steps, vol: 0.22 });
add('step_stone', 'player', 0.5, 4, 'Single footstep on smooth stone tiles in a large hall, slight echo', steps);
add('step_hive', 'player', 0.5, 4, 'Single footstep squelching into wet sticky organic resin', steps);

// melee and weapons
const wpn = { range: 35, pitch: 0.07 };
add('swing_light', 'combat', 0.5, 4, 'Fast whoosh of a knife slashing through air', { ...wpn, vol: 0.45 });
add('swing_heavy', 'combat', 0.6, 4, 'Heavy whoosh of a wrench or club swung hard through air', { ...wpn, vol: 0.5 });
add('hit_blunt', 'combat', 0.5, 4, 'Heavy wooden club thudding into a leather sandbag, deep meaty thump', { ...wpn, vol: 0.65 });
add('hit_blade', 'combat', 0.5, 4, 'Knife slash cutting cloth and flesh, sharp slice impact', { ...wpn, vol: 0.6 });
add('hit_fist', 'combat', 0.5, 4, 'Bare-knuckle punch to the face, classic movie punch smack', { ...wpn, vol: 0.65 });
add('hit_bug', 'combat', 0.5, 4, 'Hard impact cracking an insect chitin shell, crunchy wet crack', { ...wpn, vol: 0.65 });
add('hit_bullet', 'combat', 0.5, 4, 'Bullet impact into a body, sharp thwack', { ...wpn, vol: 0.55 });
add('hit_crit', 'combat', 0.7, 3, 'Brutal heavy critical hit impact with bone crack and bass punch', { ...wpn, vol: 0.75 });
add('miss', 'combat', 0.5, 3, 'Swing whiffing through empty air, quick whoosh', { ...wpn, vol: 0.4 });
add('dodge', 'combat', 0.5, 3, 'Quick body sidestep, cloth rustle and shoe scuff', { ...wpn, vol: 0.45 });
add('parry', 'combat', 0.5, 3, 'Metal weapon parrying another, bright clang and scrape', { ...wpn, vol: 0.6 });
add('block', 'combat', 0.5, 3, 'Blow blocked by a heavy wooden shield, solid thunk', { ...wpn, vol: 0.6 });
const gun = { range: 70, pitch: 0.05 };
add('shot_revolver', 'combat', 1.0, 4, `Single ${ERA} revolver gunshot outdoors with short echo`, { ...gun, vol: 0.8 });
add('shot_pistol', 'combat', 0.8, 4, 'Single semi-automatic pistol gunshot, sharp crack', { ...gun, vol: 0.75 });
add('shot_rifle', 'combat', 1.2, 4, 'Single bolt-action rifle gunshot outdoors, loud crack and rolling echo', { ...gun, vol: 0.85 });
add('shot_shotgun', 'combat', 1.2, 3, 'Single pump shotgun blast, heavy boom', { ...gun, vol: 0.9 });
add('shot_smg', 'combat', 1.0, 3, `${ERA} Thompson submachine gun short burst of four shots`, { ...gun, vol: 0.85 });
add('gun_dry', 'combat', 0.5, 1, 'Empty revolver hammer clicking on an empty chamber', { ...wpn, range: 15, vol: 0.5 });
add('gun_reload', 'combat', 1.2, 2, 'Revolver cylinder opened, bullets loaded, cylinder snapped shut', { ...wpn, range: 15, vol: 0.5 });

// spell schools: casting loops, impacts, damage-over-time ticks
const cast = { loop: true, range: 30, vol: 0.4 };
add('cast_electric', 'cast', 4, 1, 'Electrical humming buzz building with crackling sparks, charging up, steady', cast);
add('cast_fire', 'cast', 4, 1, 'Fuse hissing and sparking steadily, gunpowder sizzling', cast);
add('cast_acid', 'cast', 4, 1, 'Glass vial bubbling with fizzing corrosive chemical, steady', cast);
add('cast_holy', 'cast', 4, 1, 'Soft sustained ethereal choir hum with gentle shimmering bells, steady', cast);
add('cast_heal', 'cast', 4, 1, 'Gentle warm shimmering hum with soft wind chimes, steady healing magic', cast);
add('cast_aim', 'cast', 4, 1, 'Slow tense breathing and a gun hammer being cocked, steady quiet aiming', { ...cast, vol: 0.3 });
add('cast_bug', 'cast', 4, 1, 'Giant insect gurgling and clicking mandibles, building up acid in its throat', cast);
const tick = { range: 30, pitch: 0.08, vol: 0.45 };
add('tick_fire', 'school', 0.8, 3, 'Short burst of flame flaring, whoosh and crackle', tick);
add('tick_electric', 'school', 0.6, 3, 'Short sharp electric zap and crackle', tick);
add('tick_acid', 'school', 0.8, 3, 'Short acid sizzle hissing on skin', tick);
add('tick_holy', 'school', 0.8, 3, 'Short searing holy burn with a faint choir note', tick);
add('tick_heal', 'school', 0.8, 3, 'Soft brief shimmering chime, gentle healing pulse', { ...tick, vol: 0.35 });
add('buff_on', 'school', 1.2, 2, 'Bright rising whoosh with a confident brass hit, power up', { range: 30, vol: 0.5 });
add('debuff_on', 'school', 1.0, 2, 'Low descending dissonant whoosh, weakening curse', { range: 30, vol: 0.5 });
add('shield_on', 'school', 1.2, 2, 'Protective energy bubble forming, resonant hum and glassy shimmer', { range: 30, vol: 0.5 });
add('shield_break', 'school', 1.0, 2, 'Energy barrier shattering like glass', { range: 35, vol: 0.6 });
add('stun_on', 'school', 1.2, 2, 'Heavy thump followed by a dazed high ringing tinnitus whine in the ears', { range: 30, vol: 0.5 });
add('slow_on', 'school', 1.0, 2, 'Sticky gluey squelch with a descending slowing whoosh', { range: 30, vol: 0.5 });
add('taunt_on', 'school', 1.0, 1, 'Fist pounding a chest twice, challenging', { range: 30, vol: 0.55 });

// abilities (one per ability; the cast loop for its school plays while it casts)
const ab = { range: 40, pitch: 0.04, vol: 0.65 };
const A = (id, n, prompt, o = {}) => add(id, 'ability', o.dur || 1.2, n, prompt, { ...ab, ...o });
A('ab_vet_thrust', 2, 'Bayonet thrust stabbing hard, metal scrape and impact');
A('ab_vet_shout', 1, 'Army sergeant battle bark, a loud gruff wordless shout with an echo', { dur: 1.5 });
A('ab_vet_hold', 1, 'Rifle stock slammed on the ground and a defiant growl, holding the line');
A('ab_vet_sweep', 2, 'Wide heavy rifle swing whoosh followed by several quick thudding impacts on sandbags');
A('ab_vet_butt', 2, 'Rifle butt smashing into a skull, heavy wooden crack');
A('ab_vet_dig_in', 1, 'Boots digging into dirt and sandbags dropped, bracing', { dur: 1.5 });
A('ab_vet_over_top', 1, 'Trench whistle blast then a charging heavy blow impact', { dur: 2 });
A('ab_stn_haymaker', 2, 'Huge wild haymaker punch, big wind-up whoosh and meaty smack');
A('ab_stn_tumble', 1, 'Acrobatic forward roll on the ground, body tumble and cloth rustle');
A('ab_stn_low_blow', 2, 'Low punch to the gut, dull thud and wheezing exhale');
A('ab_stn_flurry', 2, 'Three very fast knife slashes in a row, swish swish swish');
A('ab_stn_bottle', 2, 'Glass bottle thrown and smashing on a head, glass shatter', { dur: 1.5 });
A('ab_stn_leap', 1, 'Big jump whoosh and a heavy body-slam landing that shakes the ground', { dur: 1.5 });
A('ab_stn_showstopper', 1, 'Spectacular stunt finishing blow, huge punch impact followed by an audience gasp', { dur: 2 });
A('ab_gun_aimed', 2, 'Precise single rifle shot with a sharp ring and long echo', { range: 70 });
A('ab_gun_hip', 2, 'Quick revolver shot from the hip, snappy crack', { range: 70 });
A('ab_gun_knee', 2, 'Revolver shot and a ricochet whine off the ground', { range: 70 });
A('ab_gun_fan', 2, 'Revolver fanned rapidly, six shots in a fast rattle', { range: 70, dur: 1.5 });
A('ab_gun_buckshot', 2, 'Double barrel shotgun blast with pellets spraying and hitting', { range: 70 });
A('ab_gun_nerves', 1, 'Slow calm exhale and a gun hammer cocked, steady focus');
A('ab_gun_rights', 1, 'Massive single high-calibre gunshot, booming crack with long echo', { range: 80, dur: 1.5 });
A('ab_elc_arc', 3, 'Electric arc bolt zapping across a gap, sharp crackle and snap');
A('ab_elc_insulate', 1, 'Electrical insulation field humming up, rubber stretch and buzzing shield', { dur: 1.5 });
A('ab_elc_live_wire', 2, 'Live wire whipping and sparking, electric crackle');
A('ab_elc_floodlight', 1, 'Huge carbon arc floodlight switching on with a deep clunk and intense buzzing burn', { dur: 1.8 });
A('ab_elc_short', 2, 'Short circuit, loud electrical pop and sparks fizzing out');
A('ab_elc_overload', 1, 'Heavy electrical overload discharge, transformer hum surging then a massive zap explosion', { dur: 1.8 });
A('ab_elc_chain', 2, 'Lightning bolt crackling and jumping between targets, multiple rapid zaps', { dur: 1.5 });
A('ab_pow_flash', 2, 'Magnesium flash powder igniting, bright whoomph and fizz');
A('ab_pow_fuse', 2, 'Match strike and a fuse lit, sizzling');
A('ab_pow_smoke', 2, 'Smoke pot popping open and smoke hissing out thickly', { dur: 1.5 });
A('ab_pow_squib', 2, 'Small explosive squib charge popping, sharp bang');
A('ab_pow_keg', 1, 'Gunpowder keg exploding, big boom with wooden debris', { dur: 2, range: 80 });
A('ab_pow_screen', 1, 'Smoke grenade hissing out a thick smokescreen', { dur: 1.5 });
A('ab_pow_demo', 1, 'Massive dynamite demolition explosion, deep rumbling blast and falling debris', { dur: 3, range: 100, vol: 0.85 });
A('ab_nur_ether', 2, 'Ether rag pressed, chemical hiss and splash');
A('ab_nur_dress', 1, 'Wound dressed, cloth wrapping and soft healing shimmer', { dur: 1.5 });
A('ab_nur_tonic', 1, 'Medicine bottle uncorked, liquid poured and gentle healing chime');
A('ab_nur_salts', 1, 'Smelling salts snapped open and a sharp sniff, then a bright chime');
A('ab_nur_iodine', 2, 'Liquid splashed and sizzling, stinging chemical');
A('ab_nur_surgery', 1, 'Surgical instruments clinking on a tray, then a warm swelling healing chime', { dur: 2 });
A('ab_nur_sedative', 2, 'Syringe injection pneumatic hiss and a drowsy descending tone');
A('ab_pre_rebuke', 2, 'Divine holy strike, thunderous choir hit with a bright crack');
A('ab_pre_hands', 1, 'Warm glowing hands, soft choir aah and gentle shimmer', { dur: 1.5 });
A('ab_pre_blessing', 1, 'Church bell single chime with a protective shimmering hum', { dur: 1.5 });
A('ab_pre_sermon', 2, 'Hellfire whoosh with a fiery roar and ominous low choir');
A('ab_pre_hymn', 1, 'Short uplifting gospel choir chord swelling, healing hymn', { dur: 2.5 });
A('ab_pre_fear', 1, 'Thunderclap and an ominous booming organ chord, fear of god', { dur: 1.8 });
A('ab_pre_revival', 1, 'Tent revival tambourine shake and a triumphant choir hallelujah chord', { dur: 2.5 });
A('ab_mec_swing', 2, 'Heavy wrench swing and metallic clank impact');
A('ab_mec_patch', 1, 'Ratchet wrench cranking and a hiss of steam, repair');
A('ab_mec_caltrops', 1, 'Handful of metal caltrops scattered on the ground, clattering', { dur: 1.5 });
A('ab_mec_monkey', 2, 'Monkey wrench bonking a head with a metallic clang');
A('ab_mec_welder', 2, 'Arc welder torch sparking and buzzing harshly', { dur: 1.5 });
A('ab_mec_overhaul', 1, 'Engine revving, gears and bolts tightening rapidly, mechanical overhaul', { dur: 2 });
A('ab_mec_pile', 1, 'Steam pile driver slamming down, massive metal clang and ground shake', { dur: 1.5 });
A('ab_mob_acid_spit', 2, 'Giant insect spitting a glob of acid, wet hork and sizzle');
A('ab_mob_knockback', 2, 'Huge monster claw swipe whoosh and a heavy thump, then something tumbling across the ground');
A('ab_mob_charge', 1, 'Huge insect charging, rapid clattering legs and a crashing impact', { dur: 1.8 });
A('ab_mob_acid_pool', 1, 'Acid splashing onto the ground and pooling, bubbling sizzle', { dur: 1.8 });
A('ab_mob_pistol', 2, `${ERA} pistol gunshot`, { range: 70 });

// creatures: aggro, attack, hurt, death (+ move loops for the big ones). Insects are alien, never mechanical.
const mob = { range: 45, pitch: 0.08 };
const M = (fam, desc, o = {}) =>
{
  add(`${fam}_aggro`, 'mob', o.aggroDur || 1.2, 2, `${desc}, alerted and aggressive, short`, { ...mob, vol: 0.65, ...o.m });
  add(`${fam}_attack`, 'mob', 0.8, 3, `${desc}, short vicious attack sound`, { ...mob, vol: 0.5, ...o.m });
  add(`${fam}_hurt`, 'mob', 0.6, 3, `${desc}, short pained cry when hit`, { ...mob, vol: 0.5, ...o.m });
  add(`${fam}_death`, 'mob', o.deathDur || 1.5, 2, `${desc}, death cry and collapse`, { ...mob, vol: 0.7, ...o.m });
};
M('invader_soldier', 'Large alien insect warrior with clicking mandibles, chittering hiss');
M('invader_brute', 'Huge heavy alien beetle monster, deep guttural roaring bellow and chitin creak', { deathDur: 2, m: { range: 60 } });
M('invader_drone', 'Flying alien wasp insect, high buzzing wings and angry whine');
M('invader_spitter', 'Alien insect with a gurgling acid sac throat, wet hissing gargle');
M('invader_broodling', 'Small alien insect larva, high squeaky chittering');
M('invader_worker', 'Alien worker ant insect, dry clicking and scratching chitters');
M('invader_burrower', 'Giant burrowing alien centipede, raspy hiss with dirt crumbling', { deathDur: 2 });
M('invader_flier', 'Giant alien dragonfly insect, loud droning wings and piercing shriek');
M('invader_queen', 'Colossal alien insect hive queen, enormous reverberating shriek and deep roar', { aggroDur: 3, deathDur: 4, m: { range: 120, vol: 0.85 } });
M('coyote', 'Wild coyote, yipping and snarling');
M('feral_dog', 'Feral stray dog, vicious growling and barking');
M('looter', 'Rough street thug man, non-verbal angry grunts and yells');
M('enforcer', 'Tough 1920s mobster man, non-verbal gruff grunts and shouts');
add('bug_skitter', 'mob', 4, 1, 'Many insect legs skittering and clicking on concrete, steady', { loop: true, range: 25, vol: 0.35 });
add('bug_wings', 'mob', 4, 1, 'Large insect wings buzzing in flight, steady drone', { loop: true, range: 35, vol: 0.4 });
add('burrow_emerge', 'mob', 2.5, 1, 'Giant creature bursting up from underground, earth erupting and rocks scattering', { range: 70, vol: 0.85 });
add('burrow_dig', 'mob', 4, 1, 'Something large tunnelling underground, rumbling and grinding dirt, steady', { loop: true, range: 40, vol: 0.5 });
add('queen_screech', 'mob', 3.5, 2, 'Colossal alien hive queen summoning screech, ear piercing, shaking the walls', { range: 150, vol: 0.9 });
add('brood_spawn', 'mob', 1.5, 2, 'Slimy egg membrane tearing and small insects squirming out', { range: 40, vol: 0.6 });
add('flier_swoop', 'mob', 2, 2, 'Giant flying insect diving down past the listener, doppler wing roar', { range: 70, vol: 0.75 });
add('warden_slam', 'mob', 2, 1, 'Enormous armoured insect slamming the ground, deep boom and chitin rattle', { range: 80, vol: 0.85 });

// world objects
const obj = { range: 30, pitch: 0.05, vol: 0.6 };
add('obj_scrap', 'object', 1.5, 3, 'Rummaging through a pile of scrap metal, clanking and dragging', obj);
add('obj_tin', 'object', 1.2, 2, 'Prying open a metal coffee tin lid, tin pop and rattle of contents', obj);
add('obj_egg', 'object', 1.5, 3, 'Alien egg bursting open, wet gooey splat and tearing membrane', { ...obj, vol: 0.7 });
add('obj_film', 'object', 1.2, 2, 'Metal film canister lid unscrewed and film reel lifted out', obj);
add('obj_callbox', 'object', 2, 1, 'Old police call box telephone, handset lifted, crank ringer turned, distant operator line crackle', obj);
add('obj_arclamp', 'object', 2, 1, 'Giant carbon arc lamp switched on, heavy clunk and loud buzzing ignition', { ...obj, range: 60, vol: 0.75 });
add('obj_mailbox', 'object', 1, 1, 'Iron mailbox door creaking open and letters shuffled', obj);
add('obj_reel', 'object', 1.5, 2, 'Film reel unspooling and spinning freely, celluloid flutter', obj);
add('obj_usher', 'object', 2, 2, 'Thick sticky spider-web silk pulled apart by hand, stretchy gooey tearing', obj);
add('obj_spire_charge', 'object', 3, 1, 'Alien organic spire charging up, deep rising pulsing hum and crackling', { ...obj, range: 80, vol: 0.75 });
add('obj_spire_blast', 'object', 3, 1, 'Alien organic spire exploding, wet heavy blast and debris raining down', { ...obj, range: 120, vol: 0.9 });
add('obj_redcar_bell', 'object', 1.5, 1, 'Electric streetcar trolley bell, clang clang', { ...obj, range: 80, vol: 0.75 });
add('obj_redcar_arrive', 'object', 5, 1, 'Electric streetcar trolley arriving and braking with screeching steel wheels and a hiss', { ...obj, range: 90, vol: 0.75 });
add('obj_redcar_depart', 'object', 5, 1, 'Electric streetcar trolley departing, motor whine rising and wheels rumbling away', { ...obj, range: 90, vol: 0.7 });
add('obj_portal', 'object', 2, 1, 'Heavy old theatre doors swinging open with a whoosh of stale air and an echo', { ...obj, range: 0, vol: 0.7 });
add('obj_door', 'object', 1.5, 2, 'Old wooden door opening with a creak', obj);
add('obj_chest', 'object', 1.5, 2, 'Heavy wooden crate lid prised open with a crowbar, nails creaking', obj);
add('obj_grenade', 'object', 2, 2, 'Pineapple hand grenade explosion, sharp boom and shrapnel', { ...obj, range: 90, vol: 0.85 });
add('craft_work', 'object', 4, 1, 'Workshop crafting: hammering, sawing and tools clinking at a workbench, steady', { loop: true, range: 25, vol: 0.45 });
add('craft_done', 'object', 1.2, 1, 'Final hammer tap and a satisfied tool set down on a wooden bench', { range: 0, vol: 0.55 });
add('cook_work', 'object', 4, 1, 'Food sizzling in a cast iron skillet on a stove, steady', { loop: true, range: 25, vol: 0.45 });

// positional loops in the world
const wl = { loop: true, range: 30, vol: 0.4 };
add('loop_arclamp', 'world', 6, 1, 'Carbon arc lamp electrical buzzing hum, steady', { ...wl, range: 25, vol: 0.3 });
add('loop_fire', 'world', 8, 1, 'Small campfire crackling and popping, steady', wl);
add('loop_spire', 'world', 8, 1, 'Alien organic spire pulsing with a low throbbing hum and wet breathing, steady', { ...wl, range: 45 });
add('loop_eggsac', 'world', 8, 1, 'Giant alien egg sac pulsing, slow wet heartbeat and squelching, steady', { ...wl, range: 40, vol: 0.5 });
add('loop_projector', 'world', 6, 1, `${ERA} film projector running, mechanical clatter of the reel, steady`, { ...wl, range: 25 });
add('loop_redcar', 'world', 6, 1, 'Electric streetcar running on rails, motor hum and wheel clatter, steady', { ...wl, range: 60, vol: 0.5 });
add('loop_radio', 'world', 8, 1, `${ERA} radio playing scratchy jazz faintly through a speaker, steady`, { ...wl, range: 15, vol: 0.3 });
add('loop_fountain', 'world', 8, 1, 'Small stone fountain trickling water, steady', { ...wl, range: 20 });

// ambience (2D beds, seamless, stereo; the game crossfades by subzone and time of day)
const amb = { loop: true, range: 0, vol: 0.45, inf: 0.35 };
const AMB = (id, prompt, o = {}) => add(id, 'ambient', 22, 1, prompt, { ...amb, ...o });
AMB('amb_boulevard_day', `Busy ${ERA} city boulevard during the day, distant old car horns, footsteps, murmuring pedestrians, a far-off streetcar bell`);
AMB('amb_boulevard_night', 'Quiet city street at night, distant traffic hum, a lone car passing far away, crickets');
AMB('amb_hills_day', 'Dry southern California hillside in daytime, warm wind through dry brush and grasses, distant birds');
AMB('amb_hills_night', 'Southern California hills at night, crickets, gentle wind, distant coyote howl');
AMB('amb_alley', 'Narrow back alley, dripping drainpipe, distant echoes of the city, a cat rustling in trash');
AMB('amb_rooftops', 'Windy city rooftops, gusts of wind, flapping canvas, distant city below');
AMB('amb_backlot', `${ERA} movie studio backlot, distant hammering and carpentry, a megaphone shout far away, crew chatter`);
AMB('amb_hub', 'Crowded outdoor plaza, many people chattering and murmuring, footsteps, occasional laughter');
AMB('amb_hivewall', 'Alien hive wall, deep organic throbbing hum, wet clicking and distant insect chittering, ominous');
AMB('amb_hive', 'Inside an alien insect nest, wet organic breathing, dripping slime, constant insect chittering in the dark');
AMB('amb_dungeon', 'Grand abandoned old movie palace interior, cavernous echoing silence, dust settling, distant creaks and faint insect skittering');
AMB('amb_lobby', 'Abandoned theatre lobby, faint echoing room tone, flickering electric light buzz, distant scuttling');
AMB('amb_stage', 'Empty theatre stage, hollow creaking boards, rustling heavy curtains, high cavernous space');
AMB('amb_ballroom', 'Empty grand hotel ballroom interior, faint chandelier tinkle, hollow room tone and distant wind');
AMB('amb_gym', 'Old school gymnasium interior, echoing room tone, distant squeak and creak');
AMB('amb_police', 'Inside a 1920s police station, typewriters clacking, telephone ringing distantly, murmured voices');
AMB('amb_pass', 'Mountain pass road, strong wind gusts, distant rumble of a truck, rocks trickling');
AMB('amb_fightnight', 'Boxing arena crowd, rowdy spectators cheering and shouting, excited roar', { vol: 0.5 });
AMB('amb_rain', 'Steady rain falling on a city street, gutters gurgling', { vol: 0.4 });

// events and stingers (2D)
const ev = { range: 0, vol: 0.7 };
add('ev_siren', 'event', 6, 1, 'Hand-cranked air raid siren rising and falling, wailing over a city', { ...ev, vol: 0.6 });
add('ev_clapper', 'event', 0.6, 2, 'Film clapperboard snapping shut, sharp wooden clack', ev);
add('ev_bell', 'event', 2, 1, 'Boxing ring bell ding ding ding', ev);
add('ev_cheer', 'event', 3, 3, 'Crowd cheering and applauding loudly', ev);
add('ev_boo', 'event', 2.5, 2, 'Crowd booing and jeering', ev);
add('ev_gasp', 'event', 1.5, 2, 'Crowd gasping in shock', ev);
add('ev_applause', 'event', 4, 1, 'Theatre audience polite applause', ev);
add('ev_megaphone', 'event', 1.5, 1, `${ERA} director shouting through a megaphone, a wordless call, cone resonance`, ev);
add('ev_newsboy', 'event', 1.5, 1, 'Distant newsboy shouting, wordless call, street echo', ev);
add('ev_church', 'event', 4, 1, 'Distant church bell tolling three times', ev);
add('ev_victory', 'event', 3, 1, 'Short triumphant brass and drum victory fanfare sting', ev);
add('ev_doom', 'event', 3, 1, 'Ominous low orchestral stinger, deep drum hit and dissonant brass, danger', ev);

// ------------------------------------------------------------------------------------------------------------------ helpers
const env = Object.fromEntries(readFileSync(join(ROOT, '.env'), 'utf8').split(/\r?\n/).filter((l) => l.includes('=') && !l.trim().startsWith('#'))
  .map((l) => { const i = l.indexOf('='); return [l.slice(0, i).trim(), l.slice(i + 1).trim().replace(/^["']|["']$/g, '')]; }));
const KEY = env.FAL_AI_API_KEY;
if (!KEY) { console.error('FAL_AI_API_KEY missing from .env'); process.exit(1); }

const run = (cmd, a) => new Promise((res, rej) => execFile(cmd, a, { maxBuffer: 64 << 20 }, (e, so, se) => e ? rej(new Error(`${cmd}: ${se.slice(-400)}`)) : res(so + se)));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function peakDb(file)
{
  const o = await run('ffmpeg', ['-hide_banner', '-nostats', '-i', file, '-af', 'volumedetect', '-f', 'null', '-']);
  const m = [...o.matchAll(/max_volume:\s*(-?[\d.]+) dB/g)];
  return m.length ? Number(m[m.length - 1][1]) : -99;
}

async function generate(text, dur, loop, inf, file)
{
  for (let attempt = 1; ; ++attempt)
  {
    try
    {
      const r = await fetch('https://fal.run/fal-ai/elevenlabs/sound-effects/v3', {
        method: 'POST', headers: { Authorization: `Key ${KEY}`, 'Content-Type': 'application/json' },
        body: JSON.stringify({ text, duration_seconds: dur, loop, prompt_influence: inf, output_format: 'pcm_44100', enable_safety_checker: false }) });
      const j = await r.json().catch(() => ({}));
      if (!r.ok || !j.audio?.url) throw new Error(`HTTP ${r.status} ${JSON.stringify(j).slice(0, 200)}`);
      const a = await fetch(j.audio.url);
      if (!a.ok) throw new Error(`download ${a.status}`);
      writeFileSync(file, Buffer.from(await a.arrayBuffer()));
      // ElevenLabs sometimes returns a near-silent take (mostly on very short requests): re-roll those
      const pk = await peakDb(file);
      if (pk < -35) { if (attempt < 5) { console.warn(`  quiet take (${pk} dB), re-rolling`); continue; } unlinkSync(file); throw new Error(`only quiet takes (${pk} dB)`); }
      return;
    }
    catch (e)
    {
      if (attempt >= 5 || /quiet takes/.test(e.message)) throw e;
      console.warn(`  retry ${attempt}: ${e.message}`);
      await sleep(2000 * attempt);
    }
  }
}

// Mastering. One-shots: mono, silence trimmed at both ends, 15 ms fade-in guard, 40 ms fade-out, peak -1 dBFS, 96 kbps.
// Loops: untouched length (seamless), one gain to peak -2 dBFS, stereo 128 kbps (mono 96 for positional loops).
async function master(src, dst, o)
{
  const stereo = o.stereo ?? (o.loop && !o.range);
  const ch = stereo ? 2 : 1;
  const last = (txt, re) => { const m = [...txt.matchAll(re)]; return m.length ? Number(m[m.length - 1][1]) : 0; };
  const probe = await run('ffmpeg', ['-hide_banner', '-nostats', '-i', src, '-ac', String(ch), '-af', 'volumedetect', '-f', 'null', '-']);
  const peak = last(probe, /max_volume:\s*(-?[\d.]+) dB/g);
  const gain = `volume=${((o.loop ? -2 : -1) - peak).toFixed(2)}dB`;
  if (o.loop)
  {
    await run('ffmpeg', ['-hide_banner', '-y', '-loglevel', 'error', '-i', src, '-ac', String(ch), '-ar', '44100', '-af', gain, '-c:a', 'libmp3lame', '-b:a', stereo ? '128k' : '96k', dst]);
    return { samples: Math.round(last(probe, /n_samples:\s*(\d+)/g) / ch) };
  }
  // one-shots: normalise first, then trim silence relative to the normalised level, then short fades
  const trim = `${gain},silenceremove=start_periods=1:start_threshold=-50dB:start_silence=0.01,areverse,silenceremove=start_periods=1:start_threshold=-58dB:start_silence=0.03,areverse`;
  const p2 = await run('ffmpeg', ['-hide_banner', '-nostats', '-i', src, '-ac', '1', '-af', `${trim},volumedetect`, '-f', 'null', '-']);
  const secs = last(p2, /n_samples:\s*(\d+)/g) / 44100;
  const af = secs > 0.06 ? `${trim},afade=t=in:d=0.004,afade=t=out:st=${Math.max(0, secs - 0.04).toFixed(3)}:d=0.04` : `${gain},afade=t=in:d=0.004`;
  await run('ffmpeg', ['-hide_banner', '-y', '-loglevel', 'error', '-i', src, '-ac', '1', '-ar', '44100', '-af', af, '-c:a', 'libmp3lame', '-b:a', '96k', dst]);
  return { samples: 0, secs };
}

// ---------------------------------------------------------------------------------------------------------------------- run
mkdirSync(SRC, { recursive: true }); mkdirSync(OUT, { recursive: true });
const ids = new Set();
for (const [id] of C) { if (ids.has(id)) throw new Error(`duplicate id ${id}`); ids.add(id); }
const jobs = [];
let seconds = 0;
for (const [id, cat, dur, n, prompt, o] of C)
  for (let v = 0; v < n; ++v)
  {
    const src = join(SRC, `${id}_${v}.wav`);
    const fresh = ONLY ? ONLY.test(id) && (REDO || !existsSync(src)) : (REDO || !existsSync(src));
    // under ~1 s the model tends to return near-silence; one-shots are trimmed afterwards anyway
    const d = o.loop ? dur : Math.max(1.0, dur);
    if (fresh) { jobs.push({ id, v, dur: d, prompt, o, src }); seconds += d; }
  }
console.log(`${C.length} sounds, ${C.reduce((s, r) => s + r[3], 0)} files; ${jobs.length} to generate (${seconds.toFixed(0)} s, about $${(seconds * 0.002).toFixed(2)})`);
if (DRY) process.exit(0);

let done = 0, failed = 0;
const q = jobs.slice();
await Promise.all(Array.from({ length: JOBS }, async () =>
{
  while (q.length)
  {
    const j = q.shift();
    try { await generate(j.prompt, j.dur, !!j.o.loop, j.o.inf ?? 0.45, j.src); console.log(`[${++done}/${jobs.length}] ${j.id}_${j.v}`); }
    catch (e) { ++failed; console.error(`FAILED ${j.id}_${j.v}: ${e.message}`); }
  }
}));

// master everything that has a source (cheap; keeps the index and the mp3s in step with the catalogue)
const index = {};
const keep = new Set();
const mq = [];
for (const [id, cat, dur, n, prompt, o] of C)
{
  const e = index[id] = { cat, files: [], vol: o.vol ?? 0.6, range: o.range ?? 40, pitch: o.pitch ?? 0 };
  if (o.loop) { e.loop = true; e.samples = []; }
  for (let v = 0; v < n; ++v)
  {
    const src = join(SRC, `${id}_${v}.wav`);
    if (!existsSync(src)) continue;
    const name = `${id}_${v}.mp3`;
    keep.add(name);
    e.files.push(name);
    if (o.loop) e.samples.push(0);
    mq.push({ e, idx: e.files.length - 1, src, dst: join(OUT, name), o, redo: !ONLY || ONLY.test(id) });
  }
}
await Promise.all(Array.from({ length: 6 }, async () =>
{
  while (mq.length)
  {
    const m = mq.shift();
    if (!m.redo && existsSync(m.dst) && !m.o.loop) continue;
    const r = await master(m.src, m.dst, m.o);
    if (m.o.loop) m.e.samples[m.idx] = r.samples;
  }
}));
for (const f of readdirSync(OUT)) if (f.endsWith('.mp3') && !keep.has(f)) { unlinkSync(join(OUT, f)); console.log(`removed stale ${f}`); }
for (const id of Object.keys(index)) if (!index[id].files.length) delete index[id];
writeFileSync(INDEX, JSON.stringify({ note: 'Generated by tools/gen-sfx.mjs (fal.ai ElevenLabs SFX v3). Do not edit by hand.', sounds: index }, null, 1) + '\n');
console.log(`wrote ${Object.keys(index).length} sounds to Content/Data/sfx.json; ${failed} failed`);
