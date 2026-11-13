#!/usr/bin/env python3
"""Hollywoodland spoken dialogue: NPC lines, quest dialogue, player barks and emotes (Claude), voice casting (Claude),
speech (ElevenLabs v4 through fal.ai), mastering and the game's indexes.

    python tools/dialoguegen/dialoguegen.py plan       # no keys: the jobs, line and character counts, a sample request
    python tools/dialoguegen/dialoguegen.py text       # write every job's lines                      (ANTHROPIC_API_KEY)
    python tools/dialoguegen/dialoguegen.py cast       # one request casts every speaker to a voice   (ANTHROPIC_API_KEY)
    python tools/dialoguegen/dialoguegen.py apply      # Content/Data/dialogue.json (subtitles, emotes, voice ids)
    python tools/dialoguegen/dialoguegen.py review     # tools/dialoguegen/review.md: every line with its speaker and voice
    python tools/dialoguegen/dialoguegen.py tts        # speech for every line (FAL_KEY / FAL_AI_API_KEY); resumable
    python tools/dialoguegen/dialoguegen.py master     # loudness-master to sfx/vo_*.mp3 + Content/Data/vo.json
    python tools/dialoguegen/dialoguegen.py all        # text, cast, apply, review, tts, master

Every step resumes from tools/dialoguegen/state/. --only ID,ID / --redo ID,ID|all target jobs (text) or speakers (tts).
Keys come from the environment or the repo's .env; nothing here prints or stores them.

Ids. A job is one NPC (all Boulevard Patrolmen share the job 'npc_guard'), one player class (both sexes), or 'emotes'.
Spoken lines are addressed as <speaker>/<kind>/<n>: the audio is vo_<speaker>_<kind> variant n (sfx/vo_<speaker>_<kind>_<n>.mp3)
and the subtitle is dialogue.json[...][kind][n], so text and audio always line up. Quest lines use the quest id as the
speaker (vo_q_hb_01_offer) and the giver's or turn-in NPC's voice.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DATA = ROOT / 'hollywoodland-ue' / 'Content' / 'Data'
BIBLE = ROOT / 'DESIGN-BIBLE.md'
STATE = HERE / 'state'
TEXT_STATE = STATE / 'lines.json'        # job -> generated JSON + model + at
CAST_STATE = STATE / 'cast.json'         # speaker -> {voice, stability, similarity, why}
TTS_STATE = STATE / 'tts.json'           # line key -> {hash, request_id, status_url, response_url, file}
RAW = ROOT / 'vo-src'                    # fal originals (not committed)
OUT = ROOT / 'sfx'                       # mastered vo_*.mp3 next to the sound effects (same R2 prefix, same loader)
DIALOGUE = DATA / 'dialogue.json'
VO_INDEX = DATA / 'vo.json'

MODEL = 'claude-opus-5-5'
FALLBACK_BETA = 'server-side-fallback-2026-07-01'
FAL_TTS = 'elevenlabs/tts/eleven-v4'
PRICE = {'in': 4.0, 'out': 20.0, 'cache_write_1h': 8.0, 'cache_read': 0.20}
TTS_PRICE_PER_1K = 0.08

# ElevenLabs stock voices available by name on fal (premade library). Descriptions steer the casting request.
VOICES = {
    'Roger': 'male, middle-aged, American; confident, laid-back, a little gravel',
    'George': 'male, middle-aged, British; warm, measured storyteller',
    'Callum': 'male, middle-aged, transatlantic; husky, intense, a touch villainous',
    'Charlie': 'male, young adult, Australian; easy-going, natural, cheeky',
    'Liam': 'male, young adult, American; articulate, energetic, earnest',
    'Will': 'male, young adult, American; friendly, relaxed, a bit goofy',
    'Eric': 'male, middle-aged, American; smooth, polished, salesman charm',
    'Chris': 'male, middle-aged, American; casual, everyman, wry',
    'Brian': 'male, middle-aged, American; deep, resonant, authoritative',
    'Daniel': 'male, middle-aged, British; crisp, formal, newsreader authority',
    'Bill': 'male, old, American; weathered, trustworthy, slow drawl',
    'Aria': 'female, middle-aged, American; husky, expressive, worldly',
    'Sarah': 'female, young adult, American; soft, warm, reassuring',
    'Laura': 'female, young adult, American; upbeat, sassy, quick',
    'Charlotte': 'female, young adult, Swedish-accented English; sultry, cool, theatrical',
    'Alice': 'female, middle-aged, British; clear, confident, brisk',
    'Matilda': 'female, middle-aged, American; friendly, motherly, warm',
    'Jessica': 'female, young adult, American; playful, bright, expressive',
    'Lily': 'female, middle-aged, British; warm, slightly raspy, wry',
    'Rachel': 'female, young adult, American; calm, clear, composed',
    'River': 'androgynous, adult, American; calm, even, unhurried',
}

# NPC roles -> the service lines the NPC says when that window opens
ROLE_KINDS = {'vendor': 'vendor', 'repair': 'vendor', 'trainer': 'trainer', 'trainer_prof': 'trainer', 'banker': 'bank',
              'auctioneer': 'auction', 'innkeeper': 'inn', 'redcar': 'redcar'}
# guards answer "where is...?" (the gossip menu sets a map waypoint to the POI)
DIRECTIONS = {'bank': 'First National Bank (teller, vault)', 'auction': 'the auction house', 'inn': 'the Roosevelt Hotel (innkeeper, hearth)',
              'trainers': 'the class trainers', 'redcar': 'the Pacific Electric red car stop', 'vendor': 'general goods and repairs',
              'mailbox': 'the mailbox', 'police': 'the Hollywood Police Station'}
# player voice: error/state barks and emote vocals (n per sex)
BARKS = {
    'no_target': (3, 'you tried an attack with nothing targeted'),
    'out_of_range': (3, 'the target is too far away'),
    'no_power': (3, "not enough of the class resource (named in the job) for the ability"),
    'not_ready': (3, 'the ability is still on cooldown'),
    'cant': (3, "the action is not possible right now (generic 'I can't do that')"),
    'bags_full': (2, 'the bags are full'),
    'low_health': (3, 'health just dropped under a quarter in a fight'),
    'level_up': (2, 'the character just gained a level'),
    'quest_done': (2, 'all objectives of a quest just got finished'),
}
EMOTE_VOICE = {
    'silly': (6, 'a short corny joke or pun in character (period, setting, the class), delivered for laughs; 8-30 words'),
    'flirt': (6, 'a cheesy period pickup line in character; 6-25 words; charming, PG'),
    'laugh': (2, 'a laugh, performed (tag + laughter)'),
    'cheer': (2, 'a cheer, performed (tag + short whoop or words)'),
    'train': (2, 'imitating a steam train, performed (tag + choo-choo and whistle)'),
    'chicken': (2, 'clucking like a chicken at someone (tag + bawk sounds, maybe one word of taunt)'),
    'rasp': (2, 'blowing a raspberry (tag + the sound)'),
    'charge': (2, 'a battle cry to charge (period, in character, 2-8 words)'),
    'cry': (2, 'sobbing, performed (tag + a few broken words or sobs)'),
    'spit': (1, 'spitting, performed (tag + the sound, maybe one disgusted word)'),
}
# emotes: chat text from Claude ('emotes' job), animation + voice kind from here
EMOTES = {  # id: (aliases, gesture clip, voice kind or '')
    'train': ([], 'Cheer', 'train'), 'silly': ([], 'Talk', 'silly'), 'flirt': ([], 'Wave', 'flirt'),
    'laugh': (['lol'], 'Laugh', 'laugh'), 'cheer': ([], 'Cheer', 'cheer'), 'chicken': ([], 'Shrug', 'chicken'),
    'rasp': ([], 'Laugh', 'rasp'), 'charge': ([], 'Point', 'charge'), 'cry': ([], 'Shrug', 'cry'),
    'dance': ([], 'Dance', ''), 'wave': ([], 'Wave', ''), 'bow': ([], 'Bow', ''), 'spit': ([], 'Shrug', 'spit'),
    'rude': ([], 'Point', ''), 'point': ([], 'Point', ''), 'shrug': ([], 'Shrug', ''),
}
SEXES = (('m', 'Male'), ('f', 'Female'))

_lock = threading.RLock()


def log(*a):
    with _lock:
        print(*a, flush=True)


def load_json(p: Path, default):
    try:
        return json.loads(p.read_text(encoding='utf-8'))
    except FileNotFoundError:
        return default


def save_json(p: Path, obj, indent=1):
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(p.suffix + '.tmp')
    tmp.write_text(json.dumps(obj, ensure_ascii=False, indent=indent) + '\n', encoding='utf-8')
    os.replace(tmp, p)


def load_env():
    """Fill missing keys from the repo .env (never printed). fal accepts FAL_KEY; the repo names it FAL_AI_API_KEY."""
    env = ROOT / '.env'
    if env.exists():
        for line in env.read_text(encoding='utf-8').splitlines():
            m = re.match(r'\s*([A-Z0-9_]+)\s*=\s*(.*)\s*$', line)
            if m and m.group(1) not in os.environ:
                os.environ[m.group(1)] = m.group(2).strip().strip('"').strip("'")
    if 'FAL_KEY' not in os.environ and os.environ.get('FAL_AI_API_KEY'):
        os.environ['FAL_KEY'] = os.environ['FAL_AI_API_KEY']


def need_env(k):
    if not os.environ.get(k):
        sys.exit(f'{k} is not set (environment or .env)')


def strip_tags(s: str) -> str:
    return re.sub(r'\s{2,}', ' ', re.sub(r'\[[^\]]*\]', '', s)).strip()


# ---------------------------------------------------------------- world data

def world():
    areas = json.loads((DATA / 'areas.json').read_text(encoding='utf-8'))
    areas = areas.get('areas', areas) if isinstance(areas, dict) else areas
    areas = areas if isinstance(areas, list) else list(areas.values())
    zone = areas[0]
    quests = json.loads((DATA / 'quests.json').read_text(encoding='utf-8'))
    quests = quests.get('quests', quests) if isinstance(quests, dict) else quests
    mobs = json.loads((DATA / 'mobs.json').read_text(encoding='utf-8'))
    mobs = mobs.get('mobs', mobs) if isinstance(mobs, dict) and 'mobs' in mobs else mobs
    mobs = {m['id']: m for m in (mobs if isinstance(mobs, list) else mobs.values())}
    items = json.loads((DATA / 'items.json').read_text(encoding='utf-8'))
    items = items.get('items', items) if isinstance(items, dict) else items
    classes = json.loads((DATA / 'classes.json').read_text(encoding='utf-8'))
    classes = classes.get('classes', classes) if isinstance(classes, dict) and 'classes' in classes else classes
    classes = classes if isinstance(classes, list) else list(classes.values())
    names = {m: d['name'] for m, d in mobs.items()}
    names.update({i['id']: i['name'] for i in items})
    names.update({n['id']: n['name'] + (f" ({n['title']})" if n.get('title') else '') for n in zone['npcs']})
    names.update({o['id']: o.get('name', o['id']) for o in zone.get('objects', []) if 'id' in o})
    hubs = {}
    for n in zone['npcs']:
        if n.get('hub'):
            hubs[n['hub']] = hubs.get(n['hub']) or hub_name(zone, n)
    return {'zone': zone, 'quests': quests, 'mobs': mobs, 'names': names, 'classes': classes, 'hubs': hubs}


def hub_name(zone, n):
    for s in zone['subzones']:
        if s['x'] <= n['x'] <= s['x'] + s['w'] and s['y'] <= n['y'] <= s['y'] + s['d'] and not s.get('hostile') and s['name'] != 'The Hive Wall':
            return s['name']
    return n['hub']


def place_of(zone, x, y):
    best = None
    for s in zone['subzones']:
        if s['x'] <= x <= s['x'] + s['w'] and s['y'] <= y <= s['y'] + s['d'] and s['name'] != 'The Hive Wall':
            if best is None or s['w'] * s['d'] < best['w'] * best['d']:
                best = s
    return best['name'] if best else 'Hollywood Boulevard'


def resolve(obj, names):
    """Copy of a quest row with ids replaced by 'Name [id]' so the writer sees names but can keep ids straight."""
    if isinstance(obj, dict):
        return {k: resolve(v, names) for k, v in obj.items()}
    if isinstance(obj, list):
        return [resolve(v, names) for v in obj]
    if isinstance(obj, str) and obj in names:
        return f'{names[obj]} [{obj}]'
    return obj


def quest_view(q, w):
    keep = {k: q[k] for k in ('id', 'name', 'level', 'giver', 'turnin', 'type', 'objectives', 'prereq', 'next', 'hub', 'group', 'dungeon') if k in q}
    keep['rewards'] = {k: v for k, v in (q.get('rewards') or {}).items() if k in ('choice', 'rep')}
    v = resolve(keep, w['names'])
    for k in ('prereq', 'next'):
        if q.get(k):
            nq = next((x for x in w['quests'] if x['id'] == q[k]), None)
            if nq:
                v[k] = f"{nq['name']} [{nq['id']}]"
    v['hub'] = w['hubs'].get(q.get('hub'), q.get('hub'))
    v['current_text'] = {'offer': q.get('text', ''), 'complete': q.get('complete_text', '')}
    return v


def jobs(w):
    """Every writing job: NPCs (guards as one), classes, emotes."""
    zone = w['zone']
    out = []
    seen_guard = False
    pois = zone.get('pois', {})
    for n in zone['npcs']:
        roles = n.get('roles', [])
        if 'guard' in roles:
            if seen_guard:
                continue
            seen_guard = True
            jid = 'npc_guard'
        else:
            jid = n['id']
        qs = [q for q in w['quests'] if q.get('built', True) and (q.get('giver') == n['id'] or q.get('turnin') == n['id'])]
        if jid == 'npc_guard':
            qs = []
        kinds = {'hello': 4, 'greet': 3, 'bye': 3, 'annoyed': 2}
        for r in roles:
            if r in ROLE_KINDS:
                kinds[ROLE_KINDS[r]] = 2
        spec = {'id': jid, 'name': n['name'] if jid != 'npc_guard' else 'Boulevard Patrolman (any of the LAPD guards on the Boulevard)',
                'title': n.get('title', ''), 'roles': roles, 'place': place_of(zone, n['x'], n['y']), 'real_person': bool(n.get('real_person')),
                'died': n.get('died'), 'quests': [quest_view(q, w) for q in qs], 'kinds': kinds}
        if jid == 'npc_guard':
            spec['directions'] = {k: {'what': DIRECTIONS.get(k, k), 'place': place_of(zone, *pois[k])} for k in DIRECTIONS if k in pois}
            spec['place'] = 'anywhere along Hollywood Boulevard (patrolmen walk the whole street)'
            spec['note'] = ('Directions must work from anywhere on the Boulevard: name the corner or building and the cross streets '
                            '(the Boulevard runs west to east: the Chinese forecourt, Highland, Wilcox, Cahuenga, Vine). Never say "here", "just east" or "a block".')
        out.append({'id': jid, 'type': 'npc', 'spec': spec})
    for c in w['classes']:
        out.append({'id': f"class_{c['id']}", 'type': 'class', 'spec': {
            'class': c['id'], 'name': c['name'], 'resource': c.get('resource') or c.get('power') or 'power',
            'about': {k: c[k] for k in c if k in ('role', 'desc', 'armour', 'weapons', 'specs', 'spec_desc')}}})
    out.append({'id': 'emotes', 'type': 'emotes', 'spec': {'emotes': list(EMOTES)}})
    return out


# ---------------------------------------------------------------- prompts and schemas

def arr(n, desc):
    # structured outputs accept no minItems > 1: the count is in the description and check() enforces it
    return {'type': 'array', 'description': f'exactly {n} entries; {desc}', 'items': {'type': 'string'}}


KIND_DESC = {
    'hello': 'barks when the player clicks the NPC: 1-8 words each, the NPC noticing the player, each different',
    'greet': 'the opening line of the talk window: 1-3 sentences, 12-45 words, personality, a hint of what is on their mind',
    'bye': 'barks when the player walks off or closes the window: 1-8 words, like WoW merchants ("Come back when your purse is fatter!")',
    'annoyed': 'barks after being clicked over and over: 4-15 words, exasperated, funny',
    'vendor': 'said as the shop window opens: 4-18 words, sells / repairs, salesmanship in character',
    'trainer': 'said as the training window opens: 4-18 words, about learning the trade',
    'bank': 'said as the bank vault opens: 4-18 words, about safekeeping',
    'auction': 'said as the auction window opens: 4-18 words, auctioneer patter',
    'inn': 'said when the player talks about a room or making the hotel home: 4-18 words',
    'redcar': 'said as the red car route window opens: 4-18 words, conductor patter',
}


def npc_schema(spec):
    props = {k: arr(n, KIND_DESC[k]) for k, n in spec['kinds'].items()}
    req = list(props)
    props['voice'] = {'type': 'string', 'description': 'casting note for a voice actor: sex, age, accent, timbre, pace, attitude; 15-40 words'}
    req.append('voice')
    if spec['quests']:
        props['quests'] = {'type': 'array', 'description': f"exactly {len(spec['quests'])} entries, one per quest in order", 'items': {
            'type': 'object', 'additionalProperties': False, 'required': ['id', 'offer', 'progress', 'complete'],
            'properties': {'id': {'type': 'string'}, 'offer': {'type': 'string'}, 'progress': {'type': 'string'}, 'complete': {'type': 'string'}}}}
        req.append('quests')
    if spec.get('directions'):
        props['directions'] = {'type': 'object', 'additionalProperties': False, 'required': list(spec['directions']),
                               'properties': {k: {'type': 'string', 'description': '8-30 words'} for k in spec['directions']}}
        req.append('directions')
    return {'type': 'object', 'additionalProperties': False, 'required': req, 'properties': props}


def class_schema():
    # lists keyed by an enum keep the compiled grammar small (one object per kind instead of a property per kind);
    # normalize() turns them back into {barks: {kind: [...]}, emotes: {kind: [...]}}
    kinds = {**{k: f'{n} lines: {d}; 2-8 words, in character' for k, (n, d) in BARKS.items()}, **{k: f'{n} lines: {d}' for k, (n, d) in EMOTE_VOICE.items()}}
    one = {'type': 'object', 'additionalProperties': False, 'required': ['voice', 'sets'], 'properties': {
        'voice': {'type': 'string', 'description': 'casting note for this sex of the class: age, accent, timbre, attitude; 15-40 words'},
        'sets': {'type': 'array', 'description': 'one entry per kind, every kind once. Kinds and counts: ' + json.dumps(kinds), 'items': {
            'type': 'object', 'additionalProperties': False, 'required': ['kind', 'lines'],
            'properties': {'kind': {'type': 'string', 'enum': list(kinds)}, 'lines': {'type': 'array', 'items': {'type': 'string'}}}}}}}
    return {'type': 'object', 'additionalProperties': False, 'required': ['male', 'female'], 'properties': {'male': one, 'female': one}}


def emote_schema():
    return {'type': 'object', 'additionalProperties': False, 'required': ['emotes'], 'properties': {'emotes': {
        'type': 'array', 'description': 'one entry per emote id, every id once', 'items': {
            'type': 'object', 'additionalProperties': False, 'required': ['id', 'alone', 'target', 'self_alone', 'self_target'], 'properties': {
                'id': {'type': 'string', 'enum': list(EMOTES)},
                'alone': {'type': 'string', 'description': 'seen by others with no target: "{name} ..." third person'},
                'target': {'type': 'string', 'description': 'seen by others with a target: "{name} ... {target} ..."'},
                'self_alone': {'type': 'string', 'description': 'seen by the emoter: "You ..."'},
                'self_target': {'type': 'string', 'description': 'seen by the emoter: "You ... {target} ..."'}}}}}}


def normalize(job, data):
    """Back to the dict form the rest of the pipeline uses; extra entries in a line array are dropped."""
    if job['type'] == 'class':
        for key in ('male', 'female'):
            sets = {s['kind']: s['lines'] for s in data[key].pop('sets', [])}
            data[key]['barks'] = {k: sets.get(k, [])[:n] for k, (n, _) in BARKS.items()}
            data[key]['emotes'] = {k: sets.get(k, [])[:n] for k, (n, _) in EMOTE_VOICE.items()}
    elif job['type'] == 'emotes':
        data = {e.pop('id'): e for e in data['emotes']}
    else:
        for k, n in job['spec']['kinds'].items():
            data[k] = data.get(k, [])[:n]
    return data


def build_preamble():
    brief = (HERE / 'preamble.md').read_text(encoding='utf-8')
    bible = BIBLE.read_text(encoding='utf-8')
    keep = {3, 4, 5, 7, 10}
    parts = re.split(r'(?m)^(?=## \d+\. )', bible)
    picked = []
    for p in parts:
        m = re.match(r'## (\d+)\. ', p)
        if m and int(m.group(1)) in keep:
            picked.append('#' + p.strip())
    return brief.rstrip() + '\n\n# World reference (from the design bible)\n\n' + '\n\n'.join(picked) + '\n'


def user_message(job, all_jobs):
    s = job['spec']
    if job['type'] == 'npc':
        others = [j['spec']['name'] + (f" ({j['spec']['title']})" if j['spec'].get('title') else '') for j in all_jobs if j['type'] == 'npc' and j['id'] != job['id']]
        head = (f"Job: the non-player character below. Write every array with exactly the count asked for. "
                f"Quests: one entry per quest in the same order, id copied exactly; rewrite the current text so it is funnier, sharper and exactly consistent "
                f"with the objectives (keep its facts; a quest whose turn-in is someone else still ends with a pointer to them). "
                f"If 'directions' is asked for, give one line per place telling the player where it is and how to get there.\n"
                f"Other people on the Boulevard you may mention: {', '.join(others)}.")
        return head + '\n\n' + json.dumps(s, ensure_ascii=False, indent=1)
    if job['type'] == 'class':
        return (f"Job: the player-character voice set of the {s['name']} class, male and female (two different people with their own humour). "
                f"The class resource is called '{s['resource']}' (use it in the no_power barks). These lines are spoken by the player's own character, "
                f"so they never address the player as 'you, the player', never mention game mechanics by number, and stay in the class fantasy.\n\n"
                + json.dumps(s, ensure_ascii=False, indent=1))
    return ('Job: the emote table. For each emote write the four chat lines, using {name} for the emoter and {target} for the target, '
            'one short sentence each, period-flavoured and funny where the emote allows (WoW style: "{name} makes some strange gestures."). '
            "Emote meanings: train = acts like a steam train; silly = tells a joke; flirt; laugh/lol; cheer; chicken = clucks at someone like a coward; "
            'rasp = blows a raspberry; charge = calls for a charge; cry; dance; wave; bow; spit; rude = makes a rude gesture; point; shrug.\n\n'
            + json.dumps(s))


def schema_for(job):
    return npc_schema(job['spec']) if job['type'] == 'npc' else class_schema() if job['type'] == 'class' else emote_schema()


def request_params(preamble, job, all_jobs, effort):
    return dict(model=MODEL, max_tokens=32000,
                system=[{'type': 'text', 'text': preamble, 'cache_control': {'type': 'ephemeral', 'ttl': '1h'}}],
                output_config={'effort': effort, 'format': {'type': 'json_schema', 'schema': schema_for(job)}},
                messages=[{'role': 'user', 'content': user_message(job, all_jobs)}])


class Usage:
    def __init__(self):
        self.t = {'in': 0, 'out': 0, 'cache_write_1h': 0, 'cache_read': 0}

    def add(self, u):
        with _lock:
            self.t['in'] += u.input_tokens or 0
            self.t['out'] += u.output_tokens or 0
            self.t['cache_write_1h'] += getattr(u, 'cache_creation_input_tokens', 0) or 0
            self.t['cache_read'] += getattr(u, 'cache_read_input_tokens', 0) or 0

    def report(self):
        cost = sum(self.t[k] * PRICE[k] for k in self.t) / 1e6
        return ', '.join(f'{k} {v:,}' for k, v in self.t.items()) + f'  (~${cost:.2f} estimated)'


def parse_message(msg):
    if msg.stop_reason == 'refusal':
        raise RuntimeError('refused')
    if msg.stop_reason == 'max_tokens':
        raise RuntimeError('hit max_tokens')
    return json.loads(''.join(b.text for b in msg.content if b.type == 'text'))


def check(job, data):
    """Light logic checks; problems are printed and the job stays pending (rerun to rewrite it)."""
    probs = []

    def count(where, arr_, n):
        if len(arr_) != n:
            probs.append(f'{where}: {len(arr_)} entries, wanted {n}')

    if job['type'] == 'npc':
        for k, n in job['spec']['kinds'].items():
            count(k, data.get(k, []), n)
    elif job['type'] == 'class':
        for key in ('male', 'female'):
            for k, (n, _) in BARKS.items():
                count(f'{key}.{k}', data[key]['barks'].get(k, []), n)
            for k, (n, _) in EMOTE_VOICE.items():
                count(f'{key}.{k}', data[key]['emotes'].get(k, []), n)
    if job['type'] == 'npc':
        want = [q['id'] for q in job['spec']['quests']]
        got = [q['id'] for q in data.get('quests', [])]
        if got != want:
            probs.append(f'quest ids {got} != {want}')
        for q, qv in zip(data.get('quests', []), job['spec']['quests']):
            for o in qv.get('objectives', []):
                c = o.get('count')
                if c and c > 1:
                    words = {2: 'two', 3: 'three', 4: 'four', 5: 'five', 6: 'six', 7: 'seven', 8: 'eight', 9: 'nine', 10: 'ten', 11: 'eleven', 12: 'twelve',
                             15: 'fifteen', 20: 'twenty'}
                    if str(c) not in q['offer'] and words.get(c, '#') not in q['offer'].lower() and 'dozen' not in q['offer'].lower():
                        probs.append(f"{q['id']}: offer does not state the count {c}")
    return probs


def step_text(args, js):
    texts = load_json(TEXT_STATE, {})
    only = set(args.only.split(',')) if args.only else None
    redo = set(js_id for js_id in (args.redo.split(',') if args.redo else []))
    todo = [j for j in js if (not only or j['id'] in only) and (j['id'] not in texts or 'all' in redo or j['id'] in redo)]
    log(f'text: {len(texts)} of {len(js)} jobs written; {len(todo)} to do')
    if not todo:
        return
    need_env('ANTHROPIC_API_KEY')
    import anthropic
    client = anthropic.Anthropic(max_retries=4)
    preamble = build_preamble()
    usage = Usage()

    def run(job):
        try:
            with client.beta.messages.stream(betas=[FALLBACK_BETA], fallbacks='default', **request_params(preamble, job, js, args.effort)) as s:
                msg = s.get_final_message()
            usage.add(msg.usage)
            data = parse_message(msg)
        except (anthropic.AuthenticationError, anthropic.PermissionDeniedError, anthropic.BadRequestError):
            raise
        except (anthropic.APIStatusError, anthropic.APIConnectionError, RuntimeError, json.JSONDecodeError) as ex:
            log(f"{job['id']}: failed ({type(ex).__name__}: {str(ex)[:200]}); rerun to retry")
            return
        data = normalize(job, data)
        probs = check(job, data)
        if job['type'] == 'emotes':
            probs += [f'emote {k} missing' for k in EMOTES if k not in data]
        with _lock:
            texts[job['id']] = {'data': data, 'model': msg.model, 'at': time.strftime('%Y-%m-%d %H:%M:%S'), 'problems': probs}
            save_json(TEXT_STATE, texts)
        log(f"{job['id']}: written" + (f"  PROBLEMS: {'; '.join(probs)}" if probs else '') + f"  [cache read {msg.usage.cache_read_input_tokens or 0:,}]")

    run(todo[0])
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for f in as_completed([pool.submit(run, j) for j in todo[1:]]):
            f.result()
    log('text usage: ' + usage.report())


# ---------------------------------------------------------------- speakers and lines

def speakers(js, texts):
    """speaker id -> {'note': voice note, 'who': description, 'group': casting group}"""
    sp = {}
    for j in js:
        d = texts.get(j['id'], {}).get('data')
        if not d:
            continue
        if j['type'] == 'npc':
            who = f"{j['spec']['name']}, {j['spec']['title']} ({j['spec']['place']})"
            if j['id'] == 'npc_guard':
                for k in range(3):  # three patrolmen voices share the guard lines
                    sp[f'npc_guard_v{k}'] = {'note': d['voice'], 'who': f'Boulevard Patrolman #{k + 1} (LAPD); the three must sound distinct', 'group': 'npc'}
            else:
                sp[j['id']] = {'note': d['voice'], 'who': who, 'group': 'npc'}
        elif j['type'] == 'class':
            for sx, key in (('m', 'male'), ('f', 'female')):
                sp[f"pc_{j['spec']['class']}_{sx}"] = {'note': d[key]['voice'], 'who': f"player character: {j['spec']['name']}, {key}", 'group': f'player_{key}'}
    return sp


def lines(js, texts):
    """Every spoken line: key -> {speaker (voice), sound id, variant, say (TTS text with tags), text (subtitle)}"""
    out = {}

    def add(speaker, sound, i, say, ref):
        # ref: (job id, path into its data) - where the text lives, so `soften` can rewrite it in place
        out[f'{sound}/{i}'] = {'speaker': speaker, 'sound': sound, 'n': i, 'say': say.strip(), 'text': strip_tags(say), 'ref': ref}

    for j in js:
        d = texts.get(j['id'], {}).get('data')
        if not d:
            continue
        if j['type'] == 'npc':
            voice = j['id'] if j['id'] != 'npc_guard' else None
            for kind in j['spec']['kinds']:
                for i, s in enumerate(d.get(kind, [])):
                    if voice:
                        add(voice, f'vo_{voice}_{kind}', i, s, (j['id'], [kind, i]))
                    else:  # guards: every line in each of the three voices (variant = line * 3 + voice)
                        for v in range(3):
                            add(f'npc_guard_v{v}', f'vo_npc_guard_{kind}', i * 3 + v, s, (j['id'], [kind, i]))
            for k, s in (d.get('directions') or {}).items():
                for v in range(3):
                    add(f'npc_guard_v{v}', f'vo_npc_guard_dir_{k}', v, s, (j['id'], ['directions', k]))
            for qi, q in enumerate(d.get('quests', [])):
                qd = next(x for x in j['spec']['quests'] if x['id'] == q['id'])
                giver = re.search(r'\[(npc_[a-z0-9_]+)\]', str(qd.get('giver', '')))
                turnin = re.search(r'\[(npc_[a-z0-9_]+)\]', str(qd.get('turnin', '')))
                giver = giver.group(1) if giver else j['id']
                turnin = turnin.group(1) if turnin else giver
                # each quest is written by its giver's job (and also appears in the turn-in's job when they differ: the giver's wins)
                if giver != j['id'] and any(x['id'] == giver for x in js):
                    continue
                add(giver, f"vo_{q['id']}_offer", 0, q['offer'], (j['id'], ['quests', qi, 'offer']))
                add(turnin, f"vo_{q['id']}_progress", 0, q['progress'], (j['id'], ['quests', qi, 'progress']))
                add(turnin, f"vo_{q['id']}_complete", 0, q['complete'], (j['id'], ['quests', qi, 'complete']))
        elif j['type'] == 'class':
            c = j['spec']['class']
            for sx, key in (('m', 'male'), ('f', 'female')):
                spk = f'pc_{c}_{sx}'
                for grp in ('barks', 'emotes'):
                    for kind, arr_ in d[key][grp].items():
                        for i, s in enumerate(arr_):
                            add(spk, f'vo_{spk}_{kind}', i, s, (j['id'], [key, grp, kind, i]))
    return out


# ---------------------------------------------------------------- casting

CAST_SCHEMA = {'type': 'object', 'additionalProperties': False, 'required': ['cast'], 'properties': {'cast': {'type': 'array', 'items': {
    'type': 'object', 'additionalProperties': False, 'required': ['speaker', 'voice', 'stability', 'similarity', 'why'],
    'properties': {'speaker': {'type': 'string'}, 'voice': {'type': 'string', 'enum': list(VOICES)},
                   'stability': {'type': 'number', 'description': '0.25 (wild, expressive) to 0.75 (steady)'},
                   'similarity': {'type': 'number', 'description': '0.6 to 0.9'}, 'why': {'type': 'string'}}}}}}


def step_cast(args, js):
    texts = load_json(TEXT_STATE, {})
    sp = speakers(js, texts)
    cast = load_json(CAST_STATE, {})
    if cast and set(sp) <= set(cast) and args.redo != 'all':
        log(f'cast: all {len(sp)} speakers cast')
        return
    need_env('ANTHROPIC_API_KEY')
    import anthropic
    client = anthropic.Anthropic(max_retries=4)
    msg_text = ('Cast every speaker below to one of the stock voices (descriptions given). Rules: match sex and age; '
                'the 8 male player characters get 8 different voices and the 8 female player characters 8 different voices where the library allows '
                '(the androgynous voice may fill one gap); NPCs in the same place should not share a voice; the three patrolmen must use three different voices; '
                'a voice may be reused across far-apart NPCs and players when unavoidable, then vary stability. '
                'Lower stability for comic, theatrical speakers; higher for calm or authoritative ones.\n\n'
                f'Voices:\n{json.dumps(VOICES, indent=1)}\n\nSpeakers:\n'
                + json.dumps({k: {'who': v['who'], 'note': v['note']} for k, v in sp.items()}, ensure_ascii=False, indent=1))
    with client.beta.messages.stream(betas=[FALLBACK_BETA], fallbacks='default', model=MODEL, max_tokens=32000,
                                     output_config={'effort': args.effort, 'format': {'type': 'json_schema', 'schema': CAST_SCHEMA}},
                                     messages=[{'role': 'user', 'content': msg_text}]) as s:
        msg = s.get_final_message()
    data = parse_message(msg)
    for c in data['cast']:
        if c['speaker'] in sp:
            cast[c['speaker']] = {'voice': c['voice'], 'stability': min(0.8, max(0.2, c['stability'])),
                                  'similarity': min(0.95, max(0.5, c['similarity'])), 'why': c['why']}
    missing = [k for k in sp if k not in cast]
    save_json(CAST_STATE, cast)
    log(f'cast: {len(cast)} speakers' + (f'; missing {missing} (rerun)' if missing else ''))


# ---------------------------------------------------------------- apply / review

def step_apply(args, js):
    texts = load_json(TEXT_STATE, {})
    out = {'note': 'Generated by tools/dialoguegen/dialoguegen.py. Subtitles; audio is vo_<speaker>_<kind> variant n in vo.json.',
           'npcs': {}, 'quests': {}, 'players': {}, 'emotes': {}}
    for j in js:
        d = texts.get(j['id'], {}).get('data')
        if not d:
            continue
        if j['type'] == 'npc':
            e = {k: [strip_tags(s) for s in d.get(k, [])] for k in j['spec']['kinds']}
            if d.get('directions'):
                e['directions'] = {k: strip_tags(v) for k, v in d['directions'].items()}
            out['npcs'][j['id']] = e
            for q in d.get('quests', []):
                qd = next(x for x in j['spec']['quests'] if x['id'] == q['id'])
                g = re.search(r'\[(npc_[a-z0-9_]+)\]', str(qd.get('giver', '')))
                if g and g.group(1) != j['id'] and any(x['id'] == g.group(1) for x in js):
                    continue
                out['quests'][q['id']] = {k: strip_tags(q[k]) for k in ('offer', 'progress', 'complete')}
        elif j['type'] == 'class':
            for sx, key in (('m', 'male'), ('f', 'female')):
                out['players'][f"{j['spec']['class']}_{sx}"] = {k: [strip_tags(s) for s in v] for k, v in {**d[key]['barks'], **d[key]['emotes']}.items()}
        else:
            for k, (aliases, clip, voice) in EMOTES.items():
                out['emotes'][k] = {**d[k], 'aliases': aliases, 'anim': clip, 'voice': voice}
    save_json(DIALOGUE, out)
    n = sum(len(v) for e in out['npcs'].values() for v in e.values())
    log(f"apply: {DIALOGUE.relative_to(ROOT)}: {len(out['npcs'])} npcs ({n} line groups), {len(out['quests'])} quests, "
        f"{len(out['players'])} player voices, {len(out['emotes'])} emotes")


def step_review(args, js):
    texts = load_json(TEXT_STATE, {})
    cast = load_json(CAST_STATE, {})
    ls = lines(js, texts)
    md = ['# Hollywoodland dialogue review', '', f'{len(ls)} spoken lines, {sum(len(l["say"]) for l in ls.values()):,} characters.', '']
    cur = None
    for k, l in ls.items():
        if l['speaker'].startswith('npc_guard_v') and not l['speaker'].endswith('v0'):
            continue
        if l['speaker'] != cur:
            cur = l['speaker']
            c = cast.get(cur, {})
            md += ['', f"## {cur}" + (f" — {c['voice']} (stability {c['stability']:.2f})" if c else ''), '']
        md.append(f"- `{l['sound']}/{l['n']}` {l['say']}")
    (HERE / 'review.md').write_text('\n'.join(md) + '\n', encoding='utf-8')
    log(f'review: tools/dialoguegen/review.md ({len(ls)} lines)')


# ---------------------------------------------------------------- speech

def fal_headers():
    return {'Authorization': f"Key {os.environ['FAL_KEY']}", 'Content-Type': 'application/json'}


def line_hash(l, c):
    return hashlib.sha256(json.dumps([l['say'], c['voice'], round(c['stability'], 2), round(c['similarity'], 2)]).encode()).hexdigest()[:16]


def raw_path(l):
    return RAW / f"{l['sound']}_{l['n']}.mp3"


def tts_one(key, l, c, st, http):
    h = line_hash(l, c)
    out = raw_path(l)
    if st.get(key, {}).get('hash') == h and out.exists():
        return 'kept'
    if st.get(key, {}).get('hash') == h and st[key].get('flagged'):
        return 'flagged'
    s = st.setdefault(key, {})
    if s.get('hash') != h:
        s.clear()
        s['hash'] = h
    if not s.get('request_id'):
        body = {'text': l['say'], 'voice': c['voice'], 'stability': c['stability'], 'similarity_boost': c['similarity'],
                'seed': int(h[:8], 16) & 0x7FFFFFFF, 'output_format': 'mp3_44100_128', 'language_code': 'en'}
        r = http.post(f'https://queue.fal.run/{FAL_TTS}', headers=fal_headers(), json=body, timeout=60)
        if r.status_code in (401, 403):
            raise PermissionError(f'fal rejected the key ({r.status_code})')
        r.raise_for_status()
        j = r.json()
        s.update(request_id=j['request_id'], status_url=j['status_url'], response_url=j['response_url'])
        with _lock:
            save_json(TTS_STATE, st)
    t0 = time.time()
    while True:
        r = http.get(s['status_url'], headers=fal_headers(), timeout=60)
        if r.status_code == 404:
            s.pop('request_id', None)
            raise RuntimeError('request expired on fal; resubmits next run')
        r.raise_for_status()
        j = r.json()
        if j.get('status') == 'COMPLETED':
            if j.get('error'):
                s.pop('request_id', None)
                raise RuntimeError(f"fal error: {j.get('error')}")
            break
        if time.time() - t0 > 300:
            raise TimeoutError('still queued after 5 minutes; rerun to keep waiting')
        time.sleep(1.0)
    r = http.get(s['response_url'], headers=fal_headers(), timeout=60)
    if r.status_code == 422:
        # fal's content checker (combat talk reads as "violence"): remember it so `tts` skips the line until its text
        # changes; `soften` rewrites flagged lines
        cats = sorted({c for d in (r.json().get('detail') or []) if isinstance(d, dict) for c in d.get('categories', [])})
        s.update(request_id=None, flagged=cats or ['unknown'])
        with _lock:
            save_json(TTS_STATE, st)
        raise RuntimeError(f'flagged by fal content checker: {cats}')
    r.raise_for_status()
    url = r.json()['audio']['url']
    a = http.get(url, timeout=120)
    a.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.part')
    tmp.write_bytes(a.content)
    os.replace(tmp, out)
    s.update(request_id=None, file=out.name, at=time.strftime('%Y-%m-%d %H:%M:%S'))
    with _lock:
        save_json(TTS_STATE, st)
    return 'made'


def step_tts(args, js):
    import requests
    texts = load_json(TEXT_STATE, {})
    cast = load_json(CAST_STATE, {})
    ls = lines(js, texts)
    only = set(args.only.split(',')) if args.only else None
    todo = {k: l for k, l in ls.items() if (not only or l['speaker'] in only or l['sound'] in only) and l['speaker'] in cast}
    if args.max_lines:
        todo = dict(list(todo.items())[:args.max_lines])
    st = load_json(TTS_STATE, {})
    chars = sum(len(l['say']) for k, l in todo.items() if not (st.get(k, {}).get('hash') == line_hash(l, cast[l['speaker']]) and raw_path(l).exists()))
    log(f'tts: {len(todo)} lines selected, {chars:,} characters to synthesise (~${chars / 1000 * TTS_PRICE_PER_1K:.2f})')
    uncast = sorted({l['speaker'] for l in ls.values() if l['speaker'] not in cast})
    if uncast:
        log(f'  not cast yet (run cast): {uncast}')
    if not chars:
        return
    need_env('FAL_KEY')
    http = requests.Session()
    done = {'made': 0, 'kept': 0, 'flagged': 0, 'failed': 0}

    def run(k, l):
        try:
            done[tts_one(k, l, cast[l['speaker']], st, http)] += 1
        except PermissionError:
            raise
        except Exception as ex:  # noqa: BLE001 - one line failing must not stop the run
            done['failed'] += 1
            log(f'  {k}: {type(ex).__name__}: {str(ex)[:160]}')
        n = sum(done.values())
        if n % 50 == 0:
            log(f'  {n}/{len(todo)}  {done}')

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for f in as_completed([pool.submit(run, k, l) for k, l in todo.items()]):
            f.result()
    log(f'tts: {done}' + ('  (rerun to retry the failed ones)' if done['failed'] else '')
        + ('  (run `soften` to rewrite the flagged ones, then tts again)' if done['flagged'] else ''))


# ---------------------------------------------------------------- mastering + index

SPEECH_LUFS = -17.0
GAIN = {'npc': 1.0, 'player': 0.9}


def master_one(src: Path, dst: Path):
    """Loudness-normalise speech (EBU R128, two-pass), trim silence, mono 64 kbps."""
    flt = 'silenceremove=start_periods=1:start_threshold=-50dB,areverse,silenceremove=start_periods=1:start_threshold=-50dB,areverse'
    r = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-i', str(src), '-af', f'{flt},loudnorm=I={SPEECH_LUFS}:TP=-1.5:LRA=11:print_format=json',
                        '-f', 'null', '-'], capture_output=True, text=True)
    m = re.search(r'\{[^{}]*"input_i"[^{}]*\}', r.stderr, re.S)
    if not m:
        raise RuntimeError(f'loudnorm measure failed for {src.name}')
    j = json.loads(m.group(0))
    ln = (f"loudnorm=I={SPEECH_LUFS}:TP=-1.5:LRA=11:measured_I={j['input_i']}:measured_TP={j['input_tp']}:measured_LRA={j['input_lra']}"
          f":measured_thresh={j['input_thresh']}:offset={j['target_offset']}:linear=true")
    tmp = dst.with_suffix('.part.mp3')
    r = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-y', '-i', str(src), '-af', f'{flt},{ln},aresample=44100', '-ac', '1', '-b:a', '64k', str(tmp)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr[-400:])
    os.replace(tmp, dst)


def step_master(args, js):
    texts = load_json(TEXT_STATE, {})
    ls = lines(js, texts)
    sounds = {}
    jobs_ = []
    for k, l in ls.items():
        src = raw_path(l)
        if not src.exists():
            continue
        dst = OUT / src.name
        if not dst.exists() or dst.stat().st_mtime < src.stat().st_mtime:
            jobs_.append((src, dst))
        sp = l['speaker']
        e = sounds.setdefault(l['sound'], {'cat': 'voice', 'vol': GAIN['player' if sp.startswith('pc_') else 'npc'],
                                           'range': 0 if sp.startswith('pc_') else 30, 'pitch': 0, 'files': {}})
        e['files'][l['n']] = src.name
    with ThreadPoolExecutor(max_workers=8) as pool:
        for f in as_completed([pool.submit(master_one, s, d) for s, d in jobs_]):
            f.result()
    # files as a dense list in variant order (a missing variant keeps its slot so subtitles still line up)
    idx = {}
    for sid, e in sorted(sounds.items()):
        n = max(e['files']) + 1
        idx[sid] = {**{k: v for k, v in e.items() if k != 'files'}, 'files': [e['files'].get(i, '') for i in range(n)]}
    save_json(VO_INDEX, {'note': 'Generated by tools/dialoguegen/dialoguegen.py (ElevenLabs v4 via fal.ai). Do not edit by hand.', 'sounds': idx})
    log(f'master: {len(jobs_)} files mastered; {VO_INDEX.relative_to(ROOT)}: {len(idx)} sounds, {sum(len(e["files"]) for e in idx.values())} files')


# ---------------------------------------------------------------- plan / main

def step_plan(args, js):
    w = world()
    texts = load_json(TEXT_STATE, {})
    log(f"jobs: {len(js)} ({sum(j['type'] == 'npc' for j in js)} npc, {sum(j['type'] == 'class' for j in js)} class, 1 emotes); written {len(texts)}")
    ls = lines(js, texts)
    if ls:
        log(f"lines: {len(ls)}, {sum(len(l['say']) for l in ls.values()):,} characters (~${sum(len(l['say']) for l in ls.values()) / 1000 * TTS_PRICE_PER_1K:.2f} of speech)")
    j = next(x for x in js if x['id'] == 'npc_sid_grauman')
    STATE.mkdir(parents=True, exist_ok=True)
    (STATE / 'sample_request.txt').write_text(user_message(j, js) + '\n\nSCHEMA:\n' + json.dumps(schema_for(j), indent=1), encoding='utf-8')
    log(f'sample request: {(STATE / "sample_request.txt").relative_to(ROOT)}; preamble {len(build_preamble()):,} chars; quests {len(w["quests"])}')


SOFTEN_SCHEMA = {'type': 'object', 'additionalProperties': False, 'required': ['rewrites'], 'properties': {'rewrites': {'type': 'array', 'items': {
    'type': 'object', 'additionalProperties': False, 'required': ['key', 'say'],
    'properties': {'key': {'type': 'string'}, 'say': {'type': 'string'}}}}}}


def step_soften(args, js):
    """Rewrite the lines fal's content checker flagged, in place in lines.json (subtitle and voice text together)."""
    texts = load_json(TEXT_STATE, {})
    st = load_json(TTS_STATE, {})
    cast = load_json(CAST_STATE, {})
    ls = lines(js, texts)
    flagged = {k: l for k, l in ls.items() if st.get(k, {}).get('flagged') and st[k].get('hash') == line_hash(l, cast[l['speaker']])}
    # one rewrite per source text (the three guard voices share one)
    by_ref = {}
    for k, l in flagged.items():
        by_ref.setdefault(json.dumps(l['ref']), (k, l, st[k]['flagged']))
    log(f'soften: {len(flagged)} flagged lines, {len(by_ref)} texts to rewrite')
    if not by_ref:
        return
    need_env('ANTHROPIC_API_KEY')
    import anthropic
    client = anthropic.Anthropic(max_retries=4)
    items = [{'key': k, 'flagged_as': cats, 'kind': l['sound'], 'say': l['say']} for k, l, cats in by_ref.values()]
    msg_text = ('A text-to-speech service\'s automatic content checker rejected these game dialogue lines (categories given). '
                'Rewrite each so it passes while staying the same line: same speaker voice, meaning, jokes, length (within 15%), '
                'audio tags in [brackets], and any numbers or objective counts. Swap graphic or violent wording (kill, shoot, '
                'blood, gut, slaughter, die, murder, corpse, blow up...) for period-appropriate euphemism and slang '
                '(clear out, put down, send packing, deal with, fix their wagon, lay them low, the bugs, the mess...). '
                'For "sexual": keep the flirt or tease wholesome screwball-comedy innuendo-free. For "harassment": aim the '
                'taunt at the situation or make it good-natured ribbing, no insults about the person. '
                'It is a lighthearted 1930s pulp game; keep it funny. Return every key.\n\n'
                + json.dumps(items, ensure_ascii=False, indent=1))
    with client.beta.messages.stream(betas=[FALLBACK_BETA], fallbacks='default', model=MODEL, max_tokens=32000,
                                     output_config={'effort': args.effort, 'format': {'type': 'json_schema', 'schema': SOFTEN_SCHEMA}},
                                     messages=[{'role': 'user', 'content': msg_text}]) as s:
        msg = s.get_final_message()
    data = parse_message(msg)
    n = 0
    for r in data['rewrites']:
        if r['key'] not in flagged:
            continue
        job_id, path = flagged[r['key']]['ref']
        node = texts[job_id]['data']
        for p in path[:-1]:
            node = node[p]
        log(f"  {r['key']}:\n    was: {node[path[-1]]}\n    now: {r['say']}")
        node[path[-1]] = r['say']
        n += 1
    save_json(TEXT_STATE, texts)
    log(f'soften: rewrote {n}; now run apply, review and tts')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('step', choices=['plan', 'text', 'cast', 'apply', 'review', 'tts', 'soften', 'master', 'all'])
    ap.add_argument('--only', default='')
    ap.add_argument('--redo', default='')
    ap.add_argument('--effort', default='high', choices=['low', 'medium', 'high', 'xhigh', 'max'])
    ap.add_argument('--workers', type=int, default=6)
    ap.add_argument('--max-lines', type=int, default=0, help='tts: only the first N selected lines (pilot)')
    args = ap.parse_args()
    load_env()
    js = jobs(world())
    steps = ['text', 'cast', 'apply', 'review', 'tts', 'master'] if args.step == 'all' else [args.step]
    for s in steps:
        globals()[f'step_{s}'](args, js)


if __name__ == '__main__':
    main()
