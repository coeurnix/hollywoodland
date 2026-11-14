// 300 simulated-player personas (DESIGN-BIBLE §11). Deterministic from a seed.
import { BANDS } from './items.mjs';
import { ARCHETYPES, COUNTRIES, JAM, FOUND, AI_STANCE } from './archetypes.mjs';

export function rng(seed) {
  let s = seed >>> 0;
  const f = () => { s = (s + 0x6D2B79F5) >>> 0; let t = Math.imul(s ^ (s >>> 15), 1 | s); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  f.int = (a, b) => a + Math.floor(f() * (b - a + 1));
  f.pick = (arr) => arr[Math.floor(f() * arr.length)];
  f.chance = (p) => f() < p;
  return f;
}

const NAMES = ('Brickwall Dutch Mabel Sprocket Flapjack Kilowatt Trixie Grifter Moxie Lefty Dollface Sarge Knuckles Peaches Duke Vera Gatsby Bugsmasher Reno Slim ' +
  'Tallulah Biscuit Ratchet Dynamite Opal Hooch Zeppelin Pinky Rosco Velma Jitterbug Ace Dixie Hammer Lulu Bootleg Kodiak Fizz Marlowe Sable Tin Lizzie Cricket Buster Nell ' +
  'Wrench Gigi Pops Mack Stella Fuse Hobo Juniper Cinder Otis Roxie Volt Hazel Pickles Dagger Birdie Clutch Maude Nitro Sparks Agnes Tumbleweed Rex Pearl Gunther Skeets ' +
  'Dot Bramble Jinx Cash Etta Torque Mimsy Bolt Harlow Gus Flossie Piston Winnie Shiv Clem Ruby Diesel Myrtle Tank Fern Smokey Ida Rivet Queenie Chaplain Bix Nora Anvil Lottie ' +
  'Static Hattie Ghost Mugsy Faye Crank Sadie Ember Wally Zelda Grit Blanche Ohm Irma Rook Goldie Padre Minnie Flint Edna Amp Kit Thelma Jolt Cora Slick Bess Cog Viola ' +
  'Brimstone Posey Tracer Alma Chisel Daisy Mortar Ginger Latch Wilma Powder Trudy Barrel Elsie Socket Marge Hex Olive Vandal Betty Ridge Polly Carbine Sue').split(' ');
const DECOR = [(n) => n, (n) => n, (n) => n, (n) => n.toLowerCase(), (n) => n + 'x', (n) => 'xX' + n + 'Xx', (n) => n + 'theGreat', (n) => 'Big' + n, (n) => 'Lil' + n, (n) => n + 'McGee',
  (n) => n + 'lol', (n) => 'Dr' + n, (n) => n + 'Jr', (n) => 'Old' + n, (n) => n + 'z', (n) => 'Sir' + n, (n) => n + 'ington', (n) => 'Miss' + n, (n) => n + 'ski', (n) => 'El' + n];
const TEMPERAMENTS = ['helpful', 'mercenary', 'quiet', 'loud', 'competitive', 'joker'];
const CLASSES = ['veteran', 'stuntman', 'gunman', 'electrician', 'powderman', 'nurse', 'preacher', 'mechanic'];
const ABBREV = ['lfg', 'ty', 'np', 'gz', 'brb', 'afk', 'omw', 'wts', 'wtb', 'lol', 'tbh', 'imo', 'rn', 'oom', 'inc', 'gg', 'pst', 'kk'];

// Handles a jam crowd would pick alongside the period ones.
const MODERN = ('noodle pixel kiwi tofu mango glitch voxel shader lambda quokka waffle pesto mochi panda bytes tetra nacho ramen nova zen ' +
  'pogo fjord comet biscotti yuzu ember sprout lumen orbit pickle kebab taco dumpling crumb sock moth gecko otter ferret wombat').split(' ');
const MDECOR = [(n) => n, (n) => n + '_dev', (n) => n + r2(), (n) => 'the' + n, (n) => n + 'games', (n) => 'x' + n, (n) => n + '_', (n) => n[0].toUpperCase() + n.slice(1) + 'Boi',
  (n) => 'not' + n, (n) => n + 'Enjoyer', (n) => 'mr_' + n, (n) => n + 'chan', (n) => n.replace(/o/g, '0').replace(/e/g, '3')];
let r2 = () => '42';
const EXTRA_MODERN = ('cactus marmot axolotl capybara narwhal pangolin lychee wasabi miso udon churro paprika saffron basil fennel clover thistle bramblejam ' +
  'quasar nebula pulsar photon quark boson tensor vector bitshift kernel sprite tilemap polygon raycast hitbox lerp shaderz ' +
  'pebble pixie gremlin goblin sprocketz banjo kazoo ukulele tuba cello bongo marimba yodel loaf scone crumpet bagel pretzel ' +
  'tundra canyon delta lagoon mesa geyser glacier boreal monsoon sirocco zephyr cobalt umber sienna teal ochre magenta ' +
  'beetle hornet heron puffin lemur tapir okapi ibex yak sloth koala dingo badger mole vole stoat').split(' ');
const EXTRA_NAMES = ('Clarence Mildred Eustace Winifred Horace Beatrix Ambrose Lavinia Cornelius Philomena Thaddeus Ottoline Barnaby Henrietta ' +
  'Mortimer Josephine Archibald Florence Percival Dorothea Leopold Gertrude Rufus Imogen Silas Augusta Virgil Cordelia Jasper Prudence').split(' ');
const pickW = (r, list, wOf = (x) => x.w) => { let x = r() * list.reduce((s, e) => s + wOf(e), 0); for (const e of list) if ((x -= wOf(e)) <= 0) return e; return list[list.length - 1]; };

// The game has only been up for the jam: nearly everyone is new. A few no-lifers are far ahead (and are talked about).
function levelFor(r, arch) {
  const fast = arch.id === 'speedrunner' ? 0.7 : arch.id === 'minmax' || arch.id === 'silentpro' ? 0.3 : arch.id === 'noob' || arch.id === 'tourist' ? 0 : 0.045;
  if (r.chance(fast)) return r.chance(0.3) ? 60 : r.int(40, 59);
  const x = r();
  if (arch.id === 'noob' || arch.id === 'tourist') return x < 0.75 ? r.int(1, 5) : r.int(6, 10);
  if (x < 0.66) return r.chance(0.6) ? r.int(1, 6) : r.int(7, 12);
  if (x < 0.90) return r.int(13, 25);
  return r.int(26, 39);
}

export function buildPersonas({ zones, dungeons, guilds, items, classes }, seed = 1930) {
  const r = rng(seed);
  r2 = () => String(r.int(2, 99));
  const clamp = (v) => Math.max(0, Math.min(1, v));
  const used = new Set(), oldUsed = new Set();
  // Handles: their own random stream. A root word ("tofu", "Mabel") is used once, and a decoration ("…Enjoyer", "xX…Xx",
  // "Lil…") at most twice in the whole list, so the who list never reads like tofuEnjoyer, waffleEnjoyer, pestoEnjoyer.
  const hr = rng(seed ^ 0x5eed), roots = new Set(), decorN = new Map();
  const H_MODERN = [...MODERN, ...EXTRA_MODERN], H_NAMES = [...NAMES, ...EXTRA_NAMES];
  const H_MDECOR = [[(n) => n, 99], [(n) => n + '_dev', 2], [(n) => n + String(hr.int(2, 99)), 3], [(n) => 'the' + n, 2], [(n) => n + 'games', 2], [(n) => 'x' + n, 2], [(n) => n + '_', 2],
    [(n) => n[0].toUpperCase() + n.slice(1) + 'Boi', 1], [(n) => 'not' + n, 1], [(n) => n + 'Enjoyer', 1], [(n) => 'mr_' + n, 2], [(n) => n + 'chan', 1], [(n) => n.replace(/o/g, '0').replace(/e/g, '3'), 2],
    [(n) => n[0].toUpperCase() + n.slice(1), 99], [(n) => n + 'fan', 1], [(n) => 'its' + n, 1], [(n) => n + 'Main', 1], [(n) => n + 'IRL', 1]];
  const H_DECOR = DECOR.map((f, i) => [f, i < 3 ? 99 : 2]);
  const makeHandle = () => {
    for (let t = 0; ; t++) {
      const modern = hr.chance(0.45);
      const root = hr.pick(modern ? H_MODERN : H_NAMES);
      const di = hr.int(0, (modern ? H_MDECOR : H_DECOR).length - 1), [f, cap] = (modern ? H_MDECOR : H_DECOR)[di];
      const key = (modern ? 'm' : 'n') + di;
      const h = f(root);
      if (t < 5000 && (roots.has(root.toLowerCase()) || (decorN.get(key) ?? 0) >= cap)) continue;
      if (used.has(h.toLowerCase()) || h.length > 14 || h.length < 3) continue;
      roots.add(root.toLowerCase()); decorN.set(key, (decorN.get(key) ?? 0) + 1); used.add(h.toLowerCase());
      return h;
    }
  };
  const personas = [];
  const armourSlots = ['hat', 'coat', 'trousers', 'shoes', 'gloves', 'belt', 'wrist', 'neck', 'ring'];
  const bandFor = (lvl) => { let b = 0; BANDS.forEach((req, i) => { if (req <= lvl) b = i; }); return b; };
  const bySlot = new Map();
  for (const it of items) { if (it.kind !== 'armor' && it.kind !== 'weapon') continue; const k = `${it.slot}|${it.req}`; if (!bySlot.has(k)) bySlot.set(k, []); bySlot.get(k).push(it); }
  const ORDER = ['cloth', 'leather', 'padded', 'steel'];

  for (let i = 0; i < 300; i++) {
    let handle;
    const arch = pickW(r, ARCHETYPES);
    const home_c = pickW(r, COUNTRIES);
    // the old draw still runs (and is thrown away) so every other persona field keeps its value from earlier builds
    do { handle = r.chance(0.45) ? r.pick(MDECOR)(r.pick(MODERN)) : r.pick(DECOR)(r.pick(NAMES)); } while (oldUsed.has(handle.toLowerCase()) || handle.length > 14 || handle.length < 3);
    oldUsed.add(handle.toLowerCase());
    handle = makeHandle();
    const cls = classes[i % 8 === 0 ? r.int(0, 7) : CLASSES.indexOf(r.pick(CLASSES))];
    const level = levelFor(r, arch);
    const fits = zones.filter((z) => level >= z.level_min && level <= z.level_max + (z.id === 'zone_mount_lee' ? 0 : 1));
    const home = level <= 12 ? 'zone_hollywood_blvd' : (fits.length ? r.pick(fits).id : 'zone_mount_lee');
    const temperament = arch.temperament;
    // traits: the archetype's centre, varied per persona; genre experience and level pull game knowledge up
    const traits = {};
    for (const [k, v] of Object.entries(arch.t)) traits[k] = +clamp(v + (r() - 0.5) * 0.3).toFixed(2);
    traits.know = +clamp(traits.know * 0.75 + Math.min(1, level / 14) * 0.25).toFixed(2);
    const el = r(); const english = el < home_c.english[0] ? 'native' : el < home_c.english[0] + home_c.english[1] ? 'fluent' : 'basic';
    const lower = r.chance(arch.lower ?? (english === 'basic' ? 0.85 : 0.7));
    const typing = { lowercase: lower, punctuation: r.chance(arch.punct ?? (lower ? 0.1 : 0.5)), caps: r.chance(arch.caps ?? 0), abbrev: ABBREV.filter(() => r.chance(0.1 + traits.mmo * 0.4)).slice(0, 6),
      typo_rate: +(r() * r() * 0.08 * (1.4 - traits.attn) + (english === 'basic' ? 0.015 : 0)).toFixed(3), wpm: Math.round(22 + r() * 30 + traits.mmo * 30 + (english === 'basic' ? -8 : 0)) };
    const jam = home_c && pickW(r, JAM, (e) => e[1]);
    const found = pickW(r, jam[0] === 'outsider' ? FOUND.outsider : FOUND.jam, (e) => e[1])[0];
    const ai_stance = arch.id === 'sceptic' ? (r.chance(0.5) ? 'deny' : 'evasive') : pickW(r, AI_STANCE, (e) => e[1])[0];
    const guilded = level >= 10 ? r.chance(level >= 40 ? 0.85 : 0.4) : r.chance(0.08 + traits.soc * 0.1);
    let guild = null;
    if (guilded) {
      const pool = guilds.filter((g) => (g.kind === 'raid') === (level >= 55) || (g.kind !== 'raid' && level < 55) || r.chance(0.15));
      guild = r.pick(pool.length ? pool : guilds).id;
    }
    // gear: best band at or below level; quality by level; armour type the class can wear
    const band = BANDS[bandFor(level)];
    const maxType = ORDER.indexOf(cls.armour);
    const equipped = {};
    const pick = (slot, pred) => {
      const pool = (bySlot.get(`${slot}|${band}`) || []).filter(pred);
      if (!pool.length) return null;
      const wantQ = level >= 60 ? (r.chance(0.55) ? 4 : 3) : level >= 15 ? (r.chance(0.25) ? 3 : 2) : (r.chance(0.45) ? 2 : 1);
      const prim = cls.primary;
      const score = (it) => (it.q === wantQ ? 10 : it.q < wantQ ? it.q : 0) + (it.stats[prim] || 0) * 0.5 + r();
      return pool.sort((a, b) => score(b) - score(a))[0];
    };
    for (const slot of armourSlots) {
      if (level < 5 && ['neck', 'ring', 'wrist', 'belt'].includes(slot) && r.chance(0.7)) continue;
      const it = pick(slot, (x) => x.type === 'jewel' || ORDER.indexOf(x.type) === maxType || (ORDER.indexOf(x.type) < maxType && r.chance(0.08)));
      if (it) equipped[slot] = it.id;
    }
    const main = pick('main', (x) => cls.weapons.includes(x.type));
    if (main) equipped.main = main.id;
    if (cls.ranged) { const rg = pick('ranged', (x) => cls.weapons.includes(x.type)); if (rg) equipped.ranged = rg.id; }
    if ((!main || main.hands !== 2) && r.chance(0.6)) { const off = pick('off', (x) => cls.weapons.includes(x.type)); if (off) equipped.off = off.id; }

    const dng = dungeons.filter((d) => level >= d.level_min && level <= d.level_max + 2);
    const goal = level === 60 ? `gear from ${r.pick(dungeons.filter((d) => d.level_min === 60)).name}` : dng.length ? `run ${r.pick(dng).name}` : `reach level ${level + 1}`;
    const start = r.int(0, 23);
    personas.push({ id: `persona_${String(i).padStart(4, '0')}`, handle, class: cls.id, level, home_zone: home, temperament, typing,
      archetype: arch.id, traits, english, country: home_c.country, tz: home_c.tz, lang: home_c.lang, bits: english === 'native' ? [] : home_c.bits, jam: jam[0], found, ai_stance,
      skill: { reaction_s: +(1.25 - traits.skill * 0.85 - traits.attn * 0.15 + r() * 0.15).toFixed(2), mistake_rate: +(0.02 + (1 - traits.skill) * 0.2 + r() * 0.03).toFixed(3) },
      guild, goal, schedule: { start_hour: start, hours: +(1.5 + r() * 5).toFixed(1) }, professions: r.chance(0.6) ? [r.pick(['prof_salvage', 'prof_forage', 'prof_skin']), r.pick(['prof_guns', 'prof_metal', 'prof_leather', 'prof_tailor', 'prof_pharm', 'prof_electric'])] : [],
      mount: level >= 60 ? r.pick(['Thoroughbred', 'Harley-Davidson VL', 'Duesenberg Model J', 'Cord L-29']) : level >= 40 ? r.pick(['Saddle Horse', 'Indian Scout', 'Ford Model A']) : null,
      body: r.int(0, 1), skin: r.int(0, 7), hair: r.int(0, 11), equipped, friends: [], built: true });
  }
  // friendships: a few links, mostly within guilds / level range
  for (const p of personas) {
    const near = personas.filter((q) => q !== p && (q.guild && q.guild === p.guild || Math.abs(q.level - p.level) <= 3));
    for (let k = 0; k < 2 && near.length; k++) { const f = r.pick(near); if (!p.friends.includes(f.id)) p.friends.push(f.id); }
  }
  return personas;
}
