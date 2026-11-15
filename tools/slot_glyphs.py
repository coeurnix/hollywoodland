"""Small UI art: empty equipment-slot glyphs for the character window: a gold line-art silhouette per slot on near-black, drawn
faintly inside an empty slot so it says what goes there. FLUX 3 Klein; writes hollywoodland-ue/Content/Data/UI/slot_<id>.jpg.

Run: python -I tools/slot_glyphs.py
"""
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent / 'charv2'))
import genlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'hollywoodland-ue' / 'Content' / 'Data' / 'UI'
SRC = ROOT / 'tmp' / 'slot_glyphs'
STYLE = ('A single simple icon: {what}, drawn as clean engraved gold line art (thin even strokes, art deco 1930), centred, '
         'filling about two thirds of the frame, flat front view, on a plain solid near-black background. No text, no frame, '
         'no shading, no gradient, no other objects.')
SLOTS = {
    'hat': 'a fedora hat', 'neck': 'a pendant on a fine chain necklace', 'coat': 'a buttoned jacket with lapels',
    'wrist': 'a wristwatch', 'gloves': 'a pair of gloves', 'belt': 'a belt with a square buckle',
    'trousers': 'a pair of trousers', 'shoes': 'a pair of lace-up shoes', 'ring': 'a signet ring',
    'main': 'a sabre held point up', 'off': 'a round shield', 'ranged': 'a revolver pistol',
}


# mount buff icons in the item icons' house style (tools/itemgen ICON_STYLE)
ICON = ('Hand-painted inventory icon for a classic fantasy MMORPG, 1930s Hollywood Art Deco flavour. Subject: {what}. '
        'A single subject, centred, filling about 80 percent of a square frame, bold clean silhouette with a thin dark outline, '
        'chunky simplified forms that stay readable when tiny, confident painterly brush strokes, warm golden key light from the '
        'upper left, cool teal rim light, rich saturated colour, high contrast against a plain dark warm-brown radial vignette '
        'background. No text, no frame.')
MOUNTS = {'mount_horse': 'the head and neck of a bay mare in a polished leather bridle, a white blaze down her nose, three-quarter view',
          'mount_scout': 'a red 1928 Indian Scout motorcycle with a chrome headlamp, three-quarter front view',
          'mount_modela': 'a black 1931 Ford Model A Tudor sedan, three-quarter front view'}


def main():
    genlib.load_env()
    st = genlib.State(SRC / 'state.json')
    SRC.mkdir(parents=True, exist_ok=True)

    def one(k):
        p = genlib.flux(k, STYLE.format(what=SLOTS[k]), SRC / f'{k}.png', st.get(k), 768, 768)
        im = Image.open(p).convert('RGB').resize((128, 128), Image.LANCZOS)
        im.save(OUT / f'slot_{k}.jpg', quality=90)
        return k

    def mount(k):
        p = genlib.flux(k, ICON.format(what=MOUNTS[k]), SRC / f'{k}.png', st.get(k), 768, 768)
        Image.open(p).convert('RGB').resize((128, 128), Image.LANCZOS).save(OUT / f'{k}.jpg', quality=90)
        return k

    with ThreadPoolExecutor(6) as ex:
        for k in ex.map(mount, MOUNTS):
            print('ok', k, flush=True)
    with ThreadPoolExecutor(6) as ex:
        for k in ex.map(one, SLOTS):
            print('ok', k, flush=True)
    st.save()


if __name__ == '__main__':
    main()
