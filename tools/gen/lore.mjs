// The lore pack for the Worker's world tools (AI-IMPROVEMENTS-PLAN.md §4.5): every game thing a simulated player might
// be asked about, as short prose facts built from the same data the client uses. tools/gen-data.mjs writes it to
// workers/server/src/data/lore.json; workers/server/src/lore.js looks entries up and grades them by what a persona
// would know. Entry: { k kind, n name, a aliases, lvl level it matters at, d obscurity 0..1, cls class it belongs to,
//   f the fact, loc where it is (kept apart so a wrong answer can swap it), v the vague version }.
const cap = (s) => s.charAt(0).toUpperCase() + s.slice(1);

export function buildLore({ areas, quests, mobs, classes, abilities, items, zones, dungeons }) {
  const A = areas[0];
  const out = [];
  const spawn = { x: A.spawn[0], y: A.spawn[1] };
  const subAt = (x, y) => { const s = A.subzones.find((z) => x >= z.x && x <= z.x + z.w && y >= z.y && y <= z.y + z.d); return s ? s.name : 'Hollywood Boulevard'; };
  // x east, y south (as on the map)
  const compass = (dx, dy) => { const a = Math.atan2(dy, dx) * 180 / Math.PI; return ['east', 'south-east', 'south', 'south-west', 'west', 'north-west', 'north', 'north-east'][Math.round(((a + 360) % 360) / 45) % 8]; };
  const from = (x, y) => { const dx = x - spawn.x, dy = y - spawn.y, d = Math.hypot(dx, dy); return d < 35 ? "in Grauman's Chinese Forecourt, where everyone starts" : `in ${subAt(x, y)}, about ${Math.round(d / 10) * 10} m ${compass(dx, dy)} of the Chinese forecourt (the start)`; };
  const roughly = (x, y) => { const dx = x - spawn.x, dy = y - spawn.y, d = Math.hypot(dx, dy); return d < 60 ? 'near the start' : `somewhere ${compass(dx, dy)} along the Boulevard`; };
  const npcById = Object.fromEntries(A.npcs.map((n) => [n.id, n]));
  const mobById = Object.fromEntries(mobs.map((m) => [m.id, m]));
  const E = (e) => out.push(e);

  // ---- NPCs and services
  const roleText = { quest: 'gives quests', vendor: 'sells goods', repair: 'repairs gear', banker: 'is the bank teller', auctioneer: 'runs the auction house', innkeeper: 'is the innkeeper (rest here for rested XP, set your home)',
    redcar: 'is the red car conductor', trainer_prof: 'teaches a profession' };
  const seen = new Set();
  for (const n of A.npcs) {
    if (seen.has(n.name)) continue; seen.add(n.name);
    if (n.roles.includes('guard')) { E({ k: 'npc', n: n.name, a: ['guard', 'patrolman', 'cop'], lvl: 1, d: 0.05, f: 'Boulevard Patrolmen are the guards in the hubs: talk to one for directions to the bank, trainers, inn and so on; they do not fight for you.', loc: '', v: 'the cops standing around give directions' }); continue; }
    const cls = n.trains ? classes.find((c) => c.id === n.trains) : null;
    const does = [cls ? `is the ${cls.name} class trainer (new abilities every 2 levels)` : null, ...n.roles.filter((r) => roleText[r]).map((r) => roleText[r])].filter(Boolean).join(', ');
    E({ k: 'npc', n: n.name, a: [n.title, cls ? `${cls.name} trainer` : null].filter(Boolean).map((s) => s.toLowerCase()), lvl: 1, d: n.roles.includes('quest') && !cls ? 0.3 : 0.2, cls: n.trains || undefined,
      f: `${n.name} (${n.title}) ${does || 'is an NPC'}.`, loc: `Stands ${from(n.x, n.y)}.`, v: `${n.name} is an NPC ${roughly(n.x, n.y)}` });
  }
  const poiNames = { bank: ['First National Bank', ['bank']], auction: ['the auction house (Taft Building, Hollywood & Vine)', ['auction house', 'ah', 'auction']], mailbox: ['the mailbox', ['mail', 'mailbox']],
    redcar: ['the Pacific Electric red car stop', ['red car', 'redcar', 'tram', 'streetcar']], trainers: ['the class trainers', ['trainer', 'trainers', 'class trainer']], inn: ['the Roosevelt Hotel (the inn)', ['inn', 'roosevelt', 'hotel']],
    vendor: ['the shops at Hollywood & Highland (haberdasher, gunsmith)', ['vendor', 'shop', 'store', 'repair']], police: ['the Hollywood police station', ['police', 'police station', 'cops']], vine: ['Hollywood & Vine', ['vine']] };
  for (const [key, [name, al]] of Object.entries(poiNames)) {
    const p = A.pois[key]; if (!p) continue;
    E({ k: 'place', n: name, a: al, lvl: 1, d: key === 'trainers' || key === 'inn' ? 0.05 : 0.2, f: `${cap(name)} is on Hollywood Boulevard.`, loc: `It is ${from(p[0], p[1])}.`, v: `${name} is ${roughly(p[0], p[1])}` });
  }
  const subSeen = new Set();
  for (const s of A.subzones) {
    if (subSeen.has(s.name)) continue; subSeen.add(s.name);
    const cx = s.x + s.w / 2, cy = s.y + s.d / 2;
    const here = A.camps.filter((c) => c.x >= s.x && c.x <= s.x + s.w && c.y >= s.y && c.y <= s.y + s.d).map((c) => mobById[c.mob]).filter(Boolean);
    const names = [...new Set(here.map((m) => m.name))].slice(0, 4);
    const lv = here.length ? `levels ${Math.min(...here.map((m) => m.level_min))}-${Math.max(...here.map((m) => m.level_max))}` : null;
    E({ k: 'place', n: s.name, a: [], lvl: here.length ? Math.min(...here.map((m) => m.level_min)) : 1, d: 0.3, f: `${s.name} is an area of Hollywood Boulevard${names.length ? ` with ${names.join(', ')} (${lv})` : ''}.`,
      loc: `It is ${from(cx, cy)}.`, v: `${s.name} is ${roughly(cx, cy)}` });
  }
  for (const p of A.portals || []) E({ k: 'place', n: 'the Egyptian Theatre entrance', a: ['egyptian door', 'dungeon entrance', 'dungeon door'], lvl: 8, d: 0.35, f: 'The door into the Egyptian Theatre dungeon.', loc: `It is ${from(p.x, p.y)}.`, v: `the dungeon door is ${roughly(p.x, p.y)}` });
  for (const e of A.exits) if (!e.redcar) E({ k: 'place', n: `the ${e.name} road out`, a: [e.name.toLowerCase()], lvl: 12, d: 0.5, f: `The ${e.name} road leads out of Hollywood Boulevard but is blocked by a hive wall whose gap is held by level 22-34 elites; nobody low gets through, and nothing is built beyond it in this jam build.`, loc: `It is ${from(e.x, e.y)}.`, v: 'the roads out are blocked by a bug wall with very high level guards' });

  // ---- mobs
  for (const m of mobs) {
    const camps = A.camps.filter((c) => c.mob === m.id);
    const where = [...new Set(camps.map((c) => subAt(c.x, c.y)))].slice(0, 3);
    const traits = [m.rank !== 'normal' ? m.rank : null, m.aggressive ? 'attacks on sight' : 'leaves you alone unless hit', m.ranged ? 'attacks from range' : null, m.social ? 'calls nearby friends' : null, m.flees ? 'runs when low' : null].filter(Boolean).join(', ');
    E({ k: 'mob', n: m.name, a: [m.family ? m.family.replace(/_/g, ' ') : null].filter(Boolean), lvl: m.level_min, d: m.rank === 'normal' ? 0.2 : 0.45,
      f: `${m.name}: level ${m.level_min}${m.level_max > m.level_min ? '-' + m.level_max : ''} enemy (${traits}).`, loc: where.length ? `Found in ${where.join(', ')}.` : 'Found in the Egyptian Theatre or at events.', v: `${m.name} is an enemy around level ${Math.round((m.level_min + m.level_max) / 2 / 2) * 2 || 1}` });
  }

  // ---- quests
  const qById = Object.fromEntries(quests.map((q) => [q.id, q]));
  for (const q of quests) {
    if (q.built === false) continue;
    const g = npcById[q.giver], t = npcById[q.turnin];
    const obj = (q.objectives || []).map((o) => o.label || `${o.type} ${o.count || ''}`).join('; ');
    const where = (q.objectives || []).map((o) => {
      if (o.type === 'kill' && o.mob) { const c = A.camps.find((cc) => cc.mob === o.mob); return c ? `${mobById[o.mob]?.name || 'them'} are in ${subAt(c.x, c.y)}` : null; }
      if ((o.type === 'object' || o.type === 'use') && o.kind) { const ob = A.objects.find((x) => x.kind === o.kind); return ob ? `the ${ob.name}s are in ${subAt(ob.x, ob.y)}` : null; }
      if (o.type === 'talk' && npcById[o.npc]) return `${npcById[o.npc].name} is in ${subAt(npcById[o.npc].x, npcById[o.npc].y)}`;
      return null;
    }).filter(Boolean).join('; ');
    E({ k: 'quest', n: q.name, a: [], lvl: q.min_level || q.level, d: 0.35 + (q.group ? 0.1 : 0),
      f: `Quest "${q.name}" (level ${q.level}${q.group ? ', needs a group' : ''}${q.dungeon ? ', inside the Egyptian Theatre' : ''}): ${obj}. Reward ${q.rewards?.xp || 0} XP${q.prereq && qById[q.prereq] ? `; needs "${qById[q.prereq].name}" done first` : ''}${q.next && qById[q.next] ? `; leads to "${qById[q.next].name}"` : ''}.`,
      loc: `${g ? `Given by ${g.name} in ${subAt(g.x, g.y)}` : 'Picked up along the way'}${t && t !== g ? `, handed in to ${t.name} in ${subAt(t.x, t.y)}` : ''}${where ? `; ${where}` : ''}.`, v: `"${q.name}" is a quest around level ${q.level}` });
  }

  // ---- classes and abilities
  for (const c of classes) {
    const tr = A.npcs.find((n) => n.trains === c.id);
    E({ k: 'class', n: c.name, a: [c.id], lvl: 1, d: 0.15, f: `${c.name}: a ${c.role} class (${c.resource}, wears ${c.armour}; specs ${c.specs.join(', ')}).`, loc: tr ? `Trainer: ${tr.name} in the Chinese forecourt.` : '', v: `${c.name} is one of the eight classes, a ${c.role}` });
  }
  for (const ab of abilities) {
    const c = classes.find((x) => x.id === ab.class);
    E({ k: 'ability', n: ab.name, a: [], lvl: ab.level, d: 0.45, cls: ab.class, f: `${ab.name}: ${c ? c.name : ''} ability learned at level ${ab.level}. ${ab.desc || ''}`.trim(), loc: '', v: `${ab.name} is a ${c ? c.name : 'class'} ability` });
  }

  // ---- the dungeon, zones
  for (const d of dungeons) {
    if (d.built) E({ k: 'dungeon', n: d.name, a: ['egyptian', 'the egyptian', 'dungeon'], lvl: d.level_min, d: 0.3,
      f: `${d.name}: the 5-player dungeon, levels ${d.level_min}-${d.level_max}. Bosses in order: ${d.bosses.join(', ')}. Brood Keeper: break the eggs before they hatch. Hive Warden charges. Spitter Matriarch leaves acid pools, do not stand in them and use the pillars to block her spit. The Hive Mother calls waves of adds and an egg sac heals her: kill the sac. Bosses drop blue gear, rolled need or greed. Sid Grauman gives the quest for it at level 8.`,
      loc: 'The door is on Hollywood Boulevard between Highland and Vine, on the south side.', v: `${d.name} is the dungeon, for around level ${d.level_min}+, needs a group` });
    else E({ k: 'dungeon', n: d.name, a: [], lvl: d.level_min, d: 0.7, f: `${d.name} is listed for levels ${d.level_min}-${d.level_max} but is not built in this jam build; nobody can go there.`, loc: '', v: `${d.name} is some higher level place that is not in the game yet` });
  }
  for (const z of zones) {
    if (z.id === 'zone_hollywood_blvd') E({ k: 'zone', n: z.name, a: ['boulevard', 'the boulevard', 'the zone'], lvl: 1, d: 0.05, f: `${z.name} (levels ${z.level_min}-${z.level_max}) is the only zone actually built: ${z.blurb}`, loc: '', v: 'Hollywood Boulevard is the starting zone' });
    else E({ k: 'zone', n: z.name, a: [], lvl: z.level_min, d: 0.65, f: `${z.name} (levels ${z.level_min}-${z.level_max}) is on the map but not built in this jam build; the roads to it are walled off.`, loc: '', v: `${z.name} is another zone you cannot reach` });
  }

  // ---- how things work
  const H = (n, a, lvl, d, f, v) => E({ k: 'how', n, a, lvl, d, f, loc: '', v });
  H('mounts', ['mount', 'horse', 'car', 'motorcycle', 'riding'], 30, 0.4, 'Mounts need level 40 and $80 (the fast ones at 60 cost far more): horses, an Indian Scout motorcycle, a Ford Model A. You cannot get one in this build because the zone only goes to level 12; the few riders around are players far ahead of everyone.', 'mounts are for much higher levels and cost a lot');
  H('talents', ['talent', 'talents', 'spec', 'talent tree'], 10, 0.4, 'Talents start at level 10: one point per level, three trees per class, a new tier every 5 points in a tree. Press N. Respec at your trainer for a fee that grows each time.', 'talents unlock around level 10');
  H('dying', ['death', 'die', 'died', 'corpse', 'ghost', 'spirit', 'resurrect'], 1, 0.15, 'When you die you release your spirit and run back to your corpse as a ghost, or resurrect at an aid station with sickness (-25% stats for a while). Gear loses durability; repair at a vendor with the repair role.', 'when you die you run back to your body');
  H('rested experience', ['rested', 'rested xp', 'rest'], 3, 0.5, 'Sitting at the inn (the Roosevelt) or logging out there builds rested XP, which doubles kill XP until it runs out.', 'resting at the inn gives some kind of XP bonus');
  H('professions', ['profession', 'salvage', 'salvaging', 'gunsmith', 'gunsmithing', 'crafting'], 5, 0.5, 'Professions: Salvaging (learn from Ike Brandt at Hollywood & Vine; scrap nodes around the zone) and Gunsmithing (Earl Tanner at Hollywood & Highland; makes ammunition and guns from salvage). Press K.', 'there are a couple of crafting professions');
  H('the auction house', ['bid', 'buyout', 'listing', 'undercut'], 4, 0.4, 'The auction house is in the Taft Building at Hollywood & Vine (Cornelius Vane, Miss Opal Reed). List from your bags for 1, 4 or 12 hours with a deposit; other players bid, buy out and undercut. Sales and returns arrive by mail.', 'you can sell things to other players at the auction house');
  H('grouping', ['party', 'group', 'invite', 'lfg'], 1, 0.15, 'Right-click a player or type /invite <name> to group (up to five). Kill credit and quest kills are shared; loot from bosses is rolled need or greed. /leave to drop the group. The LFG channel (/4) is where people look for groups.', 'you can invite people to a party');
  H('duels', ['duel', 'pvp'], 1, 0.4, 'Type /duel <name> outside the hubs. Nobody dies: the loser stops at 1 health; walking 40 m away forfeits. Fight night outside the Roosevelt ends with the champion challenging players for a purse.', 'you can duel people outside town');
  H('chat and emotes', ['emote', 'emotes', 'commands', 'chat', 'whisper'], 1, 0.2, 'Chat: /s say, /y yell, /1 general, /2 trade, /4 lfg, /p party, /g guild, /w <name> whisper, /r reply, /who. Emotes: /dance /wave /bow /cheer /laugh /silly /flirt /train /chicken /cry /point /shrug /sit.', 'there are slash commands for chat and emotes, try /help');
  H('picture night', ['movie', 'film', 'cinema', 'picture'], 1, 0.5, "Picture night: about every 45 minutes a real pre-1930 film shows at Grauman's Chinese; sit and watch for 45 seconds to get +10% kill XP for half an hour.", 'they show old films at the Chinese theatre sometimes and you get a buff');
  H('hive surges', ['surge', 'invasion', 'event'], 3, 0.5, 'Hive surge: every 30-50 minutes the bugs attack a hub in waves for ten minutes. Three or more kills earns a reward; if the hub is overrun its merchants close for 15 minutes.', 'sometimes a swarm of bugs attacks the town');
  H('money', ['gold', 'cash', 'dollars', 'cents'], 1, 0.2, 'Money is dollars and cents. Early income: sell grey junk to vendors, greens on the auction house, do quests. Repairs and abilities cost money, so do not buy vendor gear early.', 'money is dollars and cents, quests give some');
  H('the game itself', ['hollywoodland', 'this game', 'the dev', 'engine', 'unreal'], 1, 0.3, 'Hollywoodland is a jam entry: an Unreal Engine 5 game running in the browser (WebAssembly and WebGPU), single-player with simulated players, one built zone (Hollywood Boulevard, levels 1-12) and one dungeon; the other twelve zones on the map are a facade. Saves live in the browser.', 'it is an Unreal game that runs in the browser, made for the jam');
  H('levelling', ['level', 'xp', 'leveling', 'levelling', 'grind'], 1, 0.2, 'Levels 1-6 take roughly 40 minutes: do Sid Grauman\'s quest chain in the forecourt first, train at your class trainer every 2 levels, group up for the faster kills. The zone runs out of quests around level 12.', 'do the quests from the guy at the start to level');

  // ---- items: compact (name, slot, type, level, quality, vendor price). Quest and boss items get a little more.
  const QN = ['grey', 'white', 'green', 'blue', 'purple'];
  const its = items.map((it) => [it.name, it.slot || it.kind, it.type || '', it.req || 1, it.q ?? 1, it.price || 0]);
  return { entries: out, items: its, quality: QN };
}
