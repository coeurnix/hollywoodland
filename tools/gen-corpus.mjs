// Generates extra chat corpus lines with the LLM (build-time only) -> tools/cache/corpus.json, merged by gen-data.mjs.
// usage: FIREWORKS_API_KEY=... node tools/gen-corpus.mjs     (never commit the key; the cache file IS committed)
import { writeFileSync, mkdirSync, existsSync, readFileSync } from 'node:fs';
import { zones, dungeons } from './gen/world.mjs';
import { quests } from './gen/quests.mjs';
import { classes } from './gen/classes.mjs';
import { ARCHETYPES } from './gen/archetypes.mjs';

let KEY = process.env.FIREWORKS_API_KEY;
if (!KEY) try { KEY = (readFileSync(new URL('../.env', import.meta.url), 'utf8').match(/^FIREWORKS_API_KEY=(.*)$/m) || [])[1]?.trim().replace(/^["']|["']$/g, ''); } catch {}
if (!KEY) { console.error('set FIREWORKS_API_KEY (env or .env)'); process.exit(2); }
const MODEL = 'accounts/fireworks/models/deepseek-v4p1-flash-1017';
const facts = `Game: "Hollywoodland", a browser MMORPG set in Hollywood 1930 under a dome, occupied by giant insect invaders. It is an ENTRY IN A GAME JAM (the 2026 Epic AI MegaJam on itch.io, Unreal Engine only, theme "Director's Cut", Nov 12-15 2026) and is only days old; the people playing it are mostly jam entrants taking a break from their own unfinished games, nearly all new to it and low level. Only one zone and one dungeon are really built; a handful of players are improbably high level already.
Capital and starting zone: Hollywood Boulevard (levels 1-12). Places there: Grauman's Chinese forecourt (trainers, Sid Grauman), Roosevelt Hotel (inn), First National Bank at Hollywood & Highland, Taft Building auction house at Hollywood & Vine, Brown Derby, Pantages, police station, Franklin slopes, Selma alleys, Hollywood High, Hollywood Hotel ballroom, the hive wall at every road out (elite guards level 22-34).
Mobs there: Invader Worker, Invader Drone, Invader Spitter, Invader Broodling, Invader Soldier, Feral Dog, Coyote, Looter; rares Old Scar and The Selma Avenue Burrower; elite Ballroom Brute.
Quests there: ${quests.slice(0, 26).map((q) => q.name).join(', ')}.
Dungeon there: The Egyptian Theatre (5 players, 8-12): bosses Brood Keeper, Hive Warden, Spitter Matriarch, The Egyptian Hive Mother.
Other zones (name, levels): ${zones.slice(1).map((z) => `${z.name} ${z.level_min}-${z.level_max}`).join('; ')}.
Other dungeons/raids: ${dungeons.slice(1).map((d) => `${d.name} ${d.level_min}-${d.level_max}`).join('; ')}.
Classes: ${classes.map((c) => c.name).join(', ')}. Currency: dollars and cents. Travel: Pacific Electric red cars. Mounts at 40 and 60 (horses, Model A, Duesenberg).`;

const jobs = [];
// idle talk in each archetype's voice (the client matches a corpus line's tag against temperament or archetype)
for (const a of ARCHETYPES) jobs.push({ s: 'idle', t: a.id, n: 26, ask: `General-chat lines typed by this kind of player while levelling or standing around in the capital. The player: ${a.name}. ${a.talk} About a third of the lines are about the game being a jam entry or about their own jam situation; the rest about what they are doing in the game` });
for (const [s, ask, n] of [
  ['death', 'things players say right after dying to mobs', 25], ['wipe', 'party chat after the whole group dies on a dungeon boss in the Egyptian Theatre', 30],
  ['boss_kill', 'party chat right after killing a dungeon boss', 25], ['greet', 'greetings when logging in or meeting someone', 20], ['bye', 'goodbyes when logging off (several mention getting back to their jam entry or the deadline)', 24],
  ['decline_busy', 'short whispers declining a group invite because they are busy', 25], ['decline_low', 'short whispers declining to help a much lower level player with high-level content', 20],
  ['party_join', 'first line after joining a party', 20], ['party_leave', 'last line before leaving a party', 20], ['train', 'panicked lines while dragging a pack of mobs into town', 15],
  ['whisper_fallback', 'very short non-committal whisper replies from a busy player (1-4 words)', 30], ['loot_win', 'reaction to winning a loot roll', 15], ['loot_lose', 'reaction to losing a loot roll', 15],
  ['ding', 'what players say when they level up', 12], ['gz', 'congratulating someone on a level', 12], ['mob_tag', 'reaction when someone else tags the mob you wanted', 12],
  ['jam', 'General-chat lines about this game being a jam entry: disbelief, guesses at how it was made, rough edges they noticed, their own entry (never named), the deadline, wondering who here is real', 60],
  ['gripe', 'short annoyed party-chat lines when a group member does something careless (pulls early, ignores a request to wait)', 20],
  ['party_lost', 'short party-chat lines from someone who has lost sight of the group leader', 14], ['party_pull', 'very short party-chat lines announcing they are pulling an enemy', 14],
  ['obs_react', 'very short spoken reactions (1-4 words) to a stranger doing something odd next to you, like dancing or staring', 20],
]) jobs.push({ s, t: 'any', n, ask });

async function gen(job) {
  const r = await fetch('https://api.fireworks.ai/inference/v1/chat/completions', {
    method: 'POST', headers: { Authorization: `Bearer ${KEY}`, 'Content-Type': 'application/json' },
    body: JSON.stringify({ model: MODEL, temperature: 1.0, max_tokens: 3000, reasoning_effort: 'none', response_format: { type: 'json_object' },
      messages: [{ role: 'system', content: `${facts}\n\nYou write chat lines typed by PLAYERS of this game. They type like modern MMO players: short, casual, mostly lowercase, shorthand (lfg, ty, oom, gz, afk). Rules: max 90 characters; no emoji; mention only game names listed above (real-world talk such as the jam, their own unnamed entry, deadlines, sleep, coffee is fine); no slurs, no sexual content; no player names; every line different in wording and topic. Return JSON {"lines":["...", ...]}.` },
        { role: 'user', content: `Write ${job.n} ${job.ask}.` }] }),
  });
  if (!r.ok) throw new Error(`${r.status} ${await r.text()}`);
  const j = await r.json();
  const lines = JSON.parse(j.choices[0].message.content).lines || [];
  return lines.map((x) => String(x).trim()).filter((x) => x.length > 0 && x.length <= 100 && !/[{}\[\]]/.test(x)).map((text) => ({ s: job.s, t: job.t, text }));
}

const out = [];
for (const job of jobs) {
  try { const lines = await gen(job); out.push(...lines); console.log(job.s, job.t, lines.length); }
  catch (e) { console.error('failed', job.s, job.t, String(e).slice(0, 200)); }
}
const seen = new Set();
const uniq = out.filter((l) => { const k = l.text.toLowerCase(); if (seen.has(k)) return false; seen.add(k); return true; });
mkdirSync(new URL('./cache/', import.meta.url), { recursive: true });
writeFileSync(new URL('./cache/corpus.json', import.meta.url), JSON.stringify(uniq, null, 0));
console.log(`wrote ${uniq.length} lines`);
