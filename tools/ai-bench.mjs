// Latency comparison for the sim-player models. Fireworks (Rune + DeepSeek flash) is measured through the deployed
// Worker (its own edge-side timings in `ms`, plus the round trip from here); OpenAI (Decisions API on gpt-6-luna, chat on
// gpt-6.1-terra) is called from this machine with the same prompts. Keys are read from .env and never printed.
//   node tools/ai-bench.mjs [n]
import { readFileSync } from 'node:fs';
import { SYSTEM_PROMPT, buildStateDoc, buildUserPrompt } from '../workers/server/src/prompt.js';

const N = Number(process.argv[2] || 12);
const env = Object.fromEntries(readFileSync(new URL('../.env', import.meta.url), 'utf8').split(/\r?\n/).filter((l) => l.includes('=')).map((l) => [l.slice(0, l.indexOf('=')).trim(), l.slice(l.indexOf('=') + 1).trim().replace(/^["']|["']$/g, '')]));
const OPENAI = env.OPENAI_API_KEY;
if (!OPENAI) { console.error('no OPENAI_API_KEY in .env'); process.exit(2); }
const WORKER = 'https://hollywoodland-server.coeurnix.workers.dev';

const TEXTS = ['hey can you help me with the drones quest?', 'anyone know where the bank is', 'want to group for egyptian later?', 'lol nice hat', 'are you a real person or a bot',
  'need heals!!', 'how do i get a mount', 'this game was made for a jam??', 'what level for the dungeon', 'gz on the level', 'selling scrap metal, want some?', 'follow me i know a good spot'];
const mkReq = (i) => ({
  id: i, kind: 'reply',
  speakers: [{ persona: 'persona_0142', handle: 'Brickwall', class: 'veteran', level: 9, temperament: 'helpful', typing: 'all lowercase, no punctuation, abbreviations like ty np omw',
    activity: 'questing on Hollywood Boulevard', guild: 'Gower Irregulars', memory: ['grouped with the player on Franklin', 'player gave them a hat'], liking: 2, recent: ['omw', 'need 2 more plates'] }],
  trigger: { kind: 'chat', channel: i % 2 ? 'say' : 'whisper', from: 'player', text: TEXTS[i % TEXTS.length] },
  context: { player: { name: 'Tester', level: 6, class: 'Nurse' }, zone: 'Hollywood Boulevard', hub: 'Chinese Forecourt', in_combat: false,
    recent_chat: ['[zone] Mabel: lfm egyptian need tank', '[trade] Dutch: wts [Stetson of the Boxer] 40c', '[zone] Fizz: where is the bank', '[zone] Opal: first national, hollywood and highland'],
    world: ['Hollywood Boulevard (levels 1-12) is the capital: bank at Hollywood & Highland, auction house in the Taft Building at Vine, class trainers at Grauman\'s Chinese forecourt.',
      'The Egyptian Theatre is a 5-player dungeon for levels 8-12 on Hollywood Boulevard; bosses: Brood Keeper, Hive Warden, Spitter Matriarch, The Egyptian Hive Mother.',
      'Every road out of Hollywood Boulevard is blocked by a hive wall whose gap is guarded by elites of level 22-34.'],
    npcs: ['Sid Grauman (Showman)', 'Earl Tanner (Gunsmith)'], events: [] },
  allowed: { goals: ['GROUP', 'FOLLOW', 'IDLE'], biases: ['heal_priority'], ids: ['Hollywood Boulevard', 'The Egyptian Theatre', 'Grauman\'s Chinese', 'Gower Irregulars'] },
});

const stats = (xs) => { const s = [...xs].sort((a, b) => a - b); const q = (p) => s[Math.min(s.length - 1, Math.floor(p * s.length))]; return s.length ? `n=${s.length} min ${s[0]} p50 ${q(0.5)} p90 ${q(0.9)} max ${s[s.length - 1]} mean ${Math.round(s.reduce((a, b) => a + b, 0) / s.length)}` : 'n=0'; };
const R = { fw_rune_edge: [], fw_llm_edge: [], fw_roundtrip: [], oa_decisions: [], oa_terra_none: [], oa_terra_low: [], oa_luna_none: [] };
const errs = {};
const note = (k, e) => { errs[k] = (errs[k] || 0) + 1; if (errs[k] <= 2) console.log(`  ${k} error: ${String(e).slice(0, 260)}`); };
const samples = {};

async function openai(path, body) {
  const t0 = Date.now();
  const r = await fetch('https://api.openai.com/v1' + path, { method: 'POST', headers: { Authorization: `Bearer ${OPENAI}`, 'Content-Type': 'application/json' }, body: JSON.stringify(body), signal: AbortSignal.timeout(20000) });
  const text = await r.text();
  if (!r.ok) throw new Error(`${r.status} ${text.slice(0, 220)}`);
  return { ms: Date.now() - t0, json: JSON.parse(text) };
}
const decisionsBody = (req) => {
  const who = req.speakers[0].handle;
  const questions = [];
  if (req.trigger.channel !== 'whisper') questions.push({ type: 'choice', name: 'respond', instructions: `Does ${who} type a reply to this message?`,
    choices: [{ value: 'reply', description: `${who} answers in chat` }, { value: 'ignore', description: `${who} stays silent (afk, busy, not addressed, not interested)` }] });
  questions.push({ type: 'score', name: 'liking', instructions: `How does this interaction change ${who}'s opinion of the player?`,
    levels: ['annoyed by the player', 'slightly put off', 'no change', 'a little warmer', 'likes the player more'].map((label) => ({ label })) });
  return { model: 'gpt-6-luna', input: buildStateDoc(req), questions };
};
const chatBody = (req, model, effort) => ({ model, reasoning_effort: effort, max_completion_tokens: effort === 'none' ? 400 : 1500, response_format: { type: 'json_object' },
  messages: [{ role: 'system', content: SYSTEM_PROMPT }, { role: 'user', content: buildUserPrompt(req, { persona_0142: { respond: 'reply' } }) }] });

const sess = await (await fetch(WORKER + '/v1/session', { method: 'POST', body: '{}' })).json();
for (let i = 0; i < N + 1; i++) {
  const req = mkReq(i), warm = i === 0;
  // Fireworks through the Worker
  try {
    const t0 = Date.now();
    const r = await fetch(WORKER + '/v1/sim', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: JSON.stringify({ ...req, session: sess.token }) });
    const j = await r.json(); const rt = Date.now() - t0;
    if (!r.ok || j.fallback || !j.ms) throw new Error(`${r.status} ${JSON.stringify(j).slice(0, 200)}`);
    if (!warm) { R.fw_roundtrip.push(rt); R.fw_rune_edge.push(j.ms.rune); if (j.ms.llm) R.fw_llm_edge.push(j.ms.llm); }
    samples.fireworks = j.actions?.[0]?.chat?.text || samples.fireworks;
  } catch (e) { note('fireworks', e); }
  try { const o = await openai('/decisions', decisionsBody(req)); if (!warm) R.oa_decisions.push(o.ms); samples.decisions = JSON.stringify(o.json.answers).slice(0, 300); } catch (e) { note('oa_decisions', e); }
  for (const [k, model, effort] of [['oa_terra_none', 'gpt-6.1-terra', 'none'], ['oa_terra_low', 'gpt-6.1-terra', 'low'], ['oa_luna_none', 'gpt-6-luna', 'none']]) {
    try { const o = await openai('/chat/completions', chatBody(req, model, effort)); if (!warm) R[k].push(o.ms); samples[k] = (o.json.choices?.[0]?.message?.content || '').slice(0, 260) + ` | usage ${JSON.stringify(o.json.usage?.completion_tokens)}`; } catch (e) { note(k, e); }
  }
  process.stdout.write('.');
  await new Promise((r) => setTimeout(r, 3200)); // stay under the Worker's 20/min session limit
}
console.log('\n(ms)');
for (const [k, v] of Object.entries(R)) console.log(k.padEnd(16), stats(v));
console.log('errors', JSON.stringify(errs));
for (const [k, v] of Object.entries(samples)) console.log(`sample ${k}: ${v}`);
