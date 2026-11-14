"""Sequential paid texture calls; maximum two attempts per representative."""
import json,sys,shutil
from pathlib import Path
from rodin_client import run
R=Path(__file__).resolve().parents[1]
FAM=sys.argv[1] if len(sys.argv)>1 else 'BRK'
for item in json.loads((R/f'src/{FAM}_texture_masters.json').read_text()):
 aid=item['id']
 cached=R/'src/rodin'/aid/'base_basic_shaded.glb';target=R/'work'/aid/'base_basic_shaded.glb'
 if cached.exists() and not target.exists():target.parent.mkdir(exist_ok=True);shutil.copy2(cached,target)
 if (R/'work'/aid/'base_basic_shaded.glb').exists():continue
 if not run(aid):
  if not run(aid):print(aid,'requires projection fallback',flush=True)
