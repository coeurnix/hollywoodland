"""One ledger owner processes independent inputs while Blender work continues."""
import json
from pathlib import Path
from rodin_client import run
R=Path(__file__).resolve().parents[1]
repairs=[('SPN_U_Bay3A',None),('SPN_CrestB',None),('DECO_CrestB',None),('DECO_Parapet4',None),('DECO_Awning6','concept/DECO_Awning6_front.png')]
for aid,ref in repairs:
    run(aid,reference_path=ref)
for item in json.loads((R/'src/MSN_texture_masters.json').read_text()):
    aid=item['id']
    if not (R/f'work/{aid}/base_basic_shaded.glb').exists(): run(aid)
for item in json.loads((R/'src/roof_clutter_texture_queue.json').read_text())['assets']:
    aid=item['id']
    if not (R/f'work/{aid}/base_basic_shaded.glb').exists():
        run(aid,reference_path=item['reference'],model_path=item['model'],prompt=item['texture_notes'])
for item in json.loads((R/'work/dressing/texture_request_queue.json').read_text()):
    if not (R/f"work/{item['id']}/base_basic_shaded.glb").exists():
        run(item['id'],reference_path=item['reference'],model_path=item['model'],prompt=item['prompt'])
