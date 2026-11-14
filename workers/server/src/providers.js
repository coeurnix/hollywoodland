// Model providers behind three calls, so the Worker can switch and fall back between them:
//   decide(env, state, questions)  typed questions -> probabilities   Fireworks Rune            | OpenAI Decisions (gpt-6-luna)
//   write(env, {system, user})     a JSON chat reply                  Fireworks DeepSeek flash  | OpenAI gpt-6.1-terra
//   hard(env, {system, user, tools, callTool})  judgment, planning, tool use   OpenAI gpt-6.1-sol | DeepSeek with reasoning
// Routing: env.AI_PRIMARY ("fireworks" default, or "openai") serves decide and write; env.AI_HARD ("openai" default, or
// "fireworks") serves hard. On a timeout or an error the other provider answers the same call, and a provider that fails
// several times in a minute is skipped for five (per isolate). Every result carries {provider, ms}.
const FW = 'https://api.fireworks.ai/inference/v1';
const OA = 'https://api.openai.com/v1';
export const MODELS = {
  fw_llm: 'accounts/fireworks/models/deepseek-v4p1-flash-1017', fw_rune: 'accounts/fireworks/models/rune',
  oa_decide: 'gpt-6-luna', oa_write: 'gpt-6.1-terra', oa_hard: 'gpt-6.1-sol',
};

const health = { fireworks: { fails: [], downUntil: 0 }, openai: { fails: [], downUntil: 0 } };
function mark(provider, ok) {
  const h = health[provider], now = Date.now();
  if (ok) { h.fails = []; return; }
  h.fails = h.fails.filter((t) => now - t < 60e3); h.fails.push(now);
  if (h.fails.length >= 4) { h.downUntil = now + 300e3; h.fails = []; console.log(`provider ${provider} skipped for 5 min`); }
}
const has = (env, p) => !!(p === 'openai' ? env.OPENAI_API_KEY : env.FIREWORKS_API_KEY);
function order(env, first) {
  const second = first === 'openai' ? 'fireworks' : 'openai';
  const list = [first, second].filter((p) => has(env, p));
  const up = list.filter((p) => Date.now() >= health[p].downUntil);
  return up.length ? up : list;
}
export const providerState = () => Object.fromEntries(Object.entries(health).map(([k, v]) => [k, Date.now() < v.downUntil ? 'skipped' : 'ok']));

async function post(url, key, body, timeoutMs) {
  const r = await fetch(url, { method: 'POST', headers: { Authorization: `Bearer ${key}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(body), signal: AbortSignal.timeout(timeoutMs) });
  if (!r.ok) throw new Error(`${new URL(url).host}${new URL(url).pathname} ${r.status}: ${(await r.text()).slice(0, 240)}`);
  return r.json();
}
async function route(env, first, what, fns, forced) {
  let last;
  const list = forced && has(env, forced) ? [forced] : order(env, first);
  if (!list.length) throw new Error('no model provider configured');
  for (const p of list) {
    const t0 = Date.now();
    try { const out = await fns[p](); mark(p, true); return { ...out, provider: p, ms: Date.now() - t0 }; }
    catch (e) { last = e; mark(p, false); console.log(`${what} via ${p} failed after ${Date.now() - t0} ms: ${String(e).slice(0, 300)}`); }
  }
  throw last;
}

// ---- decide. questions: [{id, type: 'choice'|'score', text, choices: {value: description} | levels: [labels low..high]}]
// -> answers {id: {type:'choice', probabilities:{value:p}} | {type:'score', score, probabilities:[p...]}}
export function decide(env, state, questions, opts = {}) {
  return route(env, env.AI_PRIMARY || 'fireworks', 'decide', {
    fireworks: async () => {
      const qs = {};
      for (const q of questions) qs[q.id] = q.type === 'choice' ? { type: 'choice', text: q.text, criteria: q.choices } : { type: 'score', text: q.text, criteria: q.levels };
      const out = await post(FW + '/systemone', env.FIREWORKS_API_KEY, { model: MODELS.fw_rune, state, questions: qs }, opts.timeout || 2500);
      const answers = {};
      for (const [id, a] of Object.entries(out.answers || {})) {
        if (a.type === 'choice') answers[id] = { type: 'choice', probabilities: a.probabilities || {} };
        else {
          const p = a.probabilities || a.distribution;
          const probs = Array.isArray(p) ? p : p && typeof p === 'object' ? Object.values(p) : [];
          const score = typeof a.score === 'number' ? a.score : probs.reduce((s, x, i) => s + x * i, 0);
          answers[id] = { type: 'score', score, probabilities: probs };
        }
      }
      return { answers };
    },
    openai: async () => {
      const qs = questions.map((q) => q.type === 'choice'
        ? { type: 'choice', name: q.id, instructions: q.text, choices: Object.entries(q.choices).map(([value, description]) => ({ value, description })) }
        : { type: 'score', name: q.id, instructions: q.text, levels: q.levels.map((label) => ({ label })) });
      const out = await post(OA + '/decisions', env.OPENAI_API_KEY, { model: MODELS.oa_decide, input: state, questions: qs }, opts.timeout || 4000);
      const answers = {};
      for (const a of out.answers || []) {
        if (a.type === 'choice') answers[a.name] = { type: 'choice', probabilities: Object.fromEntries((a.probabilities || []).map((x) => [String(x.value), x.probability])) };
        else if (a.type === 'score') answers[a.name] = { type: 'score', score: a.score, probabilities: (a.probabilities || []).map((x) => x.probability) };
      }
      return { answers };
    },
  }, opts.provider);
}
/** Sample a choice answer (so that identical situations do not always resolve the same way); null when unanswered. */
export function sample(answer) {
  if (!answer || answer.type !== 'choice') return null;
  let x = Math.random(), last = null;
  for (const [k, p] of Object.entries(answer.probabilities)) { last = k; if ((x -= p) <= 0) return k; }
  return last;
}

function parseJson(text) {
  const s = String(text || '');
  try { return JSON.parse(s); } catch {}
  const a = s.indexOf('{'), b = s.lastIndexOf('}');
  if (a >= 0 && b > a) return JSON.parse(s.slice(a, b + 1));
  throw new Error('model returned no JSON: ' + s.slice(0, 120));
}

// ---- write: one JSON object from a system + user prompt
export function write(env, { system, user, max = 400, temperature = 0.85 }, opts = {}) {
  return route(env, env.AI_PRIMARY || 'fireworks', 'write', {
    fireworks: async () => {
      const out = await post(FW + '/chat/completions', env.FIREWORKS_API_KEY, { model: MODELS.fw_llm, temperature, max_tokens: max, reasoning_effort: 'none',
        response_format: { type: 'json_object' }, messages: [{ role: 'system', content: system }, { role: 'user', content: user }] }, opts.timeout || 5000);
      return { json: parseJson(out.choices[0].message.content) };
    },
    openai: async () => {
      const out = await post(OA + '/chat/completions', env.OPENAI_API_KEY, { model: MODELS.oa_write, reasoning_effort: 'none', max_completion_tokens: max,
        response_format: { type: 'json_object' }, messages: [{ role: 'system', content: system }, { role: 'user', content: user }] }, opts.timeout || 7000);
      return { json: parseJson(out.choices[0].message.content) };
    },
  }, opts.provider);
}

// ---- hard: the careful model. tools: [{name, description, parameters}] with callTool(name, args) -> string; at most
// `rounds` tool rounds, then the model must answer with one JSON object.
export function hard(env, { system, user, tools = [], callTool = null, rounds = 2, max = 900, effort = 'low' }, opts = {}) {
  return route(env, env.AI_HARD || 'openai', 'hard', {
    openai: async () => {
      // (json_object output needs the word JSON in the input messages themselves, not only in the instructions)
      const input = [{ role: 'user', content: /json/i.test(user) ? user : user + ' Answer with one JSON object.' }];
      const body = { model: MODELS.oa_hard, instructions: system, input, reasoning: { effort }, max_output_tokens: max + 1200, text: { format: { type: 'json_object' } }, store: false };
      const defs = tools.map((t) => ({ type: 'function', name: t.name, description: t.description, parameters: t.parameters, strict: false }));
      let calls = 0;
      for (let round = 0; ; round++) {
        const useTools = defs.length && callTool && round < rounds;
        const out = await post(OA + '/responses', env.OPENAI_API_KEY, useTools ? { ...body, tools: defs, include: ['reasoning.encrypted_content'] } : body, opts.timeout || 14000);
        const fcs = (out.output || []).filter((it) => it.type === 'function_call');
        if (fcs.length && useTools) {
          // (reasoning items go back with the tool outputs; with store:false they travel as encrypted content)
          for (const it of out.output) input.push(it);
          for (const fc of fcs) {
            let res;
            try { res = await callTool(fc.name, JSON.parse(fc.arguments || '{}')); } catch (e) { res = 'error: ' + String(e).slice(0, 120); }
            input.push({ type: 'function_call_output', call_id: fc.call_id, output: typeof res === 'string' ? res : JSON.stringify(res) });
            calls++;
          }
          continue;
        }
        const text = out.output_text || (out.output || []).filter((it) => it.type === 'message').flatMap((it) => it.content || []).map((c) => c.text || '').join('');
        return { json: parseJson(text), tool_calls: calls };
      }
    },
    fireworks: async () => {
      // no tool loop on this path: the caller's prompt already holds the linked facts
      const out = await post(FW + '/chat/completions', env.FIREWORKS_API_KEY, { model: MODELS.fw_llm, temperature: 0.6, max_tokens: max + 1500, reasoning_effort: 'low',
        response_format: { type: 'json_object' }, messages: [{ role: 'system', content: system }, { role: 'user', content: user }] }, opts.timeout || 12000);
      return { json: parseJson(out.choices[0].message.content), tool_calls: 0 };
    },
  }, opts.provider);
}
