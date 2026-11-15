"""Cuts the gameplay trailer to the theme song: python -I tools/capture/trailer_cut.py [--preview]

Shots come from tools/capture/trailer_shots.sh (hollywoodland-ue/Saved/Capture/t_<shot>/f_NNNN.png, 1080p, 30 fps). The cut
list below gives each shot's source start and its length; cuts land on the song's accents (measured: beat 0.55 s; strong
hits at 13.25, 20.15, 23.2, 24.2, 26.85, 28.75, 30.6, 33.8, 34.65, 38.05, 39.95, 42.35, 44.7, 46.6, 53.1, 54.65, 58.2).
The end card: HOLLYWOODLAND (Limelight, gold) on the first sung name, THE MMORPG on the second, held through the last hit.
Writes tools/capture/trailer/hollywoodland_trailer.mp4 (H.264 1080p30, AAC).
"""
import os, sys, subprocess, shutil
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CAP = os.path.join(ROOT, 'hollywoodland-ue', 'Saved', 'Capture')
OUT = os.path.join(ROOT, 'tools', 'capture', 'trailer')
SONG = os.path.join(ROOT, 'tools', 'capture', 'itch', 'theme-song.mp3')
FONT = os.path.join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'Fonts', 'Limelight-Regular.ttf')
FF = os.path.join(os.environ['LOCALAPPDATA'], 'Microsoft', 'WinGet', 'Packages', 'Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe',
                  'ffmpeg-9.0.2-full_build', 'bin', 'ffmpeg.exe')
FPS = 30
W, H = 1920, 1080

# (song time where the shot starts, shot, source start in seconds). Each shot runs until the next one starts.
GEAR = (0.36, 0.22, 1.6, 2.0)
TALENTS = (0.46, 0.21, 1.6, 1.9)
# Each entry: (song time, shot, source start s[, (centre x, centre y, zoom at start, zoom at end)]): a zoom crop, pushing in.
CUTS = [
    (0.00, 'intro', 0.0),        # the sign at sunset, the figure turning: grand opening
    (6.00, 'blvd', 0.0),         # the boulevard at golden hour
    (11.00, 'film', 0.0),        # the swing starts: the forecourt crowd
    (13.25, 'gestures', 1.0),    # people being people, on the phrases
    (15.45, 'gestures', 7.0),
    (17.65, 'gestures', 13.0),
    (20.15, 'run', 0.6),         # the world opens up: running the boulevard with a party
    (23.20, 'horse', 1.0),       # on horseback through Hollywood & Vine
    (26.85, 'scout', 1.0),       # the Indian Scout
    (28.75, 'modela', 1.0),      # the Model A
    (30.60, 'redcar', 3.0),      # boarding the red car
    (33.80, 'combat', 0.5),      # hard cuts on the stabs
    (34.65, 'night', 0.8),
    (36.50, 'gear', 0.5, GEAR),  # the paper doll and bags, pushed in
    (38.05, 'dungeon', 5.5),     # the big band: the party in the Egyptian
    (42.35, 'talents', 0.5, TALENTS),
    (44.70, 'dungeon', 10.0),     # back to the boss
    (49.50, 'film', 3.0),        # the crowd
    (52.00, 'intro', 9.0),       # end card over the sign
]
END = 59.6
CARD_NAME_AT, CARD_SUB_AT = 52.6, 54.4


def frames(shot):
    d = os.path.join(CAP, 't_' + shot)
    fs = sorted(f for f in os.listdir(d) if f.endswith('.png'))
    return [os.path.join(d, f) for f in fs]


def card_layer(alpha_name, alpha_sub):
    """The end card's text as an RGBA layer: gold Limelight with a soft dark glow."""
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    def put(text, size, y, a, spacing=0):
        if a <= 0:
            return
        f = ImageFont.truetype(FONT, size)
        tw = d.textlength(text, font=f) + spacing * (len(text) - 1)
        x = (W - tw) / 2
        glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        g = ImageDraw.Draw(glow)
        xx = x
        for ch in text:
            g.text((xx + 4, y + 6), ch, font=f, fill=(0, 0, 0, int(220 * a)))
            xx += d.textlength(ch, font=f) + spacing
        glow = glow.filter(ImageFilter.GaussianBlur(10))
        im.alpha_composite(glow)
        xx = x
        for ch in text:
            d.text((xx, y), ch, font=f, fill=(244, 206, 112, int(255 * a)))
            xx += d.textlength(ch, font=f) + spacing
    put('HOLLYWOODLAND', 140, 560, alpha_name, 6)
    put('THE MMORPG', 84, 745, alpha_sub, 14)
    put('play free in your browser  ·  coeurnix.itch.io/hollywoodland', 34, 880, alpha_sub * 0.9)
    return im


def main():
    preview = '--preview' in sys.argv
    tmp = os.path.join(OUT, 'frames')
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp, exist_ok=True)
    cache = {}
    total = int(round(END * FPS))
    k = 0
    for i, cut in enumerate(CUTS):
        t0, shot, src = cut[:3]
        crop = cut[3] if len(cut) > 3 else None
        t1 = CUTS[i + 1][0] if i + 1 < len(CUTS) else END
        fs = cache.setdefault(shot, frames(shot))
        n0 = int(round(t0 * FPS)); n1 = int(round(t1 * FPS))
        for n in range(n0, n1):
            j = int(round(src * FPS)) + (n - n0)
            j = min(j, len(fs) - 1)  # a short shot holds its last frame
            dst = os.path.join(tmp, f'f_{n:05d}.png')
            t = n / FPS
            if t >= CARD_NAME_AT - 0.5:
                # the end card: the backdrop darkened a little, the title over it
                im = Image.open(fs[j]).convert('RGBA').resize((W, H))
                dark = min(1.0, (t - (CARD_NAME_AT - 0.5)) / 0.8) * 0.45
                im = Image.blend(im, Image.new('RGBA', (W, H), (0, 0, 0, 255)), dark)
                a1 = max(0.0, min(1.0, (t - CARD_NAME_AT) / 0.45))
                a2 = max(0.0, min(1.0, (t - CARD_SUB_AT) / 0.35))
                im.alpha_composite(card_layer(a1, a2))
                im.convert('RGB').save(dst)
            elif crop:
                cx, cy, z0, z1 = crop
                z = z0 + (z1 - z0) * (n - n0) / max(1, n1 - n0 - 1)
                im = Image.open(fs[j]).convert('RGB')
                w, h = im.width / z, im.height / z
                x = min(max(cx * im.width - w / 2, 0), im.width - w); y = min(max(cy * im.height - h / 2, 0), im.height - h)
                im.resize((W, H), Image.LANCZOS, box=(x, y, x + w, y + h)).save(dst)
            else:
                try:
                    os.link(fs[j], dst)
                except OSError:
                    shutil.copyfile(fs[j], dst)
            k += 1
    os.makedirs(OUT, exist_ok=True)
    out = os.path.join(OUT, 'hollywoodland_trailer_preview.mp4' if preview else 'hollywoodland_trailer.mp4')
    vf = f'scale={W}:{H}:flags=lanczos,fade=t=in:st=0:d=0.6,fade=t=out:st={END - 1.2}:d=1.2' + (',scale=960:-2' if preview else '')
    subprocess.run([FF, '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(tmp, 'f_%05d.png'), '-i', SONG,
                    '-vf', vf, '-af', f'afade=t=out:st={END - 1.5}:d=1.5', '-t', str(END),
                    '-c:v', 'libx264', '-preset', 'slow' if not preview else 'veryfast', '-crf', '17' if not preview else '26',
                    '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-b:a', '256k', '-movflags', '+faststart', out], check=True)
    print('wrote', out, k, 'frames')


if __name__ == '__main__':
    main()
