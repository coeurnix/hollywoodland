"""FLUX 3 concept turnarounds for the v2 characters: one sheet per look and sex (front + back, A-pose).

They set the target for garment shapes and the painted fabric textures; they are references, never runtime art.
Writes art/characters_v2/concepts/<look>_<sex>.png and the state file next to them.

Run: python -I tools/charv2/concepts.py [--only C3_Male,S1_Female] [--attempt N]
"""
import argparse, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import genlib

OUT = genlib.ROOT / 'art' / 'characters_v2' / 'concepts'

STYLE = ('Character design turnaround sheet for a 3D game: the same single adult character shown twice side by side, '
         'left a full-body front view and right a full-body back view, standing in a relaxed A-pose with arms angled away '
         'from the body and hands open, feet apart, whole figure from hat to shoes in frame with margin. Stylized '
         'semi-realistic hand-painted 3D character in the style of a modern high-quality fantasy MMO, slightly heroic '
         'proportions, clean readable silhouette, rich painted fabric textures with visible seams, folds and stitching, '
         'warm golden-hour palette. Hollywood, 1930, pulp adventure. Plain flat light grey background, even soft '
         'lighting, no props held, no text, no labels, no frames.')

MALE = 'a {age} man with {feat}'
FEMALE = 'a {age} woman with {feat}'

# look -> (male outfit, female outfit); from ART-CHARACTERS.md section 7, made concrete
LOOKS = {
    'Starter': ('a plain collarless cream cotton work shirt with rolled sleeves, brown wool trousers held by braces, scuffed brown leather work shoes',
                'a plain cream cotton blouse, a mid-calf brown wool skirt, brown low-heeled leather shoes'),
    'C1': ('a newsboy cap, striped shirt and a buttoned wool waistcoat with braces, grey wool trousers, canvas plimsolls, cloth wristbands',
           'a cloche hat, a pleated blouse with a short knit cardigan, a mid-calf wool skirt, canvas plimsolls'),
    'C2': ('a felt fedora, a brown wool sack suit jacket with notch lapels over a shirt and tie, flannel trousers, brown oxford shoes',
           'a felt cloche, a belted wool day dress with a white collar, brown oxford shoes'),
    'C3': ('a grey Stetson fedora, a tailored charcoal three-piece suit with waistcoat, tie and pocket square, French cuffs, kid leather gloves, polished oxfords',
           'a tilted brim hat, a tailored charcoal skirt suit with fitted jacket, kid leather gloves, heeled pumps'),
    'C4': ('a silk top hat, a black opening-night tailcoat with satin lapels, white waistcoat and bow tie, striped trousers, white opera gloves, patent pumps',
           'a feathered headband, a floor-length bias-cut black satin evening gown, long white opera gloves, satin heels'),
    'C5': ('a white dinner jacket with art-deco gold trim and a red sash, black trousers, white gloves, patent shoes',
           'a sequined art-deco gold gown with a white fur stole, gold heels'),
    'C6': ('a black formal suit lined with glossy green-black insect chitin plates, iridescent violet lapels, chitin cufflinks',
           'a black gown beaded with glossy green-black insect chitin, an iridescent chitin tiara'),
    'L1': ('a leather flying cap with goggles, a brown leather flight jacket with sheepskin collar, leather gloves, khaki trousers, lace-up boots',
           'a leather flying cap with goggles, a fitted brown leather flight jacket with sheepskin collar, leather gloves, jodhpurs, lace-up boots'),
    'L2': ('a horsehide leather jacket, flared riding breeches, tall brown riding boots, leather gloves',
           'a fitted horsehide leather jacket, flared riding breeches, tall brown riding boots, leather gloves'),
    'L3': ('a wide Boss-of-the-Plains cowboy hat, a western shirt with a bandana, leather chaps over jeans, a tooled gun belt, cowboy boots',
           'a cowgirl hat, a fringed suede jacket, a leather split riding skirt, a tooled belt, cowboy boots'),
    'L4': ('a padded leather stunt-rider helmet, leather jacket, laced leather leggings over breeches, tall cavalry boots, leather roping cuffs',
           'a padded leather stunt-rider helmet, fitted leather jacket, laced leather leggings, tall cavalry boots, roping cuffs'),
    'L5': ('a long Pendleton wool-lined leather riding coat to the knee, a Sam Browne belt with shoulder strap, breeches, riding boots',
           'a long Pendleton wool-lined leather riding coat to the knee, a Sam Browne belt, breeches, riding boots'),
    'L6': ('dark riding leathers studded with glossy green-black insect chitin plates with amber rim highlights, chitin-capped boots',
           'fitted dark riding leathers studded with glossy green-black insect chitin plates with amber highlights, chitin-capped boots'),
    'P1': ('a canvas work cap, a brown duck-canvas chore coat, denim waist overalls, hobnail work boots, canvas work gloves',
           'a headscarf, a brown duck-canvas chore coat with rolled sleeves, denim overalls, work boots'),
    'P2': ('a lineman outfit: padded canvas jacket, heavy leather tool belt with tools, thick leather lineman gloves, climbing boots',
           'a lineman outfit: fitted padded canvas jacket, heavy leather tool belt with tools, leather lineman gloves, climbing boots'),
    'P3': ('a leather football helmet, bulky padded football shoulder pads over a jersey, quilted canvas leggings, cleated boots',
           'a leather football helmet, padded shoulder pads over a jersey, quilted canvas leggings, cleated boots'),
    'P4': ('a quilted aviator helmet, a quilted padded aviator vest over a wool shirt, wool trousers, logger boots',
           'a quilted aviator helmet, a fitted quilted aviator vest over a wool shirt, wool trousers, logger boots'),
    'P5': ('a heavy canvas welder jacket and apron, long leather welder gauntlets, welding goggles pushed up on a cap, work boots',
           'a heavy canvas welder jacket and apron, long leather welder gauntlets, welding goggles on a headscarf, work boots'),
    'P6': ('quilted padding armour reinforced with glossy green-black insect chitin panels and violet highlights',
           'fitted quilted padding armour reinforced with glossy green-black insect chitin panels and violet highlights'),
    'S1': ('a WWI doughboy outfit: M1917 steel brodie helmet, olive wool tunic with a riveted steel vest, cartridge belt, puttees and trench boots',
           'a WWI doughboy outfit: M1917 steel brodie helmet, fitted olive wool tunic with a riveted steel vest, cartridge belt, puttees and trench boots'),
    'S2': ('a riveted steel helmet, a long khaki trench coat with steel plate inserts, steel-kneed breeches, trench boots',
           'a riveted steel helmet, a long fitted khaki trench coat with steel plate inserts, steel-kneed breeches, trench boots'),
    'S3': ('a guard uniform with a mail-backed tunic, plated steel gauntlets, a plated steel belt, trench boots',
           'a guard uniform with a fitted mail-backed tunic, plated steel gauntlets, a plated belt, trench boots'),
    'S4': ('full studio-forged steel plate armour over a khaki uniform, bulky pauldrons, tall steel greaves, art-deco stepped trim',
           'full fitted studio-forged steel plate armour over a khaki uniform, pauldrons, tall steel greaves, art-deco trim'),
    'S5': ('blackened steel plate armour with brass art-deco sunburst and chevron trim over a dark uniform',
           'fitted blackened steel plate armour with brass art-deco sunburst trim over a dark uniform'),
    'S6': ('a cuirass and greaves of glossy green-black insect chitin plates with amber rims, a chitin-crested helmet',
           'a fitted cuirass and greaves of glossy green-black insect chitin plates with amber rims, a chitin-crested helmet'),
    'E1': ('an Argonne commander: officer peaked cap, decorated olive officer tunic with Sam Browne belt, riding breeches, polished tall boots, steel shoulder plates',
           'an Argonne commander: officer cap, decorated fitted olive officer tunic with Sam Browne belt, breeches, polished tall boots, steel shoulder plates'),
    'E2': ('a daredevil stuntman: red and white leather racing suit with stars, a white leather helmet with goggles, a cape-like scarf, tall boots',
           'a daredevil stuntwoman: red and white leather racing suit with stars, a white leather helmet with goggles, a long scarf, tall boots'),
    'E3': ('a lawman: dark Stetson, long black duster coat, waistcoat with a silver star badge, double gun belt, spurred boots',
           'a lawwoman: dark Stetson, long black duster coat, waistcoat with a silver star badge, gun belt, spurred boots'),
    'E4': ('a klieg-light rigger: padded electrician jacket with coiled cables over the shoulder, a battery lamp pack on the back, rubber gloves, work boots',
           'a klieg-light rigger: fitted padded electrician jacket with coiled cables, a battery lamp pack on the back, rubber gloves, work boots'),
    'E5': ('a pyrotechnician: scorched heavy fire-resistant asbestos coat with brass buckles, a hood with goggles, padded gloves, fuse bandolier',
           'a pyrotechnician: scorched fire-resistant coat with brass buckles, a hood with goggles, padded gloves, fuse bandolier'),
    'E6': ('a Red Cross field medic: white coat with a red cross armband, white cap with a red cross, satchel strap, white trousers, white shoes',
           'a Red Cross matron: white nurse uniform dress with apron and red cross, a white nurse cap and cape, white stockings and shoes'),
    'E7': ('a revival-tent preacher: black frock coat to the knee, white clerical collar, purple stole, wide-brimmed black hat, black boots',
           'a revival-tent preacher: black long frock dress coat, white collar, purple stole, wide-brimmed black hat, black boots'),
    'E8': ('a studio grip: canvas coveralls with many pockets, rolled sleeves, a coil of rope and gaffer tape on a tool belt, leather gloves, work boots',
           'a studio grip: fitted canvas coveralls with many pockets, rolled sleeves, a tool belt with rope and tape, leather gloves, work boots'),
}
FEAT = {'Male': 'short dark hair, broad shoulders and a strong jaw', 'Female': 'short wavy 1930s bobbed hair and an athletic build'}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--attempt', type=int, default=0)
    a = ap.parse_args()
    genlib.load_env()
    st = genlib.State(OUT / 'state.json')
    only = set(x for x in a.only.split(',') if x)
    jobs = []
    for look, (m, f) in LOOKS.items():
        for sex, outfit in (('Male', m), ('Female', f)):
            key = f'{look}_{sex}'
            if only and key not in only:
                continue
            who = (MALE if sex == 'Male' else FEMALE).format(age='young adult', feat=FEAT[sex])
            prompt = f'{STYLE} The character: {who}, wearing {outfit}.'
            jobs.append((key, prompt))

    def run(job):
        key, prompt = job
        genlib.flux(key, prompt, OUT / f'{key}.png', st.get(key), 1536, 1024, a.attempt)
        return key

    with ThreadPoolExecutor(6) as ex:
        for k in ex.map(run, jobs):
            print('ok', k, flush=True)
    st.save()


if __name__ == '__main__':
    main()
