"""Paint the game maps with Codex (gpt-6.1-sol, high reasoning, its image generation tool) from the layout sources.

    python tools/maps/source_maps.py      # art/maps/src/*.png + art/maps/maps.json (exact registration)
    python tools/maps/paint_maps.py [names...]   # art/maps/painted/<name>.png via `codex exec`, in parallel
    python tools/maps/install_maps.py     # registration check + Content/Data/UI/<name>.jpg for the game

The prompt pins the layout: the game draws icons, pins and players at exact coordinates on top of the painting.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'art', 'maps', 'src')
OUT = os.path.join(ROOT, 'art', 'maps', 'painted')
LOGS = os.path.join(ROOT, 'art', 'maps', 'logs')

STYLE = ("Style: a hand-painted fantasy-MMO zone map crossed with a 1930 Hollywood tourist map. Aged parchment, painted top-down "
         "rooftops and streets, warm sepia, cream and Art Deco gold, crisp dark ink outlines, gentle painted shading. The "
         "alien-blighted land outside the hive wall is dark violet with organic resin texture and small hive spires. Strictly "
         "top-down orthographic, no perspective tilt, no characters, no UI icons, no legend, no extra text.")

RULES = ("The attached image is an exact layout source. Your painting must stay registered to it pixel for pixel: every road, "
         "block, building footprint, wall and panel keeps the same position, size and shape, because the game overlays markers "
         "at exact coordinates. Use the source as the composition and repaint it; do not move, add or remove streets or buildings, "
         "do not crop, pad or zoom. Labels must be legible and spelled exactly as listed, placed where the source places them; "
         "drop any source label that is not listed.")

JOBS = {
    'map_zone_hollywood_blvd': dict(size='1536x1024 landscape', labels=[
        'Districts (elegant italic): Grauman\'s Chinese Forecourt, Hollywood & Highland, Hollywood & Vine, Franklin Slopes, '
        'Selma Avenue Alleys, Cahuenga Rooftops, Taft Back Lots, Hollywood High School, Sunset Boulevard',
        'Landmarks (small caps on or beside the building): Grauman\'s Chinese Theatre, Hollywood Hotel, Roosevelt Hotel, '
        'First National Bank, The Egyptian Theatre, Pantages Theatre, Taft Building, Brown Derby, Hollywood Police Station, '
        'Hollywood Receiving Hospital, Fire Station No. 27, Blessed Sacrament Church, Max Factor Salon, Drugstore',
        'Streets (small capitals along the street): Franklin Avenue, Hollywood Boulevard, Selma Avenue, Sunset Boulevard, '
        'Orange Dr., Orchid Ave., Highland Avenue, McCadden Pl., Wilcox Avenue, Cahuenga Blvd., Vine Street',
        'Outer regions beyond the hive wall (large Art Deco display letters): Cahuenga Pass, Melrose, West Hollywood, Gower Gulch; '
        'and small: Hollywood Bowl, Paramount Pictures, RKO Radio Pictures, Columbia Pictures, Poverty Row, Christie Studio, '
        'Chaplin Studios, Hollywood Cemetery, La Brea Avenue',
        'The thin purple ring is the Hive Wall (label it once, vertically, on the west side: The Hive Wall)',
        'Right-hand margin panel: an Art Deco title cartouche reading "Hollywood Boulevard" with "Under the Dome, 1930" '
        'beneath it, and a compass rose with N pointing up']),
    'map_dng_egyptian': dict(size='1024x1536 portrait', labels=[
        'Room names (Art Deco display letters, cream on dark): Forecourt, Lobby, Auditorium, Projection Booth, Stage',
        'Left margin: a title cartouche reading "The Egyptian Theatre" with Egyptian-revival ornament (scarabs, papyrus columns)',
        'Interior floor plan seen from directly above: carpets, rows of seats in the auditorium, the stage boards, hive resin '
        'creeping across the floors']),
    'map_world': dict(size='1536x1024 landscape', labels=[
        'Region names (Art Deco display letters, each centred in its region): Hollywood Boulevard, Gower Gulch, Melrose, '
        'East Hollywood, Echo Park & Edendale, Griffith Park & Los Feliz, West Hollywood & the Strip, Hollywoodland, '
        'Cahuenga Pass, Universal City, Studio City & the River, Burbank, Mount Lee',
        'Title at the top: "Hollywood Under the Dome"',
        'Geography: the San Fernando Valley flats in the north (Burbank, Universal City, Studio City with the Los Angeles River), '
        'the Hollywood Hills across the middle (Cahuenga Pass, Hollywoodland with the HOLLYWOODLAND sign on Mount Lee, Griffith '
        'Park), the city flats in the south. The great translucent violet dome rim is the ellipse; outside it, dark violet blight. '
        'Each region keeps its rectangle position and size from the source but may have softened painted borders like '
        'territories on a fantasy world map.']),
}


def prompt(name, job):
    out = os.path.join('art', 'maps', 'painted', name + '.png')
    return (f"Use your image generation tool to paint a game map. {RULES}\n\n{STYLE}\n\nLabels:\n- " + '\n- '.join(job['labels'])
            + f"\n\nOutput: {job['size']}, exactly the source's size and aspect. Save the final image as `{out}` (PNG). If the "
              "generated image is a different size, scale it to the exact size without cropping (Python Pillow is available). "
              "Look at your result next to the source and regenerate if streets or buildings moved or a label is misspelt. "
              "Do not modify any other file. Finish with one line: the saved path.")


def run(name):
    job = JOBS[name]
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(LOGS, exist_ok=True)
    src = os.path.join(SRC, name + '.png')
    cmd = ['codex', 'exec', '-C', ROOT, '-s', 'workspace-write', '-c', 'model_reasoning_effort="high"', '--skip-git-repo-check',
           '-i', src, '-o', os.path.join(LOGS, name + '.last.txt'), '-']
    with open(os.path.join(LOGS, name + '.log'), 'w', encoding='utf-8') as log:
        r = subprocess.run(cmd, input=prompt(name, job), text=True, stdout=log, stderr=subprocess.STDOUT, shell=(os.name == 'nt'))
    ok = os.path.exists(os.path.join(OUT, name + '.png'))
    print(f'{name}: exit {r.returncode}, {"saved" if ok else "NO IMAGE"}', flush=True)
    return ok


if __name__ == '__main__':
    names = sys.argv[1:] or list(JOBS)
    with ThreadPoolExecutor(len(names)) as pool:
        results = list(pool.map(run, names))
    sys.exit(0 if all(results) else 1)
