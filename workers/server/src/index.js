// hollywoodland-server: AI + persistence API for the Hollywoodland web client.
//   POST /v1/session            -> {token, player, expires}           (HMAC token, 12 h)
//   POST /v1/sim                -> DESIGN-BIBLE §13 protocol: kind = reply | decide | gossip
//   GET|PUT /v1/save/<player>   -> cloud copy of the save game (KV)
//   GET  /v1/health
// Models (providers.js): Fireworks Rune makes the accept/decline/respond decisions and DeepSeek flash writes chat lines;
// OpenAI (Decisions API, gpt-6.1-terra) answers a call when Fireworks is slow or down; gpt-6.1-sol does the hard jobs. The client stays playable when this
// Worker fails: every failure returns {fallback:true} and the client uses its local corpus.
import { SYSTEM_PROMPT, buildStateDoc, buildUserPrompt, PLAN_SYSTEM, buildPlanPrompt, MEMORY_SYSTEM, buildMemoryPrompt } from './prompt.js';
import { decide, write, hard, sample, providerState } from './providers.js';
import { attach as attachLore, toolsFor } from './lore.js';

const LIMIT_PER_MIN = 60, LIMIT_PER_HOUR = 2000, DAILY_BUDGET = 1000000; // (no budget cap wanted; the windows only stop runaway clients)
const ORIGIN_OK = /^https:\/\/([a-z0-9-]+\.)*(itch\.zone|itch\.io|coeurnix\.workers\.dev)$|^https?:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/;

const rate = new Map(); // per-isolate fallback when the Durable Object is unavailable: session -> [timestamps]
let budget = { day: '', n: 0 };

// Limiter: one Durable Object holds the per-session windows and the daily budget for every isolate, so the limits hold
// across Cloudflare's edge rather than per isolate. The budget survives restarts in DO storage.
export class Limiter {
  constructor(state) { this.state = state; this.rate = new Map(); this.budget = null; }
  async fetch(request) {
    const { session } = await request.json();
    const now = Date.now();
    const day = new Date().toISOString().slice(0, 10);
    if (!this.budget) this.budget = (await this.state.storage.get('budget')) || { day, n: 0 };
    if (this.budget.day !== day) this.budget = { day, n: 0 };
    const list = (this.rate.get(session) || []).filter((t) => now - t < 3600e3);
    if (list.filter((t) => now - t < 60e3).length >= LIMIT_PER_MIN || list.length >= LIMIT_PER_HOUR) return Response.json({ limited: 'rate' });
    if (this.budget.n >= DAILY_BUDGET) return Response.json({ limited: 'budget' });
    list.push(now); this.rate.set(session, list);
    if (this.rate.size > 20000) for (const [k, v] of this.rate) if (!v.length || now - v[v.length - 1] > 3600e3) this.rate.delete(k);
    this.budget.n++;
    if (this.budget.n % 10 === 1) await this.state.storage.put('budget', this.budget);
    return Response.json({ limited: false, n: this.budget.n });
  }
}
async function limitedDO(env, session) {
  if (!env.LIMITER) return limited(session);
  try {
    const stub = env.LIMITER.get(env.LIMITER.idFromName('global'));
    const r = await stub.fetch('https://limiter/check', { method: 'POST', body: JSON.stringify({ session }) });
    return !!(await r.json()).limited;
  } catch (e) { console.log('limiter failed', String(e)); return limited(session); }
}

function cors(request) {
  const origin = request.headers.get('Origin') || '';
  const h = {
    'Access-Control-Allow-Origin': ORIGIN_OK.test(origin) ? origin : 'https://hollywoodland.coeurnix.workers.dev',
    'Access-Control-Allow-Methods': 'GET,POST,PUT,OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
    'Access-Control-Max-Age': '7200',
    'Cross-Origin-Resource-Policy': 'cross-origin',
    'Vary': 'Origin',
    'Cache-Control': 'no-store',
  };
  return h;
}
const json = (request, obj, status = 200) =>
  new Response(JSON.stringify(obj), { status, headers: { 'Content-Type': 'application/json', ...cors(request) } });

async function hmac(env, text) {
  const key = await crypto.subtle.importKey('raw', new TextEncoder().encode(env.SESSION_SECRET || 'dev-secret'),
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  const sig = await crypto.subtle.sign('HMAC', key, new TextEncoder().encode(text));
  return [...new Uint8Array(sig)].slice(0, 12).map((b) => b.toString(16).padStart(2, '0')).join('');
}
async function makeToken(env, player) {
  const exp = Date.now() + 12 * 3600 * 1000;
  return { token: `${player}.${exp}.${await hmac(env, `${player}.${exp}`)}`, player, expires: exp };
}
async function checkToken(env, token) {
  const [player, exp, sig] = String(token || '').split('.');
  if (!player || !exp || Number(exp) < Date.now()) return null;
  return (await hmac(env, `${player}.${exp}`)) === sig ? player : null;
}
function limited(session) {
  const now = Date.now();
  const list = (rate.get(session) || []).filter((t) => now - t < 3600e3);
  if (list.filter((t) => now - t < 60e3).length >= LIMIT_PER_MIN || list.length >= LIMIT_PER_HOUR) return true;
  list.push(now);
  rate.set(session, list);
  if (rate.size > 5000) rate.clear();
  const day = new Date().toISOString().slice(0, 10);
  if (budget.day !== day) budget = { day, n: 0 };
  return ++budget.n > DAILY_BUDGET;
}

// Decisions (Rune, or the OpenAI Decisions API as fallback): one pass over a state document, typed questions ->
// probabilities. Choices are sampled instead of taking the argmax so that identical situations do not always resolve the
// same way.
async function runeDecide(env, req, meta) {
  const questions = [];
  const trig = req.trigger || {};
  for (const s of req.speakers) {
    const who = s.handle;
    if (req.kind === 'observe') {
      // someone watching the player do something: say something, just react with the body, or pay no attention
      questions.push({ id: `${s.persona}|react`, type: 'choice', text: `${who} can see what the player is doing. Given ${who}'s personality, mood and how busy they are, what does ${who} do?`,
        choices: { comment: `${who} says something out loud about it`, emote: `${who} reacts without words (looks, laughs, waves, copies it)`, ignore: `${who} pays no attention or deliberately ignores it` } });
      questions.push({ id: `${s.persona}|register`, type: 'choice', text: `If ${who} reacted, in what spirit?`,
        choices: { amused: 'finds it funny', joins: 'joins in or plays along', puzzled: 'confused, asks what they are doing', annoyed: 'irritated or dismissive', kind: 'friendly or encouraging', teasing: 'teases them' } });
    } else if (trig.kind === 'invite' || trig.kind === 'trade' || trig.kind === 'duel') {
      questions.push({ id: `${s.persona}|decision`, type: 'choice', text: `How does ${who} answer the player's ${trig.kind} request?`,
        choices: { accept: `${who} accepts`, decline: `${who} declines (busy, wrong level, not interested, about to log off)` } });
    } else if (trig.kind === 'greet' || trig.kind === 'event') {
      // (they speak: the game already chose them)
    } else if (trig.channel !== 'whisper' && !(trig.channel === 'party' && req.speakers.length === 1)) {
      // A whisper is always answered; in a party not everyone answers every line; open channels may be ignored.
      questions.push({ id: `${s.persona}|respond`, type: 'choice', text: `Does ${who} type a reply to this message?`,
        choices: { reply: `${who} answers in chat`, ignore: `${who} stays silent (afk, busy, not addressed, not interested)` } });
    }
    questions.push({ id: `${s.persona}|liking`, type: 'score', text: `How does this interaction change ${who}'s opinion of the player?`,
      levels: ['annoyed by the player', 'slightly put off', 'no change', 'a little warmer', 'likes the player more'] });
  }
  const out = await decide(env, buildStateDoc(req), questions, { provider: req._provider });
  meta.decide = { provider: out.provider, ms: out.ms };
  const decisions = {};
  for (const [qid, a] of Object.entries(out.answers)) {
    const [persona, q] = qid.split('|');
    const d = (decisions[persona] ||= {});
    if (a.type === 'choice') d[q] = sample(a);
    else if (q === 'liking') d.liking_delta = Math.max(-2, Math.min(2, Math.round(a.score) - 2));
  }
  return decisions;
}

const BAD = /\b(n[i1]gg|f[a@]gg|retard|rape|kike|sp[i1]c\b|tranny|cunt)/i;
function cleanText(t) {
  t = String(t || '').replace(/[\r\n]+/g, ' ').trim();
  if (!t || BAD.test(t)) return null;
  return t.length > 140 ? t.slice(0, 137) + '...' : t;
}
// Role guard (DESIGN-BIBLE §11 "Honesty"): disclosure is the game's job (title screen, first system line, the
// "show simulated players" toggle). In chat the characters stay in role, so assistant-style fourth-wall breaks are
// replaced with a light in-character deflection.
const BREAKS_ROLE = /\b(as an ai|an ai (model|assistant)|language model|ai[- ]simulated|simulated (player|character)|i'?m (just )?(an? )?(ai|npc|bot|program)\b|i am (just )?(an? )?(ai|npc|bot|program)\b|this is a single[- ]player game)/i;
const DEFLECT = ['lol what', 'beep boop. no, im just slow', 'i wish, id level faster', 'rude', 'lol ok', 'sure buddy', 'haha no'];
const ASSISTANT_VOICE = /\b(as an ai\b|language model|i (cannot|can't) (assist|help) with|i'?m an ai assistant)/i;
function stayInRole(actions, speakers) {
  for (const a of actions) {
    if (!a.chat) continue;
    // a persona whose stance is "admit" may say so; nobody talks like an assistant
    const stance = speakers && speakers.get(a.speaker)?.ai_stance;
    if (ASSISTANT_VOICE.test(a.chat.text) || (stance !== 'admit' && BREAKS_ROLE.test(a.chat.text))) a.chat.text = DEFLECT[Math.floor(Math.random() * DEFLECT.length)];
  }
  return actions;
}

function validate(req, raw, decisions) {
  const speakers = new Map(req.speakers.map((s) => [s.persona, s]));
  const allowedGoals = new Set((req.allowed && req.allowed.goals) || []);
  const allowedBiases = new Set((req.allowed && req.allowed.biases) || []);
  const channels = new Set(['say', 'party', 'whisper', 'zone', 'trade', 'lfg', 'guild', 'yell']);
  const out = [];
  for (const a of Array.isArray(raw && raw.actions) ? raw.actions.slice(0, 4) : []) {
    if (!a) continue;
    // (models sometimes answer with the handle, or mangle the id: map it back when it is unambiguous)
    if (!speakers.has(a.speaker)) { const byHandle = req.speakers.find((x) => x.handle === a.speaker); a.speaker = byHandle ? byHandle.persona : req.speakers.length === 1 ? req.speakers[0].persona : a.speaker; }
    if (!speakers.has(a.speaker) || out.find((o) => o.speaker === a.speaker)) continue;
    const d = decisions[a.speaker] || {};
    const act = { speaker: a.speaker, delay_ms: Math.max(0, Math.min(15000, a.delay_ms | 0)), chat: null, emote: null, goal: null,
      bias: null, memory_add: [], liking_delta: d.liking_delta ?? Math.max(-2, Math.min(2, a.liking_delta | 0)),
      decision: d.decision || null, react: d.react || null, register: d.register || null };
    if (a.chat && d.respond !== 'ignore' && !(d.react && d.react !== 'comment')) {
      const text = cleanText(a.chat.text);
      if (text) act.chat = { channel: channels.has(a.chat.channel) ? a.chat.channel : (req.trigger?.channel || 'say'), text };
    }
    if (typeof a.emote === 'string' && /^[a-z]{2,12}$/.test(a.emote)) act.emote = a.emote;
    if (a.goal && allowedGoals.has(a.goal.type)) act.goal = { type: a.goal.type, arg: String(a.goal.arg ?? '').slice(0, 60) };
    if (a.bias && allowedBiases.has(a.bias.type)) act.bias = { type: a.bias.type, actor: String(a.bias.actor ?? 'player').slice(0, 40),
      weight: Math.max(0, Math.min(5, Number(a.bias.weight) || 1)), ttl_s: Math.max(1, Math.min(120, Number(a.bias.ttl_s) || 20)) };
    if (Array.isArray(a.memory_add)) act.memory_add = a.memory_add.slice(0, 2).map((m) => String(m).slice(0, 100));
    out.push(act);
  }
  // Every speaker gets an action so that decisions always reach the client.
  for (const s of req.speakers) {
    if (!out.find((a) => a.speaker === s.persona)) {
      const d = decisions[s.persona] || {};
      out.push({ speaker: s.persona, delay_ms: 800, chat: null, emote: null, goal: null, bias: null, memory_add: [],
        liking_delta: d.liking_delta ?? 0, decision: d.decision || null, react: d.react || null, register: d.register || null });
    }
  }
  return stayInRole(out, speakers);
}

async function sim(request, env) {
  let req;
  try { req = JSON.parse(await request.text()); } catch { return json(request, { error: 'bad json' }, 400); }
  const player = await checkToken(env, req.session);
  if (!player) return json(request, { error: 'session', fallback: true }, 401);
  if (await limitedDO(env, req.session)) return json(request, { error: 'rate', fallback: true }, 429);
  if (!Array.isArray(req.speakers) || !req.speakers.length || req.speakers.length > 4) return json(request, { error: 'speakers' }, 400);
  if (!['reply', 'decide', 'gossip', 'observe', 'interpret', 'plan', 'consolidate'].includes(req.kind)) return json(request, { error: 'kind' }, 400);
  req._now = Date.now(); req.v = req.v | 0;
  req._provider = req.provider && env.DEBUG_KEY && req.debug === env.DEBUG_KEY ? String(req.provider) : undefined;
  req.speakers = req.speakers.map((s) => ({ ...s, persona: String(s.persona), handle: String(s.handle || '?').slice(0, 24) }));

  if (req.kind === 'interpret') return interpret(request, env, req);
  if (req.kind === 'plan') return plan(request, env, req);
  if (req.kind === 'consolidate') return consolidate(request, env, req);
  attachLore(req);
  const t0 = Date.now();
  // the same census events within a 2-minute bucket (retries, the same events reaching several players) cost one LLM call
  let gossipKey = null;
  if (req.kind === 'gossip' && env.HL_KV && req.v < 2) {
    const ev = JSON.stringify((req.context && req.context.events) || []);
    const h = [...new Uint8Array(await crypto.subtle.digest('SHA-1', new TextEncoder().encode(ev)))].slice(0, 8).map((b) => b.toString(16).padStart(2, '0')).join('');
    gossipKey = `gossip:${h}:${req.speakers.length}:${Math.floor(Date.now() / 120e3)}`;
  }
  if (gossipKey) {
    try {
      const hit = await env.HL_KV.get(gossipKey, 'json');
      if (hit && Array.isArray(hit.lines)) {
        const actions = req.speakers.map((s, i) => ({ speaker: s.persona, delay_ms: 1500 + i * 2500, chat: hit.lines[i] ? { channel: hit.lines[i].channel, text: hit.lines[i].text } : null,
          emote: null, goal: null, bias: null, memory_add: [], liking_delta: 0, decision: null }));
        return json(request, { id: req.id, actions: stayInRole(actions), cached: true, ms: { rune: 0, llm: 0 } });
      }
    } catch (e) { console.log('gossip cache read failed', String(e)); }
  }
  let decisions = {};
  const meta = {};
  try {
    if (req.kind !== 'gossip' && !(req.trigger && req.trigger.kind === 'greet')) decisions = await runeDecide(env, req, meta);
  } catch (e) { console.log('decide failed', String(e)); }
  const tRune = Date.now() - t0;

  // Everyone ignores: no LLM call needed.
  if ((req.kind === 'reply' && req.speakers.every((s) => decisions[s.persona]?.respond === 'ignore')) ||
    (req.kind === 'observe' && req.speakers.every((s) => decisions[s.persona]?.react !== 'comment'))) {
    return json(request, { id: req.id, actions: validate(req, { actions: [] }, decisions), ms: { rune: tRune, llm: 0 }, via: meta });
  }
  try {
    const out = await write(env, { system: SYSTEM_PROMPT, user: buildUserPrompt(req, decisions) }, { provider: req._provider });
    meta.write = { provider: out.provider, ms: out.ms };
    const raw = out.json;
    const actions = validate(req, raw, decisions);
    if (gossipKey) {
      const lines = actions.map((a) => a.chat).filter(Boolean);
      if (lines.length) await env.HL_KV.put(gossipKey, JSON.stringify({ lines }), { expirationTtl: 600 }).catch((e) => console.log('gossip cache write failed', String(e)));
    }
    return json(request, { id: req.id, actions, ms: { rune: tRune, llm: Date.now() - t0 - tRune }, via: meta });
  } catch (e) {
    console.log('llm failed', String(e));
    // Decisions still stand; the client fills the line from its corpus.
    return json(request, { id: req.id, actions: validate(req, { actions: [] }, decisions), fallback: true });
  }
}

// ---- interpret: what did the player mean in party chat? Typed questions only (no writing model), so it is fast.
const INTENTS = { attack: 'tells the group to attack or focus a particular enemy', wait: 'asks the group to wait, stop or not pull yet', go: 'says they are ready, or tells the group to go or pull now',
  heal: 'asks for healing or says they are about to die', follow: 'asks the others to follow or come to them', rest: 'calls for a break to eat, drink or recover', run: 'tells the group to run away or retreat',
  loot: 'talks about loot or who should take an item', thanks: 'thanks or praises someone', sorry: 'apologises for a mistake', blame: 'complains about or blames someone',
  question: 'asks a question about the game or the plan', bye: 'says they are leaving', banter: 'small talk, a joke, or anything else' };
async function interpret(request, env, req) {
  const c = req.context || {}, text = String(req.trigger?.text || '').slice(0, 240);
  const members = (Array.isArray(c.members) ? c.members : []).slice(0, 5).map((m) => String(m).slice(0, 24));
  const targets = (Array.isArray(c.targets) ? c.targets : []).slice(0, 8).map((m) => String(m).slice(0, 40));
  const questions = [{ id: 'intent', type: 'choice', text: 'What is the player doing with this party chat line?', choices: INTENTS }];
  if (members.length) questions.push({ id: 'to', type: 'choice', text: 'Who is the line addressed to?', choices: { everyone: 'the whole group or nobody in particular', ...Object.fromEntries(members.map((m) => [m, `the party member ${m}`])) } });
  if (targets.length) questions.push({ id: 'target', type: 'choice', text: 'Which enemy does the line refer to, if any?', choices: { none: 'no particular enemy', ...Object.fromEntries(targets.map((m, i) => [`t${i}`, m])) } });
  const state = `A party of players is in the browser MMO Hollywoodland${c.in_combat ? ', in the middle of a fight' : ''}${c.hub ? ` at ${String(c.hub).slice(0, 40)}` : ''}. Party members: ${members.join(', ') || 'unknown'}. ` +
    `${targets.length ? `Enemies in view: ${targets.join(', ')}. ` : ''}The player types in party chat: "${text.replace(/"/g, "'")}"`;
  try {
    const out = await decide(env, state, questions, { provider: req._provider });
    const top = (a) => a ? Object.entries(a.probabilities).sort((x, y) => y[1] - x[1])[0] : null;
    const tt = top(out.answers.target);
    return json(request, { id: req.id, intent: out.answers.intent?.probabilities || {}, to: top(out.answers.to)?.[0] || 'everyone',
      target: tt && tt[0] !== 'none' && tt[1] > 0.4 ? targets[Number(tt[0].slice(1))] : null, via: { decide: { provider: out.provider, ms: out.ms } } });
  } catch (e) { console.log('interpret failed', String(e)); return json(request, { id: req.id, fallback: true }); }
}

// ---- plan: the careful model picks between options the game computed
async function plan(request, env, req) {
  const opts = Array.isArray(req.report?.options) ? req.report.options : [];
  const ids = new Set(opts.map((o) => String(o.id)));
  if (!ids.size) return json(request, { error: 'options' }, 400);
  try {
    const { tools, callTool } = toolsFor(req.speakers[0], req);
    const out = await hard(env, { system: PLAN_SYSTEM, user: buildPlanPrompt(req), tools, callTool, rounds: 2, max: 300 }, { provider: req._provider });
    const j = out.json || {};
    if (!ids.has(String(j.choice))) return json(request, { id: req.id, fallback: true, via: { hard: { provider: out.provider, ms: out.ms } } });
    const say = cleanText(j.say);
    return json(request, { id: req.id, choice: String(j.choice), then: ids.has(String(j.then)) && j.then !== j.choice ? String(j.then) : null, why: String(j.why || '').slice(0, 120),
      say: say && !(req.speakers[0].ai_stance !== 'admit' && BREAKS_ROLE.test(say)) ? say : null, mood: String(j.mood || '').slice(0, 30), via: { hard: { provider: out.provider, ms: out.ms, tool_calls: out.tool_calls } } });
  } catch (e) { console.log('plan failed', String(e)); return json(request, { id: req.id, fallback: true }); }
}

// ---- consolidate: rewrite one persona's memory of the player
async function consolidate(request, env, req) {
  try {
    const out = await hard(env, { system: MEMORY_SYSTEM, user: buildMemoryPrompt(req), max: 350 }, { provider: req._provider });
    const j = out.json || {}, n = (req.memory?.episodes || []).length;
    return json(request, { id: req.id, gist: String(j.gist || '').replace(/[\r\n]+/g, ' ').slice(0, 320), keep: (Array.isArray(j.keep) ? j.keep : []).map((k) => k | 0).filter((k) => k >= 0 && k < n).slice(0, 6),
      promise: j.promise ? String(j.promise).slice(0, 120) : null, nickname: j.nickname ? String(j.nickname).slice(0, 24) : null, via: { hard: { provider: out.provider, ms: out.ms } } });
  } catch (e) { console.log('consolidate failed', String(e)); return json(request, { id: req.id, fallback: true }); }
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: cors(request) });
    try {
      if (url.pathname === '/' || url.pathname === '/v1/health') return json(request, { ok: true, service: 'hollywoodland-server', t: Date.now(), primary: env.AI_PRIMARY || 'fireworks', hard: env.AI_HARD || 'openai', providers: providerState() });
      if (url.pathname === '/v1/session' && request.method === 'POST') {
        let body = {};
        try { body = JSON.parse(await request.text() || '{}'); } catch {}
        const player = /^[a-z0-9_]{8,40}$/.test(body.player || '') ? body.player : 'p_' + crypto.randomUUID().replace(/-/g, '').slice(0, 20);
        return json(request, await makeToken(env, player));
      }
      if (url.pathname === '/v1/sim' && request.method === 'POST') return sim(request, env);
      const m = /^\/v1\/save\/([a-z0-9_]{8,40})$/.exec(url.pathname);
      if (m) {
        if (request.method === 'GET') {
          const v = await env.HL_KV.get('save:' + m[1]);
          return v === null ? json(request, { error: 'none' }, 404) : new Response(v, { headers: { 'Content-Type': 'application/json', ...cors(request) } });
        }
        if (request.method === 'PUT' || request.method === 'POST') {
          const token = url.searchParams.get('t');
          if ((await checkToken(env, token)) !== m[1]) return json(request, { error: 'session' }, 401);
          const body = await request.text();
          if (body.length > 400000) return json(request, { error: 'too large' }, 413);
          await env.HL_KV.put('save:' + m[1], body, { expirationTtl: 90 * 86400 });
          return json(request, { ok: true });
        }
      }
      return json(request, { error: 'not found' }, 404);
    } catch (e) {
      console.log('error', String(e && e.stack || e));
      return json(request, { error: 'server', fallback: true }, 500);
    }
  },
};
