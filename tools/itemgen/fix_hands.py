#!/usr/bin/env python3
"""Check icons that show hands or gloves for the wrong number of fingers, and regenerate the ones that fail.

    python tools/itemgen/fix_hands.py --slot gloves            # check, regenerate failures, re-check (up to --rounds)
    python tools/itemgen/fix_hands.py --keys a,b --check-only

A vision check (Claude) looks at each 1024 original. Failures get a tighter brief and a new seed, then go back
through the check. Results are kept in state/hands.json, so a rerun only looks at what changed.
Run `itemgen.py atlas` and `apply` afterwards.
"""
import argparse
import base64
import io
import json
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import itemgen as ig  # noqa: E402

HANDS_STATE = ig.STATE / 'hands.json'
CHECK_SCHEMA = {'type': 'object', 'additionalProperties': False, 'required': ['ok', 'reason'],
                'properties': {'ok': {'type': 'boolean'}, 'reason': {'type': 'string'}}}
CHECK_PROMPT = (
    'This is an inventory icon of a glove or a pair of gloves. Look closely at every glove and hand. '
    'Each one must show exactly one thumb and four fingers (or, for a mitten or a clenched fist, a believable shape '
    'with no extra or missing digits). Fingers hidden by folding or overlap are fine if the visible shape is '
    'plausible. Fail it for any extra finger, missing finger, two thumbs, fused or melted digits, or a glove '
    'that could not be worn by a human hand. Say ok=true only if you are confident it is anatomically right.')
FIX = ('; shown as ONE single glove lying flat, back of the hand facing the viewer, fingers pointing up, '
       'the thumb clearly separate on one side, exactly four fingers and one thumb, anatomically correct human glove')


def thumb_b64(path):
    from PIL import Image
    with Image.open(path) as im:
        im = im.convert('RGB').resize((512, 512), Image.LANCZOS)
        buf = io.BytesIO()
        im.save(buf, 'JPEG', quality=90)
    return base64.b64encode(buf.getvalue()).decode()


def check(client, key, effort='low'):
    msg = client.messages.create(
        model=ig.MODEL, max_tokens=2000,
        output_config={'effort': effort, 'format': {'type': 'json_schema', 'schema': CHECK_SCHEMA}},
        messages=[{'role': 'user', 'content': [
            {'type': 'image', 'source': {'type': 'base64', 'media_type': 'image/jpeg', 'data': thumb_b64(ig.SRC_DIR / f'{key}.png')}},
            {'type': 'text', 'text': CHECK_PROMPT}]}])
    return json.loads(''.join(b.text for b in msg.content if b.type == 'text'))


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser()
    ap.add_argument('--slot', help='item slot to check (e.g. gloves)')
    ap.add_argument('--keys', help='comma-separated keys')
    ap.add_argument('--rounds', type=int, default=4)
    ap.add_argument('--workers', type=int, default=8)
    ap.add_argument('--check-only', action='store_true')
    ap.add_argument('--recheck', action='store_true', help='check every icon again, even ones that passed')
    ap.add_argument('--effort', default='low', choices=['low', 'medium', 'high'])
    args = ap.parse_args()
    ig.need_env('ANTHROPIC_API_KEY')
    import anthropic
    import requests
    client = anthropic.Anthropic(max_retries=4)
    items = ig.load_items()['items']
    keys = set(args.keys.split(',')) if args.keys else set()
    if args.slot:
        keys |= {ig.slug(i['name']) for i in items if i.get('slot') == args.slot}
    keys = sorted(k for k in keys if (ig.SRC_DIR / f'{k}.png').exists())
    texts = ig.load_json(ig.TEXT_STATE, {})
    icons = ig.load_json(ig.ICON_STATE, {})
    hands = ig.load_json(HANDS_STATE, {})
    http = requests.Session()
    todo = [k for k in keys if args.recheck or hands.get(k, {}).get('seed') != icons.get(k, {}).get('seed') or not hands[k].get('ok')]
    ig.log(f'hands: {len(keys)} icons, {len(todo)} to check')
    for rnd in range(args.rounds):
        if not todo:
            break
        bad = []
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            futs = {pool.submit(check, client, k, args.effort): k for k in todo}
            for f in as_completed(futs):
                k = futs[f]
                try:
                    r = f.result()
                except Exception as ex:
                    ig.log(f'  {k}: check failed ({str(ex)[:120]})'); continue
                hands[k] = {'ok': r['ok'], 'reason': r['reason'], 'seed': icons.get(k, {}).get('seed')}
                if not r['ok']:
                    bad.append(k)
                    ig.log(f'  {k}: FAIL {r["reason"][:140]}')
        ig.save_json(HANDS_STATE, hands)
        ig.log(f'round {rnd + 1}: {len(todo) - len(bad)} ok, {len(bad)} wrong')
        if args.check_only or not bad:
            break
        # regenerate: tighter brief (also stored as the item's brief so the icons step agrees), new seed
        for k in bad:
            if FIX not in texts[k]['icon']:
                texts[k]['icon'] = texts[k]['icon'].replace('a pair of ', 'a ').replace('pair of ', '').rstrip('. ') + FIX
            icons[k] = {'attempt': icons.get(k, {}).get('attempt', 0) + 1}
        ig.save_json(ig.TEXT_STATE, texts)
        ig.save_json(ig.ICON_STATE, icons)
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            for f in as_completed([pool.submit(ig.make_icon, k, texts[k]['icon'], icons, http) for k in bad]):
                try:
                    f.result()
                except Exception as ex:
                    ig.log(f'  regenerate failed: {str(ex)[:120]}')
        todo = bad
    left = [k for k in keys if not hands.get(k, {}).get('ok')]
    ig.log(f'hands: {len(keys) - len(left)}/{len(keys)} pass' + (f'; still wrong: {", ".join(left)}' if left else ''))


if __name__ == '__main__':
    main()
