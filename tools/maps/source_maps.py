"""Draw accurate layout sources for the painted maps (art/maps/src/*.png) from the game data.

    python tools/maps/source_maps.py            then  python tools/maps/paint_maps.py   (Codex imagegen)

Each source is 1536 x 1024 (or 1024 x 1536 portrait): the playable area sits in an exact rectangle (recorded in
art/maps/maps.json with the area bounds) so the painted result registers back to game coordinates; the rest of the
canvas is margin for a title cartouche and compass. Only data the game uses is drawn: roads, building footprints,
the hive wall, district names, landmark labels, street names, exits.
"""
import json
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data')
OUT = os.path.join(ROOT, 'art', 'maps')
FONTS = os.path.join(DATA, 'Fonts')


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def rgb(c, k=1.0):
    return tuple(int(max(0, min(1, v * k)) * 255) for v in c[:3])


# street names: the zone's grid (tools/gen/zone.mjs strips) named after the real 1930 streets in the same order
STREETS_EW = [('Franklin Avenue', 26), ('Hollywood Boulevard', 200), ('Selma Avenue', 306), ('Sunset Boulevard', 428)]
STREETS_NS = [('Orange Dr.', 18), ('Orchid Ave.', 145), ('Highland Avenue', 298), ('McCadden Pl.', 425), ('Wilcox Avenue', 525),
              ('Cahuenga Blvd.', 616), ('Vine Street', 758)]
OUTER = {'Cahuenga Pass': (290, -240), 'Melrose': (300, 680), 'West Hollywood': (-230, 300), 'Gower Gulch': (1030, 300)}
SHOPS = {'Max Factor Salon', 'Hollywood Gun Shop', 'Haberdashery', 'Drugstore'}


class Canvas:
    def __init__(self, size, rect, amin, asize):
        """rect: pixel rectangle (x, y, w, h) that shows the area amin..amin+asize exactly (same scale on both axes)."""
        self.im = Image.new('RGB', size, (214, 200, 168))
        self.d = ImageDraw.Draw(self.im)
        self.rect, self.amin, self.asize = rect, amin, asize
        self.k = rect[2] / asize[0]

    def p(self, x, y):
        return (self.rect[0] + (x - self.amin[0]) * self.k, self.rect[1] + (y - self.amin[1]) * self.k)

    def box(self, x, y, w, d, fill, outline=None, width=1):
        a, b = self.p(x, y), self.p(x + w, y + d)
        self.d.rectangle([a, b], fill=fill, outline=outline, width=width)

    def text(self, x, y, s, f, fill=(30, 22, 12), anchor='mm', stroke=0, stroke_fill=(240, 230, 205), angle=0):
        px = self.p(x, y)
        if angle:
            w = int(self.d.textlength(s, font=f)) + 8
            h = f.size + 8
            t = Image.new('RGBA', (w, h), (0, 0, 0, 0))
            ImageDraw.Draw(t).text((w / 2, h / 2), s, font=f, fill=fill, anchor='mm', stroke_width=stroke, stroke_fill=stroke_fill)
            t = t.rotate(angle, expand=True)
            self.im.paste(t, (int(px[0] - t.width / 2), int(px[1] - t.height / 2)), t)
        else:
            self.d.text(px, s, font=f, fill=fill, anchor=anchor, stroke_width=stroke, stroke_fill=stroke_fill)


def zone_source(area, out):
    W, H = 1536, 1024
    amin, asize = area['min'], area['size']
    k = H / asize[1]
    rect = (0, 0, asize[0] * k, H)
    c = Canvas((W, H), rect, amin, asize)
    c.box(amin[0], amin[1], asize[0], asize[1], rgb(area['floor'], 1.25))
    for g in area['ground']:
        if g['d'] < 1 or g['w'] < 1:
            continue
        col = g['color']
        fill = (88, 64, 92) if col == [0.2, 0.15, 0.2] else (70, 66, 62) if col == [0.13, 0.13, 0.14] else rgb(col, 1.4)
        c.box(g['x'], g['y'], g['w'], g['d'], fill)
    for b in area['boxes']:
        col = b['color']
        if b.get('h', 0) < 1:
            continue
        if col == [0.22, 0.12, 0.26]:
            fill = (112, 52, 122)               # hive resin
        elif col == [0.72, 0.62, 0.42]:
            fill = (196, 160, 98)               # landmark
        elif col == [0.6, 0.6, 0.62]:
            fill = (176, 176, 184)              # civic
        elif col == [0.2, 0.22, 0.25]:
            fill = (60, 64, 72)                 # wreck / car
        else:
            fill = rgb(col, 1.45)
        c.box(b['x'], b['y'], b['w'], b['d'], fill, outline=(40, 30, 20))
    # hive wall ring (the playable edge) as a bold band
    big, mid, small, tiny = font('Limelight-Regular.ttf', 30), font('Limelight-Regular.ttf', 17), font('AlegreyaSans-Bold.ttf', 15), font('AlegreyaSans-Bold.ttf', 12)
    for name, y in STREETS_EW:
        for x in (60, 360, 680):
            c.text(x, y, name.upper(), tiny, fill=(245, 238, 220))
    for name, x in STREETS_NS:
        for y in (90, 380):
            c.text(x, y, name.upper(), tiny, fill=(245, 238, 220), angle=90)
    for s in area['subzones']:
        if s['name'] in ('The Hive Wall',) or s['name'] in OUTER:
            continue
        c.text(s['x'] + s['w'] / 2, s['y'] + 14, s['name'], small, fill=(70, 40, 20), stroke=2)
    for b in area['boxes']:
        lab = b.get('label')
        if not lab or lab in ('Hollywood Hotel Ballroom',):
            continue
        f = tiny if lab in SHOPS else mid if b['w'] * b['d'] > 1500 else small
        c.text(b['x'] + b['w'] / 2, b['y'] + b['d'] / 2, lab, f, fill=(25, 18, 10), stroke=2)
    for name, (x, y) in OUTER.items():
        c.text(x, y, name, big, fill=(235, 220, 240), stroke=3, stroke_fill=(60, 30, 70))
    c.text(-123, 225, 'THE HIVE WALL', small, fill=(240, 220, 245), angle=90, stroke=2, stroke_fill=(60, 30, 70))
    for e in area.get('exits', []):
        if e.get('redcar'):
            continue
    # margin: title block placeholder
    d = c.d
    d.rectangle([rect[2] + 8, 8, W - 8, H - 8], outline=(90, 70, 40), width=3)
    d.text(((rect[2] + W) / 2, 120), 'HOLLYWOOD\nBOULEVARD', font=font('Limelight-Regular.ttf', 26), fill=(60, 40, 20), anchor='mm', align='center')
    d.text(((rect[2] + W) / 2, 200), 'Under the Dome, 1930', font=font('AlegreyaSans-Italic.ttf', 18), fill=(60, 40, 20), anchor='mm')
    d.text(((rect[2] + W) / 2, 330), 'N', font=font('Limelight-Regular.ttf', 40), fill=(60, 40, 20), anchor='mm')
    d.polygon([((rect[2] + W) / 2, 360), ((rect[2] + W) / 2 - 18, 440), ((rect[2] + W) / 2 + 18, 440)], fill=(90, 50, 30))
    d.rectangle(list(rect[:2]) + [rect[0] + rect[2] - 1, rect[1] + rect[3] - 1], outline=(0, 0, 0), width=2)
    c.im.save(out)
    return {'size': [W, H], 'rect': [round(v, 2) for v in rect], 'area_min': amin, 'area_size': asize}


def dungeon_source(area, out):
    W, H = 1024, 1536
    amin, asize = area['min'], area['size']
    k = H / asize[1]
    rect = ((W - asize[0] * k) / 2, 0, asize[0] * k, H)
    c = Canvas((W, H), rect, amin, asize)
    c.box(amin[0], amin[1], asize[0], asize[1], (48, 38, 32))
    for g in area['ground']:
        c.box(g['x'], g['y'], g['w'], g['d'], rgb(g['color'], 1.5))
    for b in area['boxes']:
        c.box(b['x'], b['y'], b['w'], b['d'], rgb(b['color'], 1.6), outline=(20, 14, 10))
    mid = font('Limelight-Regular.ttf', 30)
    for s in area['subzones']:
        c.text(s['x'] + s['w'] / 2, s['y'] + s['d'] / 2, s['name'], mid, fill=(245, 225, 170), stroke=3, stroke_fill=(40, 25, 10))
    c.d.text((rect[0] / 2, 200), 'THE\nEGYPTIAN\nTHEATRE', font=font('Limelight-Regular.ttf', 28), fill=(60, 40, 20), anchor='mm', align='center')
    c.d.rectangle([rect[0], rect[1], rect[0] + rect[2] - 1, rect[1] + rect[3] - 1], outline=(0, 0, 0), width=2)
    c.im.save(out)
    return {'size': [W, H], 'rect': [round(v, 2) for v in rect], 'area_min': amin, 'area_size': asize}


def world_source(world, out):
    W, H = 1536, 1024
    im = Image.new('RGB', (W, H), (214, 200, 168))
    d = ImageDraw.Draw(im)
    # the dome over everything; terrain hints: the valley north, the hills across the middle, the flats south
    d.ellipse([20, 10, W - 20, H - 10], outline=(150, 90, 160), width=6)
    terrain = {'zone_burbank': (150, 170, 110), 'zone_universal_city': (150, 170, 110), 'zone_studio_city': (140, 165, 120),
               'zone_mount_lee': (130, 110, 80), 'zone_hollywoodland': (150, 130, 90), 'zone_cahuenga_pass': (150, 130, 90),
               'zone_griffith_park': (110, 140, 90), 'zone_hollywood_blvd': (200, 170, 120), 'zone_gower_gulch': (190, 160, 110),
               'zone_melrose': (195, 175, 130), 'zone_east_hollywood': (190, 170, 130), 'zone_echo_park': (170, 165, 120),
               'zone_west_hollywood': (195, 175, 135)}
    names = {z['id']: z['name'] for z in world['zones']}
    f = font('Limelight-Regular.ttf', 22)
    for zid, (x, y, w, h) in world['map'].items():
        d.rounded_rectangle([x * W, y * H, (x + w) * W, (y + h) * H], radius=18, fill=terrain.get(zid, (180, 160, 120)), outline=(70, 50, 30), width=3)
    for zid, (x, y, w, h) in world['map'].items():
        d.text(((x + w / 2) * W, (y + h / 2) * H), names[zid], font=f, fill=(30, 20, 10), anchor='mm', stroke_width=2, stroke_fill=(240, 230, 205))
    d.text((W / 2, 40), 'HOLLYWOOD UNDER THE DOME', font=font('Limelight-Regular.ttf', 34), fill=(60, 40, 20), anchor='mm')
    im.save(out)
    return {'size': [W, H], 'rect': [0, 0, W, H], 'normalized': True}


def main():
    os.makedirs(os.path.join(OUT, 'src'), exist_ok=True)
    areas = json.load(open(os.path.join(DATA, 'areas.json'), encoding='utf-8'))
    areas = areas if isinstance(areas, list) else areas.get('areas', areas)
    areas = {a['id']: a for a in (areas if isinstance(areas, list) else areas.values())}
    world = json.load(open(os.path.join(DATA, 'world.json'), encoding='utf-8'))
    maps = {
        'map_zone_hollywood_blvd': zone_source(areas['zone_hollywood_blvd'], os.path.join(OUT, 'src', 'map_zone_hollywood_blvd.png')),
        'map_dng_egyptian': dungeon_source(areas['dng_egyptian'], os.path.join(OUT, 'src', 'map_dng_egyptian.png')),
        'map_world': world_source(world, os.path.join(OUT, 'src', 'map_world.png')),
    }
    json.dump(maps, open(os.path.join(OUT, 'maps.json'), 'w'), indent=1)
    print('sources:', ', '.join(maps))


if __name__ == '__main__':
    main()
