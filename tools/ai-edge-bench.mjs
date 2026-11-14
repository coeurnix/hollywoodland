// Latency of the Worker's model calls measured at Cloudflare's edge (the Worker's own timers), per provider.
//   node tools/ai-edge-bench.mjs [n] [base]     needs HL_DEBUG_KEY in .env (the Worker's DEBUG_KEY secret)
import { env } from './ai-bench.mjs';
const N = Number(process.argv[2] || 8), base = process.argv[3] || 'https://hollywoodland-server.coeurnix.workers.dev';
const sess = await (await fetch(base + '/v1/session', { method: 'POST', body: '{}' })).json();
const texts = ['hey can you help me with the drones quest?', 'anyone know where the bank is', 'lol nice hat', 'what level for the dungeon', 'how do i get a mount', 'is this really a jam game'];
const R = {};
for (let i = 0; i < N; i++) for (const provider of ['fireworks', 'openai']) {
  const body = { session: sess.token, id: i, kind: 'reply', provider, debug: env.HL_DEBUG_KEY,
    speakers: [{ persona: 'persona_0142', handle: 'Brickwall', class: 'veteran', level: 9, temperament: 'helpful', typing: 'all lowercase, no punctuation', activity: 'questing' }],
    trigger: { kind: 'chat', channel: 'say', from: 'player', text: texts[i % texts.length] },
    context: { player: { name: 'Tester', level: 6, class: 'Nurse' }, zone: 'Hollywood Boulevard', recent_chat: [], world: [] }, allowed: { goals: [], biases: [], ids: [] } };
  const j = await (await fetch(base + '/v1/sim', { method: 'POST', body: JSON.stringify(body) })).json();
  for (const k of ['decide', 'write']) if (j.via?.[k]) (R[`${k} ${j.via[k].provider}${j.via[k].provider !== provider ? ' (fell back)' : ''}`] ||= []).push(j.via[k].ms);
  if (!j.via) console.log(JSON.stringify(j).slice(0, 200));
  await new Promise((r) => setTimeout(r, 1600));
}
const stats = (xs) => { const s = [...xs].sort((a, b) => a - b); const q = (p) => s[Math.min(s.length - 1, Math.floor(p * s.length))]; return `n=${s.length} p50 ${q(0.5)} p90 ${q(0.9)} max ${s[s.length - 1]}`; };
for (const [k, v] of Object.entries(R)) console.log(k.padEnd(28), stats(v));
