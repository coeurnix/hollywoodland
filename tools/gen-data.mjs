// Content generator: writes hollywoodland-ue/Content/Data/*.json (deterministic). usage: node tools/gen-data.mjs
// Order follows DESIGN-BIBLE §12. A validator at the end checks that every referenced ID exists.
import { writeFileSync, mkdirSync, existsSync, readFileSync, readdirSync, copyFileSync, rmSync } from 'node:fs';
import { zones, dungeons, factions, redcar, guilds, mapLayout } from './gen/world.mjs';
import { classes, abilities } from './gen/classes.mjs';
import { talents as handTalents } from './gen/talents.mjs';
import { buildItems, questItem } from './gen/items.mjs';
import { mobs, areas } from './gen/zone.mjs';
import { quests } from './gen/quests.mjs';
import { buildPersonas } from './gen/personas.mjs';
import { templates, corpus, convos } from './gen/chat.mjs';

const OUT = new URL('../hollywoodland-ue/Content/Data/', import.meta.url);
mkdirSync(OUT, { recursive: true });
const errors = [];
const err = (m) => errors.push(m);

// ---- items ----
const items = buildItems();
// Tooltip text and icon slots made by tools/itemgen (keyed by item name), carried across regenerations.
const nameKey = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '');
const readJson = (u) => (existsSync(u) ? JSON.parse(readFileSync(u, 'utf8')) : null);
const itemText = readJson(new URL('./itemgen/state/descriptions.json', import.meta.url)) || {};
const atlasMeta = readJson(new URL('../item-art/atlas/item_icons.json', import.meta.url));
const itemIcons = atlasMeta?.icons || {};
if (atlasMeta) {
  // the client reads the sheets plus a small layout file from Content/Data/Icons
  const iconDir = new URL('../hollywoodland-ue/Content/Data/Icons/', import.meta.url);
  mkdirSync(iconDir, { recursive: true });
  // ship the JPEG copy of each sheet when itemgen made one
  const ship = atlasMeta.sheets.map((f) => (existsSync(new URL(`../item-art/atlas/${f.replace('.png', '.jpg')}`, import.meta.url)) ? f.replace('.png', '.jpg') : f));
  for (const f of readdirSync(iconDir)) if (/^item_icons_\d+\.(png|jpg)$/.test(f) && !ship.includes(f)) rmSync(new URL(f, iconDir));
  for (const f of ship) copyFileSync(new URL(`../item-art/atlas/${f}`, import.meta.url), new URL(f, iconDir));
  writeFileSync(new URL('icons.json', iconDir), JSON.stringify({ tile: atlasMeta.tile, sheet_size: atlasMeta.sheet_size, columns: atlasMeta.columns, per_sheet: atlasMeta.per_sheet, sheets: ship }));
}
for (const it of items) {
  const k = nameKey(it.name);
  if (itemText[k]) it.desc = itemText[k].description;
  if (itemIcons[k]) it.icon = itemIcons[k].index;
}
const byNameReq = new Map(), byName = new Map();
for (const it of items) { byNameReq.set(`${it.name}|${it.req}`, it); if (!byName.has(it.name)) byName.set(it.name, it); }
const itemId = (name, req) => {
  const it = req != null ? byNameReq.get(`${name}|${req}`) : byName.get(name);
  if (!it) { err(`item not found: ${name} (${req})`); return ''; }
  return it.id;
};
// quest items (counter-based in the client, rows exist so every reference resolves)
for (const q of quests) for (const o of q.objectives) {
  if ((o.type === 'item' || o.type === 'object') && o.item && !byName.has(o.item)) { const row = questItem(o.item); items.push(row); byName.set(row.name, row); }
  if (o.item) o.item_id = byName.get(o.item).id;
}
for (const q of quests) {
  q.rewards.choice = q.rewards.choice.map(([n, r]) => itemId(n, r)).filter(Boolean);
  if (q.takes) q.takes = q.takes.map(([n, c]) => ({ item: itemId(n), count: c }));
  if (q.gives_on_accept && !byName.has(q.gives_on_accept)) { const row = questItem(q.gives_on_accept); items.push(row); byName.set(row.name, row); }
}

// ---- mobs: resolve loot names ----
for (const m of mobs) {
  m.junk = m.junk.map((n) => itemId(n));
  m.trade = m.trade.map(([n, rate]) => ({ item: itemId(n), rate }));
}
mobs.find((m) => m.id === 'mob_looter').trade.push({ item: itemId('Bootleg Gin'), rate: 0.08 });
// Boss loot: two blues from a table (Egyptian named items, req 10)
const egyLoot = items.filter((i) => i.named && i.req === 10 && i.q === 3).map((i) => i.id);
for (const m of mobs) if (m.rank === 'boss') m.boss_loot = egyLoot;

// ---- NPCs: vendor stock, trainer titles ----
const className = Object.fromEntries(classes.map((c) => [c.id, c.name]));
const stock = {
  weapons: [...items.filter((i) => i.kind === 'ammo'), ...items.filter((i) => i.name === 'Scrap Metal' || i.name === 'Brass Casings'), ...items.filter((i) => i.kind === 'weapon' && i.q === 1 && i.req <= 10)].map((i) => i.id),
  armor: items.filter((i) => i.kind === 'armor' && i.q === 1 && i.req <= 10).map((i) => i.id),
  food: ['Hot Dog', 'Coffee', 'Ham Sandwich', 'Coca-Cola', 'Linen Bandage'].map((n) => itemId(n)),
  general: ['Linen Bandage', 'Johnson & Johnson Gauze Roll', 'Hot Dog', 'Coffee', '.38 Special Lead Round-Nose', '.30-30 Winchester Soft-Point', '12-Gauge Birdshot Shells'].map((n) => itemId(n)),
  pharmacy: ['Linen Bandage', 'Johnson & Johnson Gauze Roll', 'Coca-Cola'].map((n) => itemId(n)),
};
for (const a of areas) for (const n of a.npcs) {
  if (n.trains) n.title = `${className[n.trains]} Trainer`;
  if (n.stock) n.stock = stock[n.stock];
}
for (const a of areas) for (const o of a.objects) if (o.gives) o.gives_id = itemId(o.gives);
// class start gear
for (const c of classes) {
  const w = c.start.map((b) => items.find((i) => i.base === b && i.q === 1 && i.req === 1)).filter(Boolean);
  if (w.length !== c.start.length) err(`start weapon missing for ${c.id}`);
  const armour = ['coat', 'trousers', 'shoes'].map((slot) => items.find((i) => i.slot === slot && i.type === 'cloth' && i.q === 1 && i.req === 1));
  c.start_items = [...w, ...armour].map((i) => i.id);
  c.abilities = abilities.filter((a) => a.class === c.id).map((a) => a.id);
}
// ---- talents, flavour text and icons for abilities / talents / classes / specs (tools/artgen) ----
const artText = readJson(new URL('./artgen/state/texts.json', import.meta.url)) || {};
const skillMeta = readJson(new URL('../art-gen/atlas/skill_icons.json', import.meta.url));
const skillIcon = (k) => (skillMeta && skillMeta.icons[k] != null ? skillMeta.icons[k] : -1);
const designed = readJson(new URL('./artgen/state/talents.json', import.meta.url)) || {};
const talents = [...handTalents, ...classes.flatMap((c) => designed[c.id] || [])];
for (const t of talents) { t.desc = artText[t.id]?.description || ''; t.icon = skillIcon(t.id); }
for (const a of abilities) { a.flavor = artText[a.id]?.description || ''; a.icon = skillIcon(a.id); }
for (const c of classes) {
  c.desc = artText[`class_${c.id}`]?.description || ''; c.icon = skillIcon(`class_${c.id}`);
  c.spec_desc = c.specs.map((_, i) => artText[`spec_${c.id}_${i}`]?.description || '');
  c.spec_icons = c.specs.map((_, i) => skillIcon(`spec_${c.id}_${i}`));
}
{ // talent sanity: unique ids and slots, abilities exist
  const ids = new Set(), slots = new Set();
  for (const t of talents) {
    if (ids.has(t.id)) err(`duplicate talent ${t.id}`); ids.add(t.id);
    const sl = `${t.class}|${t.spec}|${t.tier}|${t.col}`; if (slots.has(sl)) err(`talent slot taken twice: ${sl}`); slots.add(sl);
    for (const k of Object.keys(t.mods)) if (k.includes(':') && !abilities.some((a) => a.id === k.split(':')[1])) err(`talent ${t.id}: unknown ability in ${k}`);
  }
}
if (skillMeta) {
  const iconDir = new URL('../hollywoodland-ue/Content/Data/Icons/', import.meta.url);
  mkdirSync(iconDir, { recursive: true });
  const ship = skillMeta.sheets.map((f) => f.replace('.png', '.jpg'));
  for (const f of readdirSync(iconDir)) if (/^skill_icons_\d+\.(png|jpg)$/.test(f) && !ship.includes(f)) rmSync(new URL(f, iconDir));
  for (const f of ship) copyFileSync(new URL(`../art-gen/atlas/${f}`, import.meta.url), new URL(f, iconDir));
  const named = Object.fromEntries(Object.entries(skillMeta.icons).filter(([k]) => k.startsWith('menu_')));
  writeFileSync(new URL('skills.json', iconDir), JSON.stringify({ tile: skillMeta.tile, sheet_size: skillMeta.sheet_size, columns: skillMeta.columns, per_sheet: skillMeta.per_sheet, sheets: ship, named }));
}
{ // interface art
  const uiSrc = new URL('../art-gen/ui/', import.meta.url);
  if (existsSync(uiSrc)) {
    const uiDir = new URL('../hollywoodland-ue/Content/Data/UI/', import.meta.url);
    mkdirSync(uiDir, { recursive: true });
    for (const f of readdirSync(uiSrc)) if (f.endsWith('.jpg')) copyFileSync(new URL(f, uiSrc), new URL(f, uiDir));
  }
}

// ---- personas ----
const personas = buildPersonas({ zones, dungeons, guilds, items, classes });

// ---- optional LLM-generated corpus (tools/gen-corpus.mjs writes tools/cache/corpus.json) ----
const cachePath = new URL('./cache/corpus.json', import.meta.url);
let extra = [];
if (existsSync(cachePath)) extra = JSON.parse(readFileSync(cachePath, 'utf8'));
const fullCorpus = [...corpus, ...extra];

// ---- validate ----
const ids = new Set();
const reg = (type, list) => { const names = new Set(); for (const r of list) { if (ids.has(r.id)) err(`duplicate id ${r.id}`); ids.add(r.id); if (type !== 'item' && type !== 'persona' && type !== 'npc' && type !== 'mob' && names.has(r.name)) err(`duplicate ${type} name ${r.name}`); names.add(r.name); } };
reg('zone', zones); reg('dungeon', dungeons); reg('faction', factions); reg('guild', guilds); reg('class', classes); reg('ability', abilities);
reg('item', items); reg('mob', mobs); reg('quest', quests); reg('persona', personas);
const npcs = areas.flatMap((a) => a.npcs); reg('npc', npcs);
const need = (id, what) => { if (!ids.has(id)) err(`${what}: unknown id ${id}`); };
for (const a of areas) for (const c of a.camps) need(c.mob, `camp ${c.id}`);
for (const m of mobs) for (const ab of m.abilities) need(ab, `mob ${m.id}`);
const FAR_NPCS = new Set(['npc_cemetery_gatekeeper', 'npc_columbia_gate', 'npc_post43_adjutant']);
for (const q of quests) {
  need(q.giver, `quest ${q.id} giver`);
  if (!FAR_NPCS.has(q.turnin)) need(q.turnin, `quest ${q.id} turnin`);
  if (q.prereq) need(q.prereq, `quest ${q.id} prereq`);
  for (const o of q.objectives) {
    for (const m of o.mobs || []) need(m, `quest ${q.id}`);
    for (const m of o.from || []) need(m, `quest ${q.id}`);
    if ((o.type === 'talk' || o.type === 'escort') && !FAR_NPCS.has(o.npc)) need(o.npc, `quest ${q.id}`);
    if (o.type === 'craft' && !byName.has(o.item)) err(`quest ${q.id}: craft item ${o.item}`);
    if ((o.type === 'object' || o.type === 'use') && !areas.some((a) => a.objects.filter((x) => x.kind === o.kind).length >= o.count)) err(`quest ${q.id}: not enough objects of kind ${o.kind}`);
    if (o.type === 'kill' && !areas.some((a) => a.camps.some((c) => o.mobs.includes(c.mob)))) err(`quest ${q.id}: no camp for ${o.mobs}`);
  }
}
for (const p of personas) { need(p.home_zone, p.id); if (p.guild) need(p.guild, p.id); for (const v of Object.values(p.equipped)) need(v, p.id); }
for (const d of dungeons) need(d.zone, d.id);
for (const z of zones) for (const n of z.neighbours) need(n, z.id);

// ---- write ----
const write = (name, obj) => { const s = JSON.stringify(obj); writeFileSync(new URL(name, OUT), s); return (s.length / 1024).toFixed(0) + ' KB'; };
const sizes = {
  world: write('world.json', { zones, dungeons, factions, redcar, guilds, map: mapLayout }),
  classes: write('classes.json', { classes, abilities, talents }),
  items: write('items.json', { items }),
  mobs: write('mobs.json', { mobs }),
  areas: write('areas.json', { areas }),
  quests: write('quests.json', { quests }),
  personas: write('personas.json', { personas }),
  chat: write('chat.json', { templates, corpus: fullCorpus, convos }),
};
const lv = (a, b) => personas.filter((p) => p.level >= a && p.level <= b).length;
console.log('sizes', sizes);
console.log(`items ${items.length}  mobs ${mobs.length}  quests ${quests.length}  npcs ${npcs.length}  personas ${personas.length} (1-12: ${lv(1, 12)}, 1-20: ${lv(1, 20)}, 21-40: ${lv(21, 40)}, 41-60: ${lv(41, 60)}, 60: ${lv(60, 60)})  corpus ${fullCorpus.length}  templates ${templates.length}`);
console.log(`camps ${areas.map((a) => a.camps.reduce((n, c) => n + c.count, 0)).join(' / ')} mobs, boxes ${areas.map((a) => a.boxes.length).join(' / ')}, objects ${areas.map((a) => a.objects.length).join(' / ')}`);
if (errors.length) { console.error(`VALIDATION FAILED (${errors.length}):\n` + [...new Set(errors)].slice(0, 40).join('\n')); process.exit(1); }
console.log('validation ok');
