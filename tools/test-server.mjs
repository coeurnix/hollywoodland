// Smoke test for hollywoodland-server. usage: node tools/test-server.mjs [base]
const base = process.argv[2] || 'https://hollywoodland-server.coeurnix.workers.dev';
const post = async (path, body) => {
  const t0 = Date.now();
  const r = await fetch(base + path, { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: JSON.stringify(body) });
  const j = await r.json();
  console.log(`\n== ${path} ${body.kind || ''} ${r.status} ${Date.now() - t0}ms`);
  console.log(JSON.stringify(j, null, 1));
  return j;
};
const s = await post('/v1/session', {});
const speakers = [{ persona: 'persona_0142', handle: 'Brickwall', class: 'veteran', level: 9, temperament: 'helpful',
  typing: 'lowercase, abbreviations, few typos', activity: 'questing in the Selma alleys', guild: 'Gower Irregulars',
  memory: ['grouped with player on Franklin', 'player gave them a hat'], liking: 2 }];
const context = { player: { name: 'Tester', level: 6, class: 'nurse' }, zone: 'Hollywood Boulevard', hub: 'Chinese forecourt',
  in_combat: false, recent_chat: ['[zone] Mabel: lfm egyptian need tank', '[zone] Dutch: wts [Stetson of the Boxer] 40c'],
  world: ['The Egyptian Theatre is a 5-player dungeon for levels 8-12 on Hollywood Boulevard.',
    'Paramount Studios is a level 34-40 dungeon in Melrose.'] };
const allowed = { goals: ['GROUP', 'FOLLOW', 'IDLE'], biases: ['heal_priority'], ids: ['The Egyptian Theatre', 'Hollywood Boulevard', 'Paramount Studios'] };
await post('/v1/sim', { session: s.token, id: 1, kind: 'reply', speakers, context, allowed,
  trigger: { channel: 'whisper', from: 'player', text: 'hey can you run me through paramount? also are you a real person?' } });
await post('/v1/sim', { session: s.token, id: 2, kind: 'decide', speakers, context, allowed, trigger: { kind: 'invite', text: 'killing drones in the alleys' } });
await post('/v1/sim', { session: s.token, id: 3, kind: 'gossip',
  speakers: [speakers[0], { persona: 'persona_0007', handle: 'xXMabelXx', class: 'nurse', level: 58, temperament: 'loud', typing: 'ALL CAPS sometimes, no punctuation' }],
  context: { ...context, events: ['Guild <Gower Irregulars> cleared Paramount Studios', 'Dutch looted [Cornero\'s Rum-Runner Slicker] (epic) in Burbank'] },
  allowed, trigger: { channel: 'zone' } });
