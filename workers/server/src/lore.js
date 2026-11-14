// World tools over the lore pack (data/lore.json, built by tools/gen/lore.mjs). The pack holds the truth; what a
// simulated player gets back depends on who they are (AI-IMPROVEMENTS-PLAN.md §4.5):
//   knows  the fact as it is        vague  a blurred version, marked (unsure)
//   wrong  the fact with another thing's location, marked (they believe, wrongly)        none  nothing
// The grade comes from the persona's game knowledge, level and class against the entry's level and obscurity, plus a
// fixed per-persona-per-entry roll, so one persona is consistently right or wrong about one thing.
import LORE from './data/lore.json';
import NEWS from './data/news.json';

const norm = (s) => String(s || '').toLowerCase().replace(/[^a-z0-9& ]+/g, ' ').replace(/\s+/g, ' ').trim();
const ENTRIES = LORE.entries.map((e, i) => ({ ...e, i, keys: [norm(e.n).replace(/^the /, ''), ...(e.a || []).map(norm)].filter((k) => k.length >= 2) }));
const ITEMS = LORE.items.map((it, i) => ({ it, i, key: norm(it[0]) }));

function hash(s) { let h = 2166136261; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return ((h >>> 0) % 10000) / 10000; }

/** Entries whose name or alias appears in the text (longest names first), at most n. */
export function link(text, n = 4) {
  const t = ' ' + norm(text) + ' ';
  const hits = [];
  for (const e of ENTRIES) for (const k of e.keys) {
    if (k.length < 3 && !t.includes(` ${k} `)) continue;
    if (t.includes(` ${k} `) || (k.length >= 6 && t.includes(k)) || t.includes(` ${k}s `)) { hits.push({ e, len: k.length }); break; }
  }
  hits.sort((a, b) => b.len - a.len);
  const out = [];
  for (const h of hits) if (!out.some((o) => o.keys.some((k) => h.e.keys.includes(k))) && out.length < n) out.push(h.e);
  return out;
}
/** Fuzzy search by name for the find tool: entries, then items. */
export function search(q, n = 3) {
  const t = norm(q); if (!t) return [];
  const words = t.split(' ').filter((w) => w.length > 2);
  const score = (keys) => Math.max(...keys.map((k) => k === t ? 10 : k.includes(t) || t.includes(k) ? 6 + Math.min(k.length, t.length) / 20 : words.filter((w) => k.includes(w)).length));
  const es = ENTRIES.map((e) => ({ e, s: score(e.keys) })).filter((x) => x.s >= 1).sort((a, b) => b.s - a.s).slice(0, n);
  const out = es.map((x) => x.e);
  if (out.length < n) for (const x of ITEMS.map((x) => ({ x, s: score([x.key]) })).filter((y) => y.s >= 2).sort((a, b) => b.s - a.s).slice(0, n - out.length)) out.push(itemEntry(x.x));
  return out;
}
function itemEntry({ it, i }) {
  const [name, slot, type, req, q, price] = it;
  return { k: 'item', n: name, i: 100000 + i, lvl: req, d: 0.35 + q * 0.08, f: `[${name}]: ${LORE.quality[q] || 'white'} ${type ? type + ' ' : ''}${slot}, requires level ${req}, sells to a vendor for about ${price}c.`, loc: '', v: `[${name}] is a ${slot} item for around level ${Math.max(1, Math.round(req / 5) * 5)}`, keys: [norm(name)] };
}

/** How well this persona knows this entry: 'knows' | 'vague' | 'wrong' | 'none'. */
export function grade(s, e) {
  const know = typeof s.know === 'number' ? s.know : 0.5, lvl = s.level | 0;
  let x = know + (lvl >= e.lvl ? 0.18 : -0.3 * Math.min(1, (e.lvl - lvl) / 5)) - e.d;
  if (e.cls) x += e.cls === s.class ? 0.3 : -0.15;
  if (e.k === 'how' && typeof s.mmo === 'number') x += (s.mmo - 0.5) * 0.3; // genre veterans guess mechanics right
  if (s.archetype === 'lore' && (e.k === 'npc' || e.k === 'quest' || e.k === 'place')) x += 0.25;
  if (s.archetype === 'trader' && e.k === 'item') x += 0.3;
  const roll = hash(`${s.persona}|${e.i}`);
  x += (roll - 0.5) * 0.3;
  if (x >= 0.42) return 'knows';
  if (x >= 0.2) return 'vague';
  // the confident-but-wrong: likelier for people who think they know games
  if (e.loc && hash(`w|${s.persona}|${e.i}`) < 0.25 + (s.mmo || 0.5) * 0.25) return 'wrong';
  return 'none';
}
function told(s, e) {
  const g = grade(s, e);
  if (g === 'knows') return `${e.f} ${e.loc}`.trim();
  if (g === 'vague') return `(unsure) ${e.v}`;
  if (g === 'wrong') {
    const same = ENTRIES.filter((o) => o.k === e.k && o.loc && o.i !== e.i);
    const o = same.length ? same[Math.floor(hash(`o|${s.persona}|${e.i}`) * same.length)] : null;
    return o ? `(they believe, wrongly) ${e.f} ${o.loc}` : `(unsure) ${e.v}`;
  }
  return null;
}

/** Before a reply: link names in what the player typed, attach the plain names for everyone and graded facts per speaker. */
export function attach(req) {
  if (req.v < 2 || !Array.isArray(req.speakers)) return;
  const t = req.trigger || {};
  const text = req.kind === 'observe' ? '' : String(t.text || '');
  let hits = text ? link(text, 4) : [];
  // "my trainer", "the trainer": the player's own class trainer
  if (/\btrainer\b/i.test(text) && req.context?.player?.class) { const c = norm(req.context.player.class); const tr = ENTRIES.find((e) => e.k === 'npc' && e.keys.includes(`${c} trainer`)); if (tr && !hits.includes(tr)) hits.unshift(tr); }
  if (!hits.length) return;
  req._facts = hits.map((e) => `${e.n} (${e.k})`);
  for (const s of req.speakers) {
    const facts = [];
    for (const e of hits) { const f = told(s, e); facts.push(f || `(no idea about ${e.n})`); }
    s.facts = [...(Array.isArray(s.facts) ? s.facts : []), ...facts];
  }
}

/** Tools for the careful model, bound to one persona. */
export function toolsFor(s, req) {
  const tools = [
    { name: 'find', description: 'Look up a game thing by name (a quest, NPC, place, enemy, ability, item, class, the dungeon). Returns what THIS player knows about the closest matches, which may be vague or wrong.',
      parameters: { type: 'object', properties: { name: { type: 'string', description: 'the name or a few words of it' } }, required: ['name'], additionalProperties: false } },
    { name: 'how', description: 'How a game system works as far as THIS player knows: mounts, talents, dying, rested experience, professions, the auction house, grouping, duels, emotes, picture night, hive surges, money, levelling, the game itself.',
      parameters: { type: 'object', properties: { topic: { type: 'string' } }, required: ['topic'], additionalProperties: false } },
    { name: 'news', description: 'What is going on in the real world and in the game jam right now (this player reads the news like anyone).',
      parameters: { type: 'object', properties: { topic: { type: 'string', description: 'jam, games, tech, world, us, sport, film, or any' } }, required: ['topic'], additionalProperties: false } },
  ];
  const callTool = async (name, args) => {
    if (name === 'news') {
      const tag = norm(args.topic);
      if (tag.includes('jam')) return NEWS.jam.facts.join(' | ') + ` | deadline ${NEWS.jam.deadline_local}`;
      const list = NEWS.topics.filter((x) => tag === 'any' || !tag || x.tag === tag || x.text.toLowerCase().includes(tag));
      return (list.length ? list : NEWS.topics).slice(0, 4).map((x) => x.text).join(' | ');
    }
    const es = name === 'how' ? search(args.topic, 2).filter((e) => e.k === 'how').concat(search(args.topic, 1)).slice(0, 2) : search(args.name, 3);
    if (!es.length) return 'Nothing by that name comes to mind.';
    return es.map((e) => told(s, e) || `No idea about ${e.n}.`).join('\n');
  };
  return { tools, callTool };
}
