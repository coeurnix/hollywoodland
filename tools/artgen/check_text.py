#!/usr/bin/env python3
"""Regenerate an interface image until a vision check finds no lettering (image models garble text).

    python tools/artgen/check_text.py ui_title [--tries 5]
"""
import argparse
import base64
import io
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import artgen as ag  # noqa: E402

SCHEMA = {'type': 'object', 'additionalProperties': False, 'required': ['has_text', 'where'],
          'properties': {'has_text': {'type': 'boolean'}, 'where': {'type': 'string'}}}


def has_text(path):
    import anthropic
    from PIL import Image
    with Image.open(path) as im:
        im = im.convert('RGB')
        im.thumbnail((1280, 1280))
        buf = io.BytesIO()
        im.save(buf, 'JPEG', quality=90)
    msg = anthropic.Anthropic(max_retries=4).messages.create(
        model=ag.MODEL, max_tokens=2000,
        output_config={'effort': 'medium', 'format': {'type': 'json_schema', 'schema': SCHEMA}},
        messages=[{'role': 'user', 'content': [
            {'type': 'image', 'source': {'type': 'base64', 'media_type': 'image/jpeg', 'data': base64.b64encode(buf.getvalue()).decode()}},
            {'type': 'text', 'text': 'Does this illustration contain any letters, words, numbers or letter-like glyphs anywhere '
                                     '(signs, marquees, posters, vehicles, banners)? Even tiny or garbled ones count.'}]}])
    return json.loads(''.join(b.text for b in msg.content if b.type == 'text'))


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser()
    ap.add_argument('key')
    ap.add_argument('--tries', type=int, default=5)
    args = ap.parse_args()
    ag.load_env()
    import requests
    w, h, prompt = ag.UI_ART[args.key]
    images = ag.load_json(ag.IMAGE_STATE, {})
    src = ag.SRC_DIR / f'{args.key}.png'
    for i in range(args.tries):
        if src.exists() and images.get(args.key, {}).get('prompt') == prompt and images[args.key].get('status') == 'done':
            r = has_text(src)
            ag.log(f'{args.key}: text={r["has_text"]} {r["where"][:160]}')
            if not r['has_text']:
                ag.log('clean'); return
        images[args.key] = {'attempt': images.get(args.key, {}).get('attempt', 0) + 1}
        ag.make_image(args.key, prompt, w, h, images, requests.Session())
    ag.log('gave up; the last image stays (rerun for more tries)')


if __name__ == '__main__':
    main()
