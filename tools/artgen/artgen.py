#!/usr/bin/env python3
"""Hollywoodland art and text for everything that is not an item: talent trees, ability / talent / class / spec
text and icons, menu icons, and interface art (title key art, panel textures).

    python tools/artgen/artgen.py design     # Claude designs talent tiers 1-5 around the hand-made tiers 1-2
    python tools/artgen/artgen.py text       # flavour text + icon briefs for abilities, talents, classes, specs
    python tools/artgen/artgen.py images     # FLUX 3 Klein: icons (1024 square) and interface art
    python tools/artgen/artgen.py atlas      # 96 px icons + atlas sheets, interface art for the client
    python tools/artgen/artgen.py all

Resumable: every step skips work already stored in tools/artgen/state/. gen-data.mjs merges the results.
Keys come from the environment or from the repository's .env (never printed, never written anywhere).
"""
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
STATE = HERE / 'state'
TALENT_STATE = STATE / 'talents.json'     # class -> [designed talents]
TEXT_STATE = STATE / 'texts.json'         # key -> {description, icon}
IMAGE_STATE = STATE / 'images.json'       # key -> fal request state
ART = ROOT / 'art-gen'
SRC_DIR = ART / 'originals'               # 1024 originals (not committed)
SMALL_DIR = ART / 'icons-96'
ATLAS_DIR = ART / 'atlas'
UI_DIR = ART / 'ui'
MODEL = 'claude-opus-5-5'
FALLBACK_BETA = 'server-side-fallback-2026-07-01'
FAL_MODEL = 'blackforestlabs/flux-3/klein/text-to-image'
TILE, SHEET = 96, 2048
_lock = threading.RLock()

ICON_STYLE = (
    'Hand-painted ability icon for a classic fantasy MMORPG, 1930s Hollywood Art Deco flavour. '
    'Subject: {subject}. '
    'A bold symbolic image that fills the whole square edge to edge, dramatic and readable when tiny, '
    'one clear focal shape, thick dark outlines, chunky simplified forms, confident painterly brush strokes, '
    'strong rim lighting and a glowing accent colour, rich saturated colour, high contrast, cinematic. '
    'No text, no letters, no numbers, no logo, no border, no frame.'
)
MENU_STYLE = (
    'Hand-painted interface icon for a classic fantasy MMORPG, 1930s Hollywood Art Deco flavour. '
    'Subject: {subject}. '
    'A single object, centred, filling about 75 percent of the square, bold clean silhouette, thin dark outline, '
    'polished brass and gold with deep red enamel accents, warm key light, plain very dark brown background. '
    'No text, no letters, no numbers, no logo, no border, no frame, no people.'
)
# Interface art: fixed prompts, sizes in pixels (multiples of 16).
UI_ART = {
    'ui_title': (1536, 864,
        'Art Deco movie poster key art, Hollywood in autumn 1930 under a vast transparent dome, giant insect invaders '
        'swarming over the Hollywood hills and the studio water towers at dusk, searchlights crossing the sky, '
        'Hollywood Boulevard with marquees and a red streetcar in the foreground, a small band of defenders '
        '(a soldier with a bayonet rifle, a nurse, a stuntman, a gunman in a fedora) silhouetted against the glow, '
        'bold geometric sunburst composition, gold, plum, teal and deep red palette, painterly, epic, cinematic, '
        'large empty dark sky in the upper third for a title. Every marquee, sign and poster is blank: plain glowing bulbs and '
        'coloured panels only, no lettering anywhere. No text, no letters, no numbers, no logo.'),
    'ui_panel': (1024, 1024,
        'Seamless dark interface panel texture, very dark walnut and black lacquer with a faint Art Deco '
        'geometric pattern of thin gold fan shapes and chevrons, subtle, low contrast, evenly lit, flat, top-down, '
        'no objects, no text, no vignette, no border.'),
    'ui_header': (1024, 256,
        'Art Deco ornamental header bar for a game window, deep red lacquer band with polished gold stepped edges, '
        'a gold sunburst medallion in the centre and thin gold speed lines running to both ends, symmetrical, '
        'flat front view, evenly lit, on black. No text, no letters.'),
}
MENU_ICONS = {
    'menu_char': 'a gentleman\'s fedora resting on a folded tweed suit jacket',
    'menu_bags': 'a battered leather Gladstone travel bag with brass clasps',
    'menu_quests': 'a rolled newspaper and a brass fountain pen, a red wax seal',
    'menu_skills': 'an open leather-bound field manual with a gold star on the cover',
    'menu_map': 'a folded 1930 road map of Los Angeles with a brass compass on it',
    'menu_who': 'a 1930 candlestick telephone in black bakelite and brass',
    'menu_talents': 'a gold Art Deco star medal on a red and black ribbon',
    'menu_menu': 'a brass film reel with a coil of film',
}

MOD_KEYS = {'str_pct', 'agi_pct', 'sta_pct', 'int_pct', 'spi_pct', 'hp_pct', 'res_pct', 'crit', 'dodge', 'parry', 'block',
            'armor_pct', 'dmg_pct', 'heal_pct', 'taken_pct', 'threat_pct', 'speed_pct', 'regen_pct'}
# per-rank ceilings; capstones (1 rank) may go to 3x
MOD_CAP = {'crit': 0.02, 'dodge': 0.02, 'parry': 0.02, 'block': 0.03, 'taken_pct': 0.03, 'speed_pct': 0.04,
           'hp_pct': 0.04, 'res_pct': 0.05, 'regen_pct': 0.08, 'threat_pct': 0.1, 'armor_pct': 0.06, 'dmg_pct': 0.02,
           'heal_pct': 0.02, 'ab:': 0.1, 'cost:': 0.2}
TIER_COLS = {1: 3, 2: 3, 3: 4, 4: 3, 5: 3}   # 16 per tree


# ---------------------------------------------------------------- helpers

def log(*a):
    with _lock:
        print(time.strftime('%H:%M:%S'), *a, flush=True)


def load_json(p, default):
    try:
        return json.loads(Path(p).read_text(encoding='utf-8'))
    except FileNotFoundError:
        return default


def save_json(p, obj):
    p = Path(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix('.tmp')
    tmp.write_text(json.dumps(obj, ensure_ascii=False, indent=1), encoding='utf-8')
    os.replace(tmp, p)


def load_env():
    """Fill missing keys from ROOT/.env. FAL_AI_API_KEY is accepted as FAL_KEY."""
    env = ROOT / '.env'
    if env.exists():
        for line in env.read_text(encoding='utf-8').splitlines():
            m = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$', line)
            if m and not line.lstrip().startswith('#'):
                k, v = m.group(1), m.group(2).strip().strip('"').strip("'")
                os.environ.setdefault(k, v)
    if not os.environ.get('FAL_KEY') and os.environ.get('FAL_AI_API_KEY'):
        os.environ['FAL_KEY'] = os.environ['FAL_AI_API_KEY']


def need_env(name):
    if not os.environ.get(name):
        sys.exit(f'{name} is not set (environment or .env).')


def game_data():
    """Classes, abilities and the hand-made talents straight from the generator modules."""
    js = ("import { classes, abilities } from './tools/gen/classes.mjs';"
          "import { talents } from './tools/gen/talents.mjs';"
          "console.log(JSON.stringify({ classes, abilities, talents }));")
    out = subprocess.run(['node', '--input-type=module', '-e', js], cwd=ROOT, capture_output=True, text=True, check=True)
    return json.loads(out.stdout)


def claude_json(system, user, schema, effort='medium', max_tokens=32000):
    import anthropic
    client = anthropic.Anthropic(max_retries=4)
    params = dict(model=MODEL, max_tokens=max_tokens,
                  system=[{'type': 'text', 'text': system, 'cache_control': {'type': 'ephemeral', 'ttl': '1h'}}],
                  output_config={'effort': effort, 'format': {'type': 'json_schema', 'schema': schema}},
                  messages=[{'role': 'user', 'content': user}])
    with client.beta.messages.stream(betas=[FALLBACK_BETA], fallbacks='default', **params) as s:
        msg = s.get_final_message()
    if msg.stop_reason == 'refusal':
        raise RuntimeError('refused')
    if msg.stop_reason == 'max_tokens':
        raise RuntimeError('hit max_tokens')
    return json.loads(''.join(b.text for b in msg.content if b.type == 'text'))


def world_brief():
    """The setting sections of the design bible (the same ones the item writer gets)."""
    bible = (ROOT / 'DESIGN-BIBLE.md').read_text(encoding='utf-8')
    parts = re.split(r'(?m)^(?=## \d+\. )', bible)
    keep = []
    for p in parts:
        m = re.match(r'## (\d+)\. ', p)
        if m and int(m.group(1)) in {3, 4, 5}:
            keep.append('#' + re.sub(r'(?m)^&#91;embedded content.*$\n?', '', p).strip())
    return '\n\n'.join(keep)


# ---------------------------------------------------------------- design step

DESIGN_SCHEMA = {
    'type': 'object', 'additionalProperties': False, 'required': ['talents'],
    'properties': {'talents': {'type': 'array', 'items': {
        'type': 'object', 'additionalProperties': False,
        'required': ['spec', 'tier', 'col', 'slug', 'name', 'ranks', 'mods'],
        'properties': {
            'spec': {'type': 'integer'}, 'tier': {'type': 'integer'}, 'col': {'type': 'integer'},
            'slug': {'type': 'string'}, 'name': {'type': 'string'}, 'ranks': {'type': 'integer'},
            'mods': {'type': 'array', 'items': {'type': 'object', 'additionalProperties': False,
                     'required': ['key', 'value'], 'properties': {'key': {'type': 'string'}, 'value': {'type': 'number'}}}}}}}},
}

DESIGN_SYSTEM = """You are the systems designer for Hollywoodland, a classic-style MMORPG (2004-era World of Warcraft, levels 1-60) set in Hollywood in autumn 1930 under an insect invasion. You are completing the talent trees.

Rules of the tree (fixed):
- One talent point per level from 10 (51 at 60). Three trees per class, 16 talents per tree, five tiers. A tier opens after 5 points are spent in that tree (tier 2 at 5, tier 3 at 10, tier 4 at 15, tier 5 at 20).
- Layout per tree: tier 1 has 3 talents (cols 0-2), tier 2 has 3 (cols 0-2), tier 3 has 4 (cols 0-3), tier 4 has 3 (cols 0-2), tier 5 has 3 (cols 0-2).
- Talents are passive modifiers only. Ranks: 1, 2, 3 or 5. Each tree's tier 5 must have exactly one 1-rank capstone, the tree's signature.
- Values are PER RANK. Keys you may use (exactly these spellings):
  str_pct agi_pct sta_pct int_pct spi_pct (attribute +fraction), hp_pct res_pct (max health / Power), crit dodge parry block (flat chance, 0.01 = 1%), armor_pct, dmg_pct heal_pct (done), taken_pct (negative = less damage taken), threat_pct, speed_pct, regen_pct (resource regeneration),
  ab:<ability id> (that ability's damage, healing or absorb +fraction), cost:<ability id> (cost -fraction), cd:<ability id> (cooldown, seconds shorter).
  Only use ability ids from the class's own list. A talent may carry one or two mods.
- Per-rank ceilings: crit/dodge/parry 0.02, block 0.03, dmg_pct and heal_pct 0.02, taken_pct -0.03, speed_pct 0.04, hp_pct 0.04, res_pct 0.05, regen_pct 0.08, threat_pct 0.1, armor_pct 0.06, ab: 0.1, cost: 0.2, cd: no more than a third of the ability's cooldown over all ranks. A 1-rank capstone may be up to three times these.
- Tank trees lean on armour, health, threat and avoidance; healer trees on healing, Power and regeneration; damage trees on damage, crit and their key abilities. Spread the class's abilities across its trees the way the spec names suggest. Avoid duplicating a talent already in the tree.

Names: period-true 1930 Hollywood and Great War flavour, two or three words, wry where it fits (see the hand-made ones). The slug is a short lower_snake_case id unique within the class.

You receive one class with its abilities and the hand-made tier 1-2 talents (2 per tier per tree). Add exactly what is missing: per tree, one talent at tier 1 col 2, one at tier 2 col 2, four at tier 3, three at tier 4, three at tier 5. That is 12 per tree, 36 in all."""


def validate_design(cls, rows, ab_ids, ab_cd, existing_ids, prefix):
    out, errs = [], []
    seen = set(existing_ids)
    for r in rows:
        tid = f"tal_{prefix}_{re.sub(r'[^a-z0-9_]+', '_', r['slug'].lower()).strip('_')}"
        if tid in seen:
            tid += f"_{r['spec']}{r['tier']}{r['col']}"
        mods, ok = {}, True
        for m in r['mods']:
            k, v = m['key'], float(m['value'])
            base = k if k in MOD_KEYS else (k.split(':')[0] + ':' if ':' in k else None)
            if base is None or (base in ('ab:', 'cost:', 'cd:') and k.split(':', 1)[1] not in ab_ids):
                errs.append(f'{tid}: bad mod {k}'); ok = False; continue
            mult = 3 if r['ranks'] == 1 else 1
            if base == 'cd:':
                cap = ab_cd.get(k[3:], 0) / 3 / max(1, r['ranks']) * mult
                v = min(v, round(cap, 2))
                if v <= 0:
                    errs.append(f'{tid}: cd on an ability without a cooldown'); ok = False; continue
            elif base == 'taken_pct':
                v = -min(abs(v), MOD_CAP['taken_pct'] * mult)
            else:
                v = max(0.0, min(v, MOD_CAP.get(base, 0.02 if base.endswith('_pct') else 0.02) * mult))
            mods[k] = round(v, 4)
        if not ok or not mods or r['ranks'] not in (1, 2, 3, 5):
            errs.append(f'{tid}: rejected'); continue
        seen.add(tid)
        out.append({'id': tid, 'name': r['name'].strip(), 'class': cls['id'], 'spec': r['spec'], 'tier': r['tier'],
                    'col': r['col'], 'ranks': r['ranks'], 'mods': mods, 'desc': ''})
    return out, errs


def step_design(args, data):
    state = load_json(TALENT_STATE, {})
    todo = [c for c in data['classes'] if c['id'] not in state or args.redo]
    log(f'design: {len(data["classes"]) - len(todo)} of {len(data["classes"])} classes done; {len(todo)} to do')
    if not todo:
        return
    need_env('ANTHROPIC_API_KEY')

    def run(cls):
        abil = [a for a in data['abilities'] if a['class'] == cls['id']]
        hand = [t for t in data['talents'] if t['class'] == cls['id']]
        user = json.dumps({'class': {k: cls[k] for k in ('id', 'name', 'role', 'resource', 'primary', 'secondary', 'specs')},
                           'abilities': [{k: a[k] for k in ('id', 'name', 'level', 'cd', 'cost', 'school', 'target', 'desc')} for a in abil],
                           'hand_made_talents': [{k: t[k] for k in ('spec', 'tier', 'col', 'name', 'ranks', 'mods')} for t in hand]},
                          ensure_ascii=False, indent=1)
        for attempt in range(3):
            res = claude_json(DESIGN_SYSTEM, user, DESIGN_SCHEMA, effort=args.effort)
            rows, errs = validate_design(cls, res['talents'], {a['id'] for a in abil}, {a['id']: a['cd'] for a in abil},
                                         [t['id'] for t in hand], abil[0]['id'].split('_')[1])
            # keep only the wanted slots, first come first served
            want = {(s, t, c) for s in range(3) for t, n in TIER_COLS.items() for c in range(n)}
            have = {(t['spec'], t['tier'], t['col']) for t in hand}
            got, used = [], set()
            for r in rows:
                key = (r['spec'], r['tier'], r['col'])
                if key in want and key not in have and key not in used:
                    used.add(key); got.append(r)
            missing = want - have - used
            if errs:
                log(f'  {cls["id"]}: {len(errs)} fix-ups: ' + '; '.join(errs[:6]))
            if not missing:
                return cls['id'], got
            log(f'  {cls["id"]}: {len(missing)} slot(s) missing, asking again')
        raise RuntimeError(f'{cls["id"]}: could not fill every slot')

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for f in as_completed([pool.submit(run, c) for c in todo]):
            try:
                cid, rows = f.result()
            except Exception as ex:
                log(f'design: FAILED {ex}'); continue
            with _lock:
                state[cid] = rows
                save_json(TALENT_STATE, state)
            log(f'design: {cid}: {len(rows)} talents')


def all_talents(data):
    state = load_json(TALENT_STATE, {})
    out = list(data['talents'])
    for c in data['classes']:
        out += state.get(c['id'], [])
    return out


# ---------------------------------------------------------------- text step

TEXT_SCHEMA = {
    'type': 'object', 'additionalProperties': False, 'required': ['entries'],
    'properties': {'entries': {'type': 'array', 'items': {
        'type': 'object', 'additionalProperties': False, 'required': ['key', 'description', 'icon'],
        'properties': {'key': {'type': 'string'}, 'description': {'type': 'string'}, 'icon': {'type': 'string'}}}}},
}

TEXT_SYSTEM = """You are the writer for Hollywoodland, a classic-style MMORPG (2004-era World of Warcraft, levels 1-60) set in Hollywood in autumn 1930. Insect invaders have sealed Hollywood under a transparent Dome; veterans, stuntmen, gunmen, studio electricians, powder men, nurses, preachers and mechanics fight back.

You write the flavour line shown under the mechanics in the tooltips of abilities, talents, classes and specialisations, and a visual brief for each icon.

Voice: period-true, plain-spoken, a little wry: a 1930 Los Angeles newspaperman, a studio prop master and a WoW quest writer sharing a booth at Musso & Frank's. Concrete nouns, short sentences. Humour is seasoning (about one in four), from the period and the setting, never mocking the player. No modern slang, no fourth-wall breaks, no game jargon ("DPS", "tank", "buff", "proc", "talent point").

- description: the flavour line. Abilities and talents: one or two sentences, 12-30 words, saying what it looks or feels like in the world, never restating numbers (the tooltip shows those). Classes: two or three sentences, 35-60 words, who these people are in 1930 Hollywood and why they fight. Specs: one or two sentences, 15-35 words.
- icon: a visual brief of 8-20 words for a painted square icon: one striking, concrete subject with action or glow (for example "a bayonet thrusting forward through green insect ichor, motion streaks"). Distinct from its neighbours. No text or lettering. For a class, an emblem of the trade; for a spec, a symbol of that discipline.

Copy every key exactly; one entry per key, same order.

"""


def text_entries(data):
    cls_by = {c['id']: c for c in data['classes']}
    rows = []
    for c in data['classes']:
        rows.append({'key': f"class_{c['id']}", 'group': c['id'], 'kind': 'class', 'name': c['name'],
                     'role': c['role'], 'specs': c['specs']})
        for i, s in enumerate(c['specs']):
            rows.append({'key': f"spec_{c['id']}_{i}", 'group': c['id'], 'kind': 'spec', 'name': s, 'class': c['name']})
    for a in data['abilities']:
        if a['class'] in cls_by:
            rows.append({'key': a['id'], 'group': a['class'], 'kind': 'ability', 'name': a['name'],
                         'class': cls_by[a['class']]['name'], 'mechanics': a['desc']})
    for t in all_talents(data):
        c = cls_by[t['class']]
        rows.append({'key': t['id'], 'group': t['class'], 'kind': 'talent', 'name': t['name'], 'class': c['name'],
                     'tree': c['specs'][t['spec']], 'mods': t['mods']})
    return rows


def step_text(args, data):
    texts = load_json(TEXT_STATE, {})
    rows = [r for r in text_entries(data) if r['key'] not in texts or args.redo]
    log(f'text: {len(rows)} to write')
    if not rows:
        return
    need_env('ANTHROPIC_API_KEY')
    system = TEXT_SYSTEM + world_brief()
    batches, cur = [], []
    for r in rows:   # batches stay inside one class so neighbours read as a set
        if cur and (len(cur) >= args.batch_size or cur[-1]['group'] != r['group']):
            batches.append(cur); cur = []
        cur.append(r)
    if cur:
        batches.append(cur)

    def run(i, b):
        user = f'Batch {i + 1} of {len(batches)}: {len(b)} entries.\n\n' + '\n'.join(
            json.dumps({k: v for k, v in r.items() if k != 'group'}, ensure_ascii=False) for r in b)
        try:
            res = claude_json(system, user, TEXT_SCHEMA, effort=args.effort)
        except Exception as ex:
            log(f'  batch {i + 1}: failed ({str(ex)[:160]}); rerun to retry'); return
        want = {r['key'] for r in b}
        n = 0
        with _lock:
            for e in res.get('entries', []):
                if e['key'] in want and e['description'].strip() and e['icon'].strip():
                    texts[e['key']] = {'description': e['description'].strip(), 'icon': e['icon'].strip()}
                    n += 1
            save_json(TEXT_STATE, texts)
        log(f'  batch {i + 1}/{len(batches)}: {n}/{len(b)}')

    run(0, batches[0])   # writes the prompt cache first
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for f in as_completed([pool.submit(run, i, b) for i, b in enumerate(batches) if i]):
            f.result()


# ---------------------------------------------------------------- images step

def fal_headers():
    return {'Authorization': f"Key {os.environ['FAL_KEY']}", 'Content-Type': 'application/json'}


def make_image(key, prompt, w, h, images, http):
    st = images.setdefault(key, {'attempt': 0})
    out = SRC_DIR / f'{key}.png'
    if not st.get('request_id'):
        st['seed'] = int(hashlib.sha256(f'{key}:{st["attempt"]}'.encode()).hexdigest()[:8], 16) & 0x7FFFFFFF
        st['prompt'] = prompt
        body = {'prompt': prompt, 'image_size': {'width': w, 'height': h}, 'num_images': 1, 'seed': st['seed'],
                'enable_prompt_expansion': False, 'output_format': 'png', 'enable_safety_checker': True}
        r = http.post(f'https://queue.fal.run/{FAL_MODEL}', headers=fal_headers(), json=body, timeout=60)
        r.raise_for_status()
        j = r.json()
        st.update(request_id=j['request_id'], status_url=j['status_url'], response_url=j['response_url'])
        with _lock:
            save_json(IMAGE_STATE, images)
    t0 = time.time()
    while True:
        r = http.get(st['status_url'], headers=fal_headers(), timeout=60)
        if r.status_code == 404:
            st['request_id'] = None
            raise RuntimeError('request expired; resubmits next run')
        r.raise_for_status()
        j = r.json()
        if j.get('status') == 'COMPLETED':
            if j.get('error'):
                st['request_id'] = None; st['attempt'] += 1
                raise RuntimeError(f"fal error: {j['error']}")
            break
        if time.time() - t0 > 600:
            raise TimeoutError('queued for 10 minutes; rerun to keep waiting')
        time.sleep(2)
    r = http.get(st['response_url'], headers=fal_headers(), timeout=60)
    r.raise_for_status()
    res = r.json()
    if any(res.get('has_nsfw_concepts') or []):
        st['request_id'] = None; st['attempt'] += 1
        raise RuntimeError('safety checker flagged the image; new seed next run')
    img = http.get(res['images'][0]['url'], timeout=120)
    img.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.part')
    tmp.write_bytes(img.content)
    os.replace(tmp, out)
    st.update(status='done', request_id=None, prompt=prompt, at=time.strftime('%Y-%m-%d %H:%M:%S'))
    with _lock:
        save_json(IMAGE_STATE, images)


def image_jobs(data):
    texts = load_json(TEXT_STATE, {})
    jobs = {}
    for r in text_entries(data):
        if r['key'] in texts:
            jobs[r['key']] = (ICON_STYLE.format(subject=texts[r['key']]['icon'].rstrip('. ')), 1024, 1024)
    for k, subj in MENU_ICONS.items():
        jobs[k] = (MENU_STYLE.format(subject=subj), 1024, 1024)
    for k, (w, h, p) in UI_ART.items():
        jobs[k] = (p, w, h)
    return jobs


def step_images(args, data):
    import requests
    images = load_json(IMAGE_STATE, {})
    jobs = image_jobs(data)
    redo = set((args.redo or '').split(',')) if args.redo and args.redo != 'all' else set()
    todo = []
    for k, (p, w, h) in jobs.items():
        st = images.get(k, {})
        done = (SRC_DIR / f'{k}.png').exists() and st.get('status') == 'done' and st.get('prompt') == p
        if args.redo == 'all' or k in redo or (st.get('status') == 'done' and st.get('prompt') != p):
            if st.get('status') == 'done':
                images[k] = {'attempt': st.get('attempt', 0) + 1}
            done = False
        if not done:
            todo.append(k)
    log(f'images: {len(jobs) - len(todo)} of {len(jobs)} done; {len(todo)} to make')
    if not todo:
        return
    need_env('FAL_KEY')
    http = requests.Session()
    fails = 0

    def run(k):
        p, w, h = jobs[k]
        try:
            make_image(k, p, w, h, images, http)
            return None
        except Exception as ex:
            with _lock:
                images.setdefault(k, {})['status'] = 'failed'
                images[k]['error'] = str(ex)[:300]
                save_json(IMAGE_STATE, images)
            return ex

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futs = {pool.submit(run, k): k for k in todo}
        for i, f in enumerate(as_completed(futs), 1):
            ex = f.result()
            if ex:
                fails += 1
                log(f'  [{i}/{len(todo)}] {futs[f]}: FAILED {str(ex)[:150]}')
            elif i % 20 == 0 or i == len(todo):
                log(f'  [{i}/{len(todo)}] {futs[f]}')
    log(f'images: {len(todo) - fails} made, {fails} failed' + (' (rerun to retry)' if fails else ''))


# ---------------------------------------------------------------- atlas step

def step_atlas(args, data):
    from PIL import Image, ImageFilter
    keys = [r['key'] for r in text_entries(data)] + list(MENU_ICONS)
    keys = [k for k in keys if (SRC_DIR / f'{k}.png').exists()]
    per_row = SHEET // TILE
    per_sheet = per_row * per_row
    n_sheets = -(-len(keys) // per_sheet) if keys else 0
    sheets = [Image.new('RGB', (SHEET, SHEET), (0, 0, 0)) for _ in range(n_sheets)]
    icons = {}
    for idx, k in enumerate(keys):
        small = SMALL_DIR / f'{k}.png'
        src = SRC_DIR / f'{k}.png'
        if not small.exists() or small.stat().st_mtime < src.stat().st_mtime:
            with Image.open(src) as im:
                im = im.convert('RGB').resize((384, 384), Image.LANCZOS).resize((TILE, TILE), Image.LANCZOS)
                im = im.filter(ImageFilter.UnsharpMask(radius=0.8, percent=70, threshold=2))
                small.parent.mkdir(parents=True, exist_ok=True)
                im.save(small, optimize=True)
        s, cell = divmod(idx, per_sheet)
        with Image.open(small) as im:
            sheets[s].paste(im.convert('RGB'), ((cell % per_row) * TILE, (cell // per_row) * TILE))
        icons[k] = idx
    ATLAS_DIR.mkdir(parents=True, exist_ok=True)
    names = []
    for s, im in enumerate(sheets):
        name = f'skill_icons_{s}.png'
        im.save(ATLAS_DIR / name, optimize=True)
        im.save(ATLAS_DIR / name.replace('.png', '.jpg'), quality=92, subsampling=0, optimize=True)
        names.append(name)
    save_json(ATLAS_DIR / 'skill_icons.json', {'tile': TILE, 'sheet_size': SHEET, 'columns': per_row,
                                               'per_sheet': per_sheet, 'sheets': names, 'icons': icons})
    # interface art: JPEG for the client
    UI_DIR.mkdir(parents=True, exist_ok=True)
    for k in UI_ART:
        src = SRC_DIR / f'{k}.png'
        if src.exists():
            with Image.open(src) as im:
                im = im.convert('RGB')
                if k == 'ui_header':   # trim the black surround so the ornament sits on any background
                    box = im.point(lambda v: 255 if v > 24 else 0).convert('L').getbbox()
                    if box:
                        im = im.crop(box)
                im.save(UI_DIR / f'{k}.jpg', quality=88, optimize=True)
    log(f'atlas: {len(icons)} icons on {len(names)} sheet(s); {sum((SRC_DIR / f"{k}.png").exists() for k in UI_ART)} interface images')


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('step', choices=['design', 'text', 'images', 'atlas', 'all'])
    ap.add_argument('--effort', default='medium', choices=['low', 'medium', 'high', 'xhigh', 'max'])
    ap.add_argument('--workers', type=int, default=6)
    ap.add_argument('--batch-size', type=int, default=20)
    ap.add_argument('--redo', help="design/text: redo everything; images: comma-separated keys or 'all'")
    args = ap.parse_args()
    load_env()
    data = game_data()
    for s in (['design', 'text', 'images', 'atlas'] if args.step == 'all' else [args.step]):
        {'design': step_design, 'text': step_text, 'images': step_images, 'atlas': step_atlas}[s](args, data)


if __name__ == '__main__':
    main()
