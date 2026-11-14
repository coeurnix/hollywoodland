"""Split generated two-view sheets without altering the generated artwork."""
from pathlib import Path
from PIL import Image
import json
R=Path(__file__).resolve().parents[2]
for a in json.loads((R/'src/hub_root/plan.json').read_text()):
 p=R/f"work/hub_root/{a['id']}_sheet.png"
 if not p.exists():continue
 im=Image.open(p).convert('RGB');w,h=im.size
 im.crop((0,0,w//2,h)).save(R/f"concept/{a['id']}_34.png")
 im.crop((w//2,0,w,h)).save(R/f"concept/{a['id']}_front.png")
print('Root hub concepts packaged')
