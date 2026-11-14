// Latency of the sim-player model calls, per provider, through the Worker's own provider layer
// (workers/server/src/providers.js) from this machine. Keys come from .env and are never printed.
//   node tools/ai-bench.mjs [n]
import { readFileSync } from 'node:fs';
import { decide, write, hard } from '../workers/server/src/providers.js';

const N = Number(process.argv[2] || 10);
export const env = Object.fromEntries(readFileSync(new URL('../.env', import.meta.url), 'utf8').split(/\r?\n/).filter((l) => l.includes('='))
  .map((l) => [l.slice(0, l.indexOf('=')).trim(), l.slice(l.indexOf('=') + 1).trim().replace(/^["']|["']$/g, '')]));
if (import.meta.url.endsWith(process.argv[1].replace(/\\/g, '/').split('/').pop())) {
  const state = 'Hollywoodland is a browser MMORPG made for a game jam. Brickwall (level 9 veteran, helpful, busy questing) is another player. ' +
    'The player Tester (level 6 nurse) says nearby: "hey can you help me with the drones quest?"';
  const questions = [{ id: 'respond', type: 'choice', text: 'Does Brickwall type a reply?', choices: { reply: 'answers in chat', ignore: 'stays silent' } },
    { id: 'liking', type: 'score', text: "How does this change Brickwall's opinion of the player?", levels: ['annoyed', 'slightly put off', 'no change', 'a little warmer', 'likes them more'] }];
  const system = 'You write one chat line for a player of an online game. Return JSON {"text": "..."} with a casual reply of at most 90 characters, lowercase, no emoji.';
  const user = state;
  const plan = 'Brickwall has finished a kill quest. Options: o1 sell and train in town (150 s), o2 help the player with their quest (240 s), o3 join a dungeon group forming now (25 min, slightly low level). ' +
    'He is helpful but short on time tonight. Return JSON {"choice":"o1|o2|o3","why":"<12 words>"}.';
  const R = {};
  const time = async (k, fn) => { try { const o = await fn(); (R[k] ||= []).push(o.ms); } catch (e) { (R[k + ' ERR'] ||= []).push(0); if ((R[k + ' ERR']).length === 1) console.log(k, String(e).slice(0, 200)); } };
  for (let i = 0; i < N + 1; i++) {
    await time('decide fireworks rune', () => decide(env, state, questions, { provider: 'fireworks' }));
    await time('decide openai decisions', () => decide(env, state, questions, { provider: 'openai' }));
    await time('write fireworks deepseek', () => write(env, { system, user, max: 120 }, { provider: 'fireworks' }));
    await time('write openai terra', () => write(env, { system, user, max: 120 }, { provider: 'openai', timeout: 20000 }));
    await time('hard openai sol (low)', () => hard(env, { system: 'You judge options for a game character. Answer with JSON only.', user: plan, max: 200 }, { provider: 'openai', timeout: 40000 }));
    await time('hard fireworks deepseek (reasoning)', () => hard(env, { system: 'You judge options for a game character. Answer with JSON only.', user: plan, max: 200 }, { provider: 'fireworks', timeout: 40000 }));
    if (i === 0) for (const k of Object.keys(R)) R[k] = []; // warm-up
    process.stdout.write('.');
  }
  const stats = (xs) => { const s = [...xs].sort((a, b) => a - b); const q = (p) => s[Math.min(s.length - 1, Math.floor(p * s.length))]; return s.length ? `n=${s.length} p50 ${q(0.5)} p90 ${q(0.9)} max ${s[s.length - 1]}` : 'n=0'; };
  console.log('\n(ms)');
  for (const [k, v] of Object.entries(R)) console.log(k.padEnd(38), k.endsWith('ERR') ? `${v.length} errors` : stats(v));
}
