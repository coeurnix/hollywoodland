# Text-to-3D enemy meshes from Hyper3D Rodin Gen-2.5. The API key comes from the RODIN_KEY env var only.
# Usage: python -I gen.py <outdir> [takes=2] [only=name,name]
import json, os, sys, time, uuid, urllib.request, urllib.error, concurrent.futures as cf

API = 'https://api.hyper3d.com/api/v2'
KEY = os.environ['RODIN_KEY']
OUT = sys.argv[1]
TAKES = int(sys.argv[2]) if len(sys.argv) > 2 else 2
ONLY = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None

STYLE = ('stylized hand-painted game creature for a 1930s pulp sci-fi game, painterly texture, clean readable silhouette, '
         'bilaterally symmetrical, standing in a neutral rest pose, single creature, no base, no ground, no props')
INSECT_POSE = 'all six legs spread out wide to the sides and clearly separated from the body and from each other, body level, head facing forward'
QUAD_POSE = 'standing on all four legs in a neutral side-profile pose, legs straight and clearly separated, tail held out behind, head facing forward'

PROMPTS = {
  'worker':  f'giant worker ant insect, olive green chitin, slender segmented body, small mandibles, long antennae, thin legs, {INSECT_POSE}, {STYLE}',
  'drone':   f'giant beetle-like hive drone insect, dark green glossy chitin carapace, large curved mandibles, compound eyes, short antennae, strong legs, {INSECT_POSE}, {STYLE}',
  'spitter': f'giant acid spitter insect, yellow-green chitin, swollen glowing translucent acid sac abdomen, long neck with a tube-like spitting mouth, {INSECT_POSE}, {STYLE}',
  'soldier': f'giant armoured soldier insect, heavy dark green chitin armour plates like the bodywork of a 1930 automobile, huge serrated mandibles, horned armoured head, thick legs, {INSECT_POSE}, {STYLE}',
  'broodling': f'small larval insect hatchling the size of a dog, pale green soft chitin, oversized head with sharp mandibles, translucent green venom sac under the body, stubby legs, {INSECT_POSE}, {STYLE}',
  'burrower': f'giant burrowing insect like a mole cricket, reddish-brown chitin, huge shovel-like digging front claws, domed armoured back, tiny eyes, {INSECT_POSE}, {STYLE}',
  'flier':   f'giant flying wasp-like insect, ochre yellow and dark brown chitin, four large translucent membrane wings spread flat out to the sides, long stinger, legs tucked under the body, hovering pose, {STYLE}',
  'brute':   f'massive brute rhinoceros beetle the size of an automobile, dark purple armoured chitin, enormous horn on the head, thick crushing front legs, hunched heavy body, {INSECT_POSE}, {STYLE}',
  'queen':   f'giant insect hive queen in the style of an Egyptian scarab, magenta-purple chitin with gold scarab markings, huge bloated egg-laying abdomen glowing with orange eggs, regal crested head with a crown of horns, two raised clawed forelimbs, four walking legs spread wide, {STYLE}',
  'brood_keeper': f'massive brute beetle carrying clusters of glowing orange insect eggs fused to its back, dark purple-mauve armoured chitin, thick crushing front legs, heavy hunched body, {INSECT_POSE}, {STYLE}',
  'hive_warden':  f'giant armoured soldier insect guardian, teal-green chitin armour, tall flat crested head shield like a battering ram for charging, huge mandibles, spiked shoulder plates, {INSECT_POSE}, {STYLE}',
  'matriarch':    f'huge bloated acid spitter insect matriarch, bright acid-yellow chitin, enormous glowing green acid sac abdomen dripping acid, wide head with a gaping spitting mouth, {INSECT_POSE}, {STYLE}',
  'coyote':     f'lean wild coyote, tawny grey-brown fur, pointed ears, bushy tail, {QUAD_POSE}, {STYLE}',
  'feral_dog':  f'scruffy feral street dog mutt, mangy dark brown fur, thin with ribs showing, torn ear, {QUAD_POSE}, {STYLE}',
  'old_scar':   f'big old grey coyote, grizzled grey fur, muscular, a long pale scar across the muzzle and eye, one torn ear, {QUAD_POSE}, {STYLE}',
  'hive_coyote': f'coyote parasitised by an insect hive, mangy grey-brown fur with dark purple chitin plates and spiky growths erupting from its back and shoulders, glowing orange pustules, {QUAD_POSE}, {STYLE}',
  # second round: insist on a horizontal six-legged insect (round one gave dogs and upright humanoids)
  'broodling_b': f'insect larva grub hatchling with six short legs, horizontal segmented pale green body like a beetle larva, big round head with sharp mandibles, translucent green venom sac on the belly, not a mammal, not a dog, low to the ground, {INSECT_POSE}, {STYLE}',
  'soldier_b':   f'giant armoured soldier beetle on six legs, horizontal body low to the ground like a stag beetle, heavy dark green chitin armour plates, huge serrated pincer mandibles, horned armoured head, not humanoid, not standing upright, {INSECT_POSE}, {STYLE}',
  'hive_warden_b': f'giant armoured guardian beetle on six legs, horizontal body low to the ground, teal-green chitin armour, broad flat shovel-shaped crest on the head like a battering ram, huge mandibles, spiked shoulder plates, not humanoid, not standing upright, {INSECT_POSE}, {STYLE}',
  'queen_b':     f'giant insect hive queen lying horizontally on the ground, scarab beetle head and thorax with magenta-purple chitin and gold scarab markings, crown of horns, enormous long bloated egg-laying abdomen behind her glowing with orange eggs, two clawed forelimbs raised in front, four walking legs spread wide, not humanoid, not standing upright, {STYLE}',
  'broodling_c': f'small insect larva crawling flat on the ground, long horizontal segmented grub body like a giant mealworm, pale green, big head at the front with sharp mandibles, six short legs spread out to the sides under the front segments, translucent green venom sac on its back, low profile, lying flat, not upright, not standing, {STYLE}',
  'egg_sac':    'large pulsing insect egg sac, translucent amber-orange veined membrane with dark larvae visible inside, resting on a low mound of dark hive resin, stylized hand-painted game prop for a 1930s pulp sci-fi game, painterly texture, no ground plane',
}

def call(path, *, form=None, body=None, tries=5):
    for i in range(tries):
        if form is not None:
            b = uuid.uuid4().hex; data = b''.join(
                f'--{b}\r\nContent-Disposition: form-data; name="{k}"\r\n\r\n{v}\r\n'.encode() for k, v in form) + f'--{b}--\r\n'.encode()
            hdr = {'Content-Type': f'multipart/form-data; boundary={b}'}
        else:
            data = json.dumps(body).encode(); hdr = {'Content-Type': 'application/json'}
        req = urllib.request.Request(API + path, data=data, headers={**hdr, 'Authorization': f'Bearer {KEY}'}, method='POST')
        try:
            with urllib.request.urlopen(req, timeout=120) as r: return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 429 or (form is None and e.code >= 500): time.sleep(5 * (i + 1)); continue
            raise RuntimeError(f'{path} {e.code} {e.read()[:300]}')
        except urllib.error.URLError:
            if form is not None: raise  # never resubmit a paid job blindly
            time.sleep(5 * (i + 1))
    raise RuntimeError(path + ' retries exhausted')

def job(name, take):
    d = os.path.join(OUT, f'{name}_{take}')
    os.makedirs(d, exist_ok=True)
    if any(f.endswith('.glb') for f in os.listdir(d)): return f'{name}_{take}: already done'
    tj = os.path.join(d, 'task.json')
    if os.path.exists(tj): t = json.load(open(tj))
    else:
        t = call('/rodin', form=[('prompt', PROMPTS[name]), ('tier', 'Gen-2.5-Medium'), ('geometry_file_format', 'glb'),
                                 ('material', 'PBR'), ('mesh_mode', 'Raw'), ('quality', 'medium')])
        json.dump(t, open(tj, 'w'))
    sub = t['jobs']['subscription_key']
    while True:
        s = call('/status', body={'subscription_key': sub})
        st = [j['status'] for j in s['jobs']]
        if all(x == 'Done' for x in st): break
        if any(x == 'Failed' for x in st): return f'{name}_{take}: FAILED'
        time.sleep(10)
    for f in call('/download', body={'task_uuid': t['uuid']})['list']:
        if f['name'].endswith(('.glb', '.webp', '.png', '.jpg')):
            with urllib.request.urlopen(f['url'], timeout=300) as r, open(os.path.join(d, os.path.basename(f['name'])), 'wb') as o: o.write(r.read())
    return f'{name}_{take}: ok'

jobs = [(n, k) for n in PROMPTS if not ONLY or n in ONLY for k in range(1, TAKES + 1)]
with cf.ThreadPoolExecutor(6) as ex:
    for fut in cf.as_completed([ex.submit(job, n, k) for n, k in jobs]):
        try: print(fut.result(), flush=True)
        except Exception as e: print('ERROR', e, flush=True)
