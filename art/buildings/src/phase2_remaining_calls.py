"""Run only after phase2_texture_queue.py exits: sole ledger writer."""
import json
from pathlib import Path
from rodin_client import run
R=Path(__file__).resolve().parents[1]
for aid in ['MSN_G_Bay6A','MSN_Parapet4','MSN_Awning6']:
 run(aid)
for a in json.loads((R/'work/dressing/texture_request_queue.json').read_text()):
 if not (R/f"work/{a['id']}/base_basic_shaded.glb").exists():run(a['id'],reference_path=a['reference'],model_path=a['model'],prompt=a['prompt'])
