"""Rebuild the phase-one checkpoint without paid API calls.
Usage: python art/buildings/src/rebuild_phase1.py --blender <blender executable>
This publishes the phase-one manifest; do not use it to replace a later phase manifest.
"""
import argparse,subprocess,sys,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--blender',default=shutil.which('blender'));args=parser.parse_args()
if not args.blender:parser.error('Pass --blender with the Blender executable path')
for folder in ['work','meshes','textures','signs','preview','src']:(R/folder).mkdir(exist_ok=True)
def python(script):subprocess.run([sys.executable,str(R/'src'/script)],check=True,cwd=R.parents[1])
def blender(script):subprocess.run([args.blender,'--background','--python',str(R/'src'/script)],check=True,cwd=R.parents[1])
python('make_sign_page.py');blender('proof_model.py');blender('proof_remaining.py');blender('export_signs.py')
for aid in ['BRK_G_Bay6A','LM_Drugstore']:
 dest=R/'work'/aid;dest.mkdir(exist_ok=True)
 for f in (R/'src/rodin'/aid).glob('*.glb'):shutil.copy2(f,dest/f.name)
blender('finish_proof.py');blender('audit_uv_density.py');python('publish_phase1.py')
