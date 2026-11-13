#!/usr/bin/env python3
"""Hollywoodland item content pipeline: tooltip descriptions (Claude) and inventory icons (FLUX 3 Klein on fal.ai).

    python tools/itemgen/itemgen.py plan                 # no keys needed: counts, batches, a sample request
    python tools/itemgen/itemgen.py text [--batch-api]   # descriptions + icon briefs        (ANTHROPIC_API_KEY)
    python tools/itemgen/itemgen.py icons                # 1024x1024 originals               (FAL_KEY)
    python tools/itemgen/itemgen.py atlas                # 96x96 icons + atlas sheets + mapping
    python tools/itemgen/itemgen.py apply                # write desc / icon into items.json
    python tools/itemgen/itemgen.py review               # item-art/review.html contact sheet
    python tools/itemgen/itemgen.py all                  # text, icons, atlas, apply, review

Every step resumes: finished work is kept in tools/itemgen/state/ and item-art/, so a rerun only does what is missing.
Items that share a name (the same item at several level bands) share one description and one icon.
Use --only KEY,KEY or --redo KEY,KEY (or --redo all) to target or regenerate specific entries.
Keys come only from the environment; nothing here prints or stores them.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import re
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ITEMS = ROOT / 'hollywoodland-ue' / 'Content' / 'Data' / 'items.json'
BIBLE = ROOT / 'DESIGN-BIBLE.md'
STATE = HERE / 'state'
TEXT_STATE = STATE / 'descriptions.json'      # key -> {name, description, icon, model, at}
ICON_STATE = STATE / 'icons.json'             # key -> {seed, prompt, request_id, status_url, response_url, status, ...}
PENDING_BATCH = STATE / 'pending_batch.json'  # in-flight Message Batch (batch-api mode)
ART = ROOT / 'item-art'
SRC_DIR = ART / 'icons-1024'                  # originals as delivered by fal (not committed: ~1.5 MB each)
SMALL_DIR = ART / 'icons-96'
ATLAS_DIR = ART / 'atlas'
ATLAS_MAP = ATLAS_DIR / 'item_icons.json'

MODEL = 'claude-opus-5-5'
FALLBACK_BETA = 'server-side-fallback-2026-07-01'
FAL_MODEL = 'blackforestlabs/flux-3/klein/text-to-image'
TILE, SHEET = 96, 2048                        # 21 x 21 = 441 icons per 2048 sheet (power of two for WebGL)
# Text-token prices (USD per million) used only for the running estimate printed at the end.
PRICE = {'in': 4.0, 'out': 20.0, 'cache_write_1h': 8.0, 'cache_read': 0.20}

ICON_STYLE = (
    'Hand-painted inventory icon for a classic fantasy MMORPG, 1930s Hollywood Art Deco flavour. '
    'Subject: {subject}. '
    'A single object, centred, filling about 80 percent of a square frame, whole object visible, '
    'bold clean silhouette with a thin dark outline, chunky simplified forms that stay readable when tiny, '
    'confident painterly brush strokes, warm golden key light from the upper left, cool teal rim light, '
    'rich saturated colour, high contrast against a plain dark warm-brown radial vignette background. '
    'No text, no letters, no numbers, no logo, no border, no frame, no people, no hands.'
)

TEXT_SCHEMA = {
    'type': 'object',
    'properties': {'items': {'type': 'array', 'items': {
        'type': 'object',
        'properties': {'key': {'type': 'string'}, 'description': {'type': 'string'}, 'icon': {'type': 'string'}},
        'required': ['key', 'description', 'icon'], 'additionalProperties': False}}},
    'required': ['items'], 'additionalProperties': False,
}

_lock = threading.RLock()


def log(*a):
    with _lock:
        print(*a, flush=True)


def slug(s: str) -> str:
    return re.sub(r'[^a-z0-9]+', '_', s.lower()).strip('_')


def load_json(path: Path, default):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except FileNotFoundError:
        return default


def save_json(path: Path, obj, compact=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + '.tmp')
    text = json.dumps(obj, ensure_ascii=False, separators=(',', ':')) if compact else json.dumps(obj, ensure_ascii=False, indent=1)
    tmp.write_text(text, encoding='utf-8')
    os.replace(tmp, path)


# ---------------------------------------------------------------- items

def load_items():
    return json.loads(ITEMS.read_text(encoding='utf-8'))


def entries_from_items(items):
    """One entry per distinct item name, in file order. Returns (entries, id->key)."""
    by_key, id_to_key = {}, {}
    for it in items:
        key = slug(it['name'])
        id_to_key[it['id']] = key
        e = by_key.get(key)
        if e is None:
            e = by_key[key] = {'key': key, 'name': it['name'], 'kind': it['kind'], 'slot': it.get('slot', ''),
                               'type': it.get('type', ''), 'q': it.get('q', 1), 'base': it.get('base', ''),
                               'levels': [], 'stats': it.get('stats') or {}, 'ids': []}
            for f in ('blurb', 'use', 'named', 'hands', 'stack'):
                if it.get(f) not in (None, '', {}):
                    e[f] = it[f]
        e['ids'].append(it['id'])
        if it.get('req') is not None and it['req'] not in e['levels']:
            e['levels'].append(it['req'])
    for e in by_key.values():
        e['levels'].sort()
    return list(by_key.values()), id_to_key


def prompt_view(e):
    """The compact row Claude sees for one entry."""
    v = {'key': e['key'], 'name': e['name'], 'kind': e['kind']}
    for f in ('slot', 'type'):
        if e[f]:
            v[f] = e[f]
    v['q'] = e['q']
    lv = e['levels']
    if lv:
        v['levels'] = str(lv[0]) if len(lv) == 1 else f'{lv[0]}-{lv[-1]}'
    if e['stats']:
        v['stats'] = e['stats']
    for f in ('hands', 'blurb', 'use', 'named'):
        if f in e:
            v[f] = e[f]
    return v


def group_label(e):
    if e['base']:
        return e['base']
    if e.get('named'):
        return f"named_{'weapon' if e['kind'] == 'weapon' else 'armor'}"
    return f"{e['kind']}_{e['type']}"


def make_batches(entries, size):
    """Related items together: each base family (or loose category) stays in one batch where possible,
    and small families of the same kind/slot are packed together up to `size`."""
    groups, order = {}, []
    for e in entries:
        g = group_label(e)
        if g not in groups:
            groups[g] = []
            order.append(g)
        groups[g].append(e)
    batches, cur, cur_slot = [], [], None
    for g in order:
        members = groups[g]
        slot_sig = (members[0]['kind'], members[0]['slot'])
        if len(members) > size:
            if cur:
                batches.append(cur)
                cur = []
            n = -(-len(members) // size)
            per = -(-len(members) // n)
            batches += [members[i:i + per] for i in range(0, len(members), per)]
            continue
        if cur and (len(cur) + len(members) > size or slot_sig != cur_slot):
            batches.append(cur)
            cur = []
        cur += members
        cur_slot = slot_sig
    if cur:
        batches.append(cur)
    return batches


# ---------------------------------------------------------------- preamble

def build_preamble():
    """Fixed system prompt: the writer's brief plus world sections of the design bible. Byte-stable across runs
    (as long as the two files are unchanged), so it is written to the prompt cache once and read thereafter."""
    brief = (HERE / 'preamble.md').read_text(encoding='utf-8')
    bible = BIBLE.read_text(encoding='utf-8')
    keep = {3, 4, 5, 7, 8, 9, 10}
    parts = re.split(r'(?m)^(?=## \d+\. )', bible)
    picked = []
    for p in parts:
        m = re.match(r'## (\d+)\. ', p)
        if m and int(m.group(1)) in keep:
            p = re.sub(r'(?m)^&#91;embedded content.*$\n?', '', p)
            picked.append('#' + p.strip())   # demote to ### under the brief
    return brief.rstrip() + '\n\n' + '\n\n'.join(picked) + '\n'


def user_message(batch, n, total):
    fams = sorted({e['name'] if not e['base'] else e['base'].removeprefix('base_').replace('_', ' ') for e in batch})
    head = (f'Batch {n} of {total}: {len(batch)} items'
            + (f" (families: {', '.join(fams[:6])}{'...' if len(fams) > 6 else ''})" if len(fams) <= 12 else '')
            + '. Write one entry per item, same order, keys copied exactly.')
    rows = '\n'.join(json.dumps(prompt_view(e), ensure_ascii=False) for e in batch)
    return f'{head}\n\n{rows}'


def request_params(preamble, batch, n, total, effort):
    return dict(
        model=MODEL,
        max_tokens=32000,
        system=[{'type': 'text', 'text': preamble, 'cache_control': {'type': 'ephemeral', 'ttl': '1h'}}],
        output_config={'effort': effort, 'format': {'type': 'json_schema', 'schema': TEXT_SCHEMA}},
        messages=[{'role': 'user', 'content': user_message(batch, n, total)}],
    )


# ---------------------------------------------------------------- text step

class Usage:
    def __init__(self):
        self.t = {'in': 0, 'out': 0, 'cache_write_1h': 0, 'cache_read': 0}

    def add(self, u):
        with _lock:
            self.t['in'] += u.input_tokens or 0
            self.t['out'] += u.output_tokens or 0
            self.t['cache_write_1h'] += getattr(u, 'cache_creation_input_tokens', 0) or 0
            self.t['cache_read'] += getattr(u, 'cache_read_input_tokens', 0) or 0

    def report(self, discount=1.0):
        cost = sum(self.t[k] * PRICE[k] for k in self.t) / 1e6 * discount
        return ', '.join(f'{k} {v:,}' for k, v in self.t.items()) + f'  (~${cost:.2f} estimated)'


def accept(batch, data, texts, model_used):
    """Store the entries that came back for this batch; anything missing or empty stays pending."""
    want = {e['key']: e for e in batch}
    got = 0
    for row in (data or {}).get('items', []):
        k, d, ic = row.get('key'), (row.get('description') or '').strip(), (row.get('icon') or '').strip()
        if k in want and d and ic:
            texts[k] = {'name': want[k]['name'], 'description': d, 'icon': ic, 'model': model_used,
                        'at': time.strftime('%Y-%m-%d %H:%M:%S')}
            got += 1
            words = len(d.split())
            if words > 70:
                log(f'  note: {k} description is {words} words')
    if got < len(want):
        log(f'  {len(want) - got} item(s) missing from the reply; they stay pending: '
            + ', '.join(k for k in want if k not in texts))
    return got


def parse_message(msg):
    if msg.stop_reason == 'refusal':
        cat = getattr(getattr(msg, 'stop_details', None), 'category', None)
        raise RuntimeError(f'refused (category {cat})')
    if msg.stop_reason == 'max_tokens':
        raise RuntimeError('hit max_tokens')
    text = ''.join(b.text for b in msg.content if b.type == 'text')
    return json.loads(text)


def text_sync(batches, preamble, texts, effort, workers):
    import anthropic
    client = anthropic.Anthropic(max_retries=4)
    usage = Usage()
    total = len(batches)

    def run(i, batch):
        params = request_params(preamble, batch, i + 1, total, effort)
        try:
            with client.beta.messages.stream(betas=[FALLBACK_BETA], fallbacks='default', **params) as s:
                msg = s.get_final_message()
            usage.add(msg.usage)
            data = parse_message(msg)
        except (anthropic.AuthenticationError, anthropic.PermissionDeniedError, anthropic.BadRequestError):
            raise   # a bad key or a bad request will not fix itself: stop the run
        except (anthropic.APIStatusError, anthropic.APIConnectionError, RuntimeError, json.JSONDecodeError) as ex:
            log(f'batch {i + 1}/{total}: failed ({type(ex).__name__}: {str(ex)[:200]}); will retry on the next run')
            return
        with _lock:
            got = accept(batch, data, texts, msg.model)
            save_json(TEXT_STATE, texts)
        log(f'batch {i + 1}/{total}: {got}/{len(batch)} written  [cache read {msg.usage.cache_read_input_tokens or 0:,}]')

    if not batches:
        return usage
    run(0, batches[0])   # first request alone, so it writes the cache that the parallel ones then read
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for f in as_completed([pool.submit(run, i, b) for i, b in enumerate(batches) if i > 0]):
            f.result()
    return usage


def text_batch_api(batches, preamble, texts, effort, poll, by_key):
    """Message Batches API: half price, asynchronous (usually minutes, at most 24 h). Resumable: the batch id and
    its request -> keys map are saved before waiting, and a rerun picks the same batch up again."""
    import anthropic
    client = anthropic.Anthropic(max_retries=4)
    pending = load_json(PENDING_BATCH, None)
    if pending is None:
        if not batches:
            return Usage()
        total = len(batches)
        reqs, mapping = [], {}
        for i, b in enumerate(batches):
            cid = f'b{i + 1:04d}'
            mapping[cid] = [e['key'] for e in b]
            reqs.append({'custom_id': cid, 'params': request_params(preamble, b, i + 1, total, effort)})
        mb = client.messages.batches.create(requests=reqs)
        pending = {'id': mb.id, 'map': mapping, 'created': time.strftime('%Y-%m-%d %H:%M:%S')}
        save_json(PENDING_BATCH, pending)
        log(f'submitted message batch {mb.id} with {len(reqs)} requests')
    else:
        log(f"resuming message batch {pending['id']} ({len(pending['map'])} requests)")
    while True:
        mb = client.messages.batches.retrieve(pending['id'])
        if mb.processing_status == 'ended':
            break
        c = mb.request_counts
        log(f'  {mb.processing_status}: {c.processing} processing, {c.succeeded} done, {c.errored} errored')
        time.sleep(poll)
    usage = Usage()
    for r in client.messages.batches.results(pending['id']):
        keys = pending['map'].get(r.custom_id, [])
        batch = [by_key[k] for k in keys if k in by_key]
        if r.result.type != 'succeeded':
            log(f'{r.custom_id}: {r.result.type}; will retry on the next run')
            continue
        msg = r.result.message
        usage.add(msg.usage)
        try:
            data = parse_message(msg)
        except (RuntimeError, json.JSONDecodeError) as ex:
            log(f'{r.custom_id}: {ex}; will retry on the next run')
            continue
        got = accept(batch, data, texts, msg.model)
        log(f'{r.custom_id}: {got}/{len(batch)} written')
    save_json(TEXT_STATE, texts)
    PENDING_BATCH.unlink()
    return usage


def step_text(args, entries):
    texts = load_json(TEXT_STATE, {})
    todo = select(entries, texts, args)
    batches = make_batches(todo, args.batch_size)
    if args.max_batches:
        batches = batches[:args.max_batches]
    log(f'text: {len(texts)} of {len(entries)} written; {sum(map(len, batches))} to do in {len(batches)} request(s)')
    if PENDING_BATCH.exists() and not args.batch_api:
        sys.exit(f'A Message Batch is still in flight ({PENDING_BATCH}); rerun with --batch-api to collect it first.')
    if PENDING_BATCH.exists():
        log('a submitted Message Batch is in flight; collecting it (new work waits for the next run)')
    elif not batches:
        return
    need_env('ANTHROPIC_API_KEY')
    preamble = build_preamble()
    if args.batch_api:
        usage = text_batch_api(batches, preamble, texts, args.effort, args.poll, {e['key']: e for e in entries})
        log('text usage: ' + usage.report(0.5))
    else:
        usage = text_sync(batches, preamble, texts, args.effort, args.workers)
        log('text usage: ' + usage.report())
    left = sum(1 for e in entries if e['key'] not in texts)
    log(f'text: {len(entries) - left}/{len(entries)} written' + (f', {left} pending (rerun to continue)' if left else ''))


# ---------------------------------------------------------------- icons step

def fal_headers():
    return {'Authorization': f"Key {os.environ['FAL_KEY']}", 'Content-Type': 'application/json'}


def icon_seed(key, attempt):
    return int(hashlib.sha256(f'{key}:{attempt}'.encode()).hexdigest()[:8], 16) & 0x7FFFFFFF


def make_icon(key, brief, icons, http):
    """Submit (or resume) one fal queue request and download the image. State is saved after submit, so an
    interrupted run polls the same request instead of paying for a new one."""
    st = icons.setdefault(key, {'attempt': 0})
    out = SRC_DIR / f'{key}.png'
    prompt = ICON_STYLE.format(subject=brief.rstrip('. '))
    if not st.get('request_id'):
        st['seed'] = icon_seed(key, st['attempt'])
        st['prompt'] = prompt
        body = {'prompt': prompt, 'image_size': {'width': 1024, 'height': 1024}, 'num_images': 1,
                'seed': st['seed'], 'enable_prompt_expansion': False, 'output_format': 'png',
                'enable_safety_checker': True}
        r = http.post(f'https://queue.fal.run/{FAL_MODEL}', headers=fal_headers(), json=body, timeout=60)
        r.raise_for_status()
        j = r.json()
        st.update(request_id=j['request_id'], status_url=j['status_url'], response_url=j['response_url'], status='queued')
        with _lock:
            save_json(ICON_STATE, icons)
    t0 = time.time()
    while True:
        r = http.get(st['status_url'], headers=fal_headers(), timeout=60)
        if r.status_code == 404:
            raise RuntimeError('request expired on fal; will resubmit')
        r.raise_for_status()
        j = r.json()
        if j.get('status') == 'COMPLETED':
            if j.get('error'):
                raise RuntimeError(f"fal error: {j.get('error_type')}: {j['error']}")
            break
        if time.time() - t0 > 600:
            raise TimeoutError('still queued after 10 minutes; rerun to keep waiting')
        time.sleep(2)
    r = http.get(st['response_url'], headers=fal_headers(), timeout=60)
    r.raise_for_status()
    res = r.json()
    if any(res.get('has_nsfw_concepts') or []):
        raise RuntimeError('safety checker flagged the image; retrying with a new seed next run')
    img = http.get(res['images'][0]['url'], timeout=120)
    img.raise_for_status()
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.part')
    tmp.write_bytes(img.content)
    os.replace(tmp, out)
    st.update(status='done', file=out.name, brief=brief, request_id=None, at=time.strftime('%Y-%m-%d %H:%M:%S'))
    with _lock:
        save_json(ICON_STATE, icons)


def step_icons(args, entries):
    import requests
    texts = load_json(TEXT_STATE, {})
    icons = load_json(ICON_STATE, {})
    redo = set(args.redo.split(',')) if args.redo and args.redo != 'all' else set()
    todo = []
    for e in entries:
        k = e['key']
        if args.only and k not in args.only.split(','):
            continue
        if k not in texts:
            continue
        st = icons.get(k, {})
        done = (SRC_DIR / f'{k}.png').exists() and st.get('status') == 'done'
        brief_changed = done and st.get('brief') != texts[k]['icon']
        if args.redo == 'all' or k in redo or brief_changed:
            if done:
                icons[k] = {'attempt': st.get('attempt', 0) + 1}
            done = False
        if not done:
            todo.append(k)
    if args.max_icons:
        todo = todo[:args.max_icons]
    no_text = sum(1 for e in entries if e['key'] not in texts)
    log(f'icons: {len(todo)} to make' + (f' ({no_text} item(s) still need text first)' if no_text else ''))
    if not todo:
        return
    need_env('FAL_KEY')
    http = requests.Session()
    ok = fail = 0

    def run(k):
        try:
            make_icon(k, texts[k]['icon'], icons, http)
            return k, None
        except Exception as ex:   # one bad image must not stop the run; it is retried next time
            with _lock:
                st = icons.setdefault(k, {'attempt': 0})
                if 'expired' in str(ex) or 'flagged' in str(ex) or 'fal error' in str(ex):
                    st['request_id'] = None
                    st['attempt'] = st.get('attempt', 0) + 1
                st['status'] = 'failed'
                st['error'] = str(ex)[:300]
                save_json(ICON_STATE, icons)
            return k, ex

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for i, f in enumerate(as_completed([pool.submit(run, k) for k in todo]), 1):
            k, ex = f.result()
            if ex:
                fail += 1
                log(f'  [{i}/{len(todo)}] {k}: FAILED {str(ex)[:160]}')
            else:
                ok += 1
                if i % 10 == 0 or i == len(todo):
                    log(f'  [{i}/{len(todo)}] {k}')
    log(f'icons: {ok} made, {fail} failed' + (' (rerun to retry)' if fail else ''))


# ---------------------------------------------------------------- atlas step

def downsample(src: Path, dst: Path):
    from PIL import Image, ImageFilter
    with Image.open(src) as im:
        im = im.convert('RGB')
        # Lanczos in stages keeps edges crisp without ringing; a light unsharp mask restores the outline at small sizes.
        im = im.resize((384, 384), Image.LANCZOS).resize((TILE, TILE), Image.LANCZOS)
        im = im.filter(ImageFilter.UnsharpMask(radius=0.8, percent=70, threshold=2))
        dst.parent.mkdir(parents=True, exist_ok=True)
        im.save(dst, optimize=True)


def step_atlas(args, entries, id_to_key):
    from PIL import Image
    amap = load_json(ATLAS_MAP, {})
    slots = {k: v['index'] for k, v in amap.get('icons', {}).items()}
    made = 0
    for e in entries:   # item-file order; slots are append-only so existing positions never move
        k = e['key']
        src, dst = SRC_DIR / f'{k}.png', SMALL_DIR / f'{k}.png'
        if not src.exists():
            continue
        if not dst.exists() or dst.stat().st_mtime < src.stat().st_mtime:
            downsample(src, dst)
            made += 1
        if k not in slots:
            slots[k] = len(slots)
    per_row = SHEET // TILE
    per_sheet = per_row * per_row
    n_sheets = -(-len(slots) // per_sheet) if slots else 0
    sheets = [Image.new('RGB', (SHEET, SHEET), (0, 0, 0)) for _ in range(n_sheets)]
    icons = {}
    for k, idx in sorted(slots.items(), key=lambda kv: kv[1]):
        s, cell = divmod(idx, per_sheet)
        x, y = (cell % per_row) * TILE, (cell // per_row) * TILE
        p = SMALL_DIR / f'{k}.png'
        if p.exists():
            with Image.open(p) as im:
                sheets[s].paste(im.convert('RGB'), (x, y))
        icons[k] = {'index': idx, 'sheet': s, 'x': x, 'y': y}
    ATLAS_DIR.mkdir(parents=True, exist_ok=True)
    names = []
    for s, im in enumerate(sheets):
        name = f'item_icons_{s}.png'
        im.save(ATLAS_DIR / name, optimize=True)
        # the game ships a JPEG copy (about a fifth of the size; the icons sit on black, so nothing needs alpha)
        im.save(ATLAS_DIR / name.replace('.png', '.jpg'), quality=92, subsampling=0, optimize=True)
        names.append(name)
    items_map = {iid: k for iid, k in id_to_key.items() if k in icons}
    save_json(ATLAS_MAP, {'tile': TILE, 'sheet_size': SHEET, 'columns': per_row, 'per_sheet': per_sheet,
                          'sheets': names, 'icons': icons, 'items': items_map})
    log(f'atlas: {made} icon(s) downsampled; {len(icons)} icons on {len(names)} sheet(s); {len(items_map)} item ids mapped')


# ---------------------------------------------------------------- apply step

def step_apply(args, items_doc, id_to_key):
    """Adds `desc` (tooltip text) and `icon` (global atlas index; sheet = icon // per_sheet) to each item row.
    Every other field and the row order are left exactly as they were."""
    texts = load_json(TEXT_STATE, {})
    amap = load_json(ATLAS_MAP, {})
    icons = amap.get('icons', {})
    nd = ni = 0
    for it in items_doc['items']:
        k = id_to_key[it['id']]
        if k in texts:
            it['desc'] = texts[k]['description']
            nd += 1
        if k in icons and (SMALL_DIR / f'{k}.png').exists():
            it['icon'] = icons[k]['index']
            ni += 1
    save_json(ITEMS, items_doc, compact=True)
    log(f'apply: items.json updated: {nd}/{len(items_doc["items"])} with desc, {ni} with icon')


# ---------------------------------------------------------------- review page

def step_review(args, entries):
    texts = load_json(TEXT_STATE, {})
    qcol = {0: '#9d9d9d', 1: '#ffffff', 2: '#1eff00', 3: '#3a8cff', 4: '#b65cff'}
    rows = []
    for e in entries:
        k = e['key']
        if k not in texts and not (SRC_DIR / f'{k}.png').exists():
            continue
        t = texts.get(k, {})
        small = f'icons-96/{k}.png' if (SMALL_DIR / f'{k}.png').exists() else ''
        big = f'icons-1024/{k}.png' if (SRC_DIR / f'{k}.png').exists() else ''
        img = (f'<a href="{big}"><img src="{small or big}" width="96" height="96" alt=""></a>'
               f'<img src="{small or big}" width="32" height="32" alt="">') if (small or big) else '<div class="none"></div>'
        rows.append(f'<div class="row">{img}<div><b style="color:{qcol.get(e["q"], "#fff")}">{html.escape(e["name"])}</b>'
                    f' <span class="k">{html.escape(k)} · {e["kind"]} {html.escape(e["slot"])}</span>'
                    f'<p>{html.escape(t.get("description", ""))}</p><p class="ic">{html.escape(t.get("icon", ""))}</p></div></div>')
    page = ('<!doctype html><meta charset="utf-8"><title>Item review</title><style>'
            'body{background:#16120e;color:#e8dcc8;font:14px/1.4 Georgia,serif;margin:16px}'
            '.row{display:flex;gap:12px;padding:8px 0;border-bottom:1px solid #3a3026}'
            '.row img{image-rendering:auto;border:1px solid #5a4a36;flex:none}.row p{margin:4px 0}'
            '.k{color:#8a7a66;font-size:12px}.ic{color:#9a8a76;font-size:12px;font-style:italic}'
            '.none{width:96px;height:96px;background:#2a221a;flex:none}</style>'
            f'<h1>Hollywoodland items ({len(rows)})</h1>' + ''.join(rows))
    ART.mkdir(parents=True, exist_ok=True)
    (ART / 'review.html').write_text(page, encoding='utf-8')
    log(f'review: {ART / "review.html"} ({len(rows)} items)')


# ---------------------------------------------------------------- plan

def step_plan(args, entries):
    texts = load_json(TEXT_STATE, {})
    todo = select(entries, texts, args)
    batches = make_batches(todo, args.batch_size)
    preamble = build_preamble()
    have_icons = sum(1 for e in entries if (SRC_DIR / f"{e['key']}.png").exists())
    log(f'{len(entries)} distinct items (from {sum(len(e["ids"]) for e in entries)} item ids)')
    log(f'text: {len(texts)} done, {len(todo)} to do in {len(batches)} request(s) of up to {args.batch_size}')
    log(f'icons: {have_icons} done, {len(entries) - have_icons} to do')
    log(f'preamble: {len(preamble):,} chars (~{len(preamble) // 4:,} tokens), cached for 1 h after the first request')
    sizes = {}
    for b in batches:
        sizes[len(b)] = sizes.get(len(b), 0) + 1
    log('batch sizes: ' + ', '.join(f'{n} x {s}' for s, n in sorted(sizes.items(), reverse=True)))
    if batches:
        STATE.mkdir(parents=True, exist_ok=True)
        (STATE / 'sample_request.txt').write_text(
            '=== SYSTEM (cached) ===\n' + preamble + '\n=== USER ===\n' + user_message(batches[0], 1, len(batches)),
            encoding='utf-8')
        log(f'sample request: {STATE / "sample_request.txt"}')
    log('keys: ANTHROPIC_API_KEY ' + ('set' if os.environ.get('ANTHROPIC_API_KEY') else 'NOT set')
        + ', FAL_KEY ' + ('set' if os.environ.get('FAL_KEY') else 'NOT set'))


# ---------------------------------------------------------------- main

def select(entries, texts, args):
    only = set(args.only.split(',')) if args.only else None
    redo = set(args.redo.split(',')) if args.redo and args.redo != 'all' else set()
    out = []
    for e in entries:
        k = e['key']
        if only is not None and k not in only:
            continue
        if args.redo == 'all' or k in redo or k not in texts:
            out.append(e)
    return out


def need_env(name):
    if not os.environ.get(name):
        sys.exit(f'{name} is not set. Set it in the environment for this shell, then rerun.')


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('step', choices=['plan', 'text', 'icons', 'atlas', 'apply', 'review', 'all'])
    ap.add_argument('--batch-api', action='store_true', help='text via the Message Batches API (half price, async)')
    ap.add_argument('--batch-size', type=int, default=24, help='items per Claude request (default 24)')
    ap.add_argument('--max-batches', type=int, default=0, help='stop after N text requests (pilot runs)')
    ap.add_argument('--max-icons', type=int, default=0, help='stop after N icons (pilot runs)')
    ap.add_argument('--effort', default='medium', choices=['low', 'medium', 'high', 'xhigh', 'max'])
    ap.add_argument('--workers', type=int, default=4, help='parallel requests (default 4)')
    ap.add_argument('--poll', type=int, default=30, help='seconds between Message Batch status checks')
    ap.add_argument('--only', help='comma-separated keys to limit the step to')
    ap.add_argument('--redo', help="comma-separated keys to regenerate, or 'all'")
    args = ap.parse_args()

    items_doc = load_items()
    entries, id_to_key = entries_from_items(items_doc['items'])
    steps = ['text', 'icons', 'atlas', 'apply', 'review'] if args.step == 'all' else [args.step]
    for s in steps:
        if s == 'plan':
            step_plan(args, entries)
        elif s == 'text':
            step_text(args, entries)
        elif s == 'icons':
            step_icons(args, entries)
        elif s == 'atlas':
            step_atlas(args, entries, id_to_key)
        elif s == 'apply':
            step_apply(args, items_doc, id_to_key)
        elif s == 'review':
            step_review(args, entries)


if __name__ == '__main__':
    main()
