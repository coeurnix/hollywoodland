// 300 simulated-player personas (DESIGN-BIBLE §11). Deterministic from a seed.
import { BANDS } from './items.mjs';

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

function levelFor(r) {
  const x = r();
  if (x < 0.30) return r.chance(0.62) ? r.int(1, 12) : r.int(13, 20);   // 30% at 1-20, most of them in the built zone's range
  if (x < 0.55) return r.int(21, 40);
  return r.chance(0.6) ? 60 : r.int(41, 59);
}

export function buildPersonas({ zones, dungeons, guilds, items, classes }, seed = 1930) {
  const r = rng(seed);
  const used = new Set();
  const personas = [];
  const armourSlots = ['hat', 'coat', 'trousers', 'shoes', 'gloves', 'belt', 'wrist', 'neck', 'ring'];
  const bandFor = (lvl) => { let b = 0; BANDS.forEach((req, i) => { if (req <= lvl) b = i; }); return b; };
  const bySlot = new Map();
  for (const it of items) { if (it.kind !== 'armor' && it.kind !== 'weapon') continue; const k = `${it.slot}|${it.req}`; if (!bySlot.has(k)) bySlot.set(k, []); bySlot.get(k).push(it); }
  const ORDER = ['cloth', 'leather', 'padded', 'steel'];

  for (let i = 0; i < 300; i++) {
    let handle;
    do { handle = r.pick(DECOR)(r.pick(NAMES)); } while (used.has(handle.toLowerCase()) || handle.length > 14);
    used.add(handle.toLowerCase());
    const cls = classes[i % 8 === 0 ? r.int(0, 7) : CLASSES.indexOf(r.pick(CLASSES))];
    const level = levelFor(r);
    const fits = zones.filter((z) => level >= z.level_min && level <= z.level_max + (z.id === 'zone_mount_lee' ? 0 : 1));
    const home = level <= 12 ? 'zone_hollywood_blvd' : (fits.length ? r.pick(fits).id : 'zone_mount_lee');
    const temperament = r.pick(TEMPERAMENTS);
    const lower = r.chance(0.7);
    const typing = { lowercase: lower, punctuation: !lower && r.chance(0.5), caps: temperament === 'loud' && r.chance(0.5), abbrev: ABBREV.filter(() => r.chance(0.35)).slice(0, 6), typo_rate: +(r() * r() * 0.08).toFixed(3) };
    const guilded = level >= 10 ? r.chance(level === 60 ? 0.9 : 0.6) : r.chance(0.2);
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
      skill: { reaction_s: +(0.25 + r() * 0.95).toFixed(2), mistake_rate: +(0.02 + r() * 0.18).toFixed(3) },
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
