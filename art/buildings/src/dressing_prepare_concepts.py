"""Package the generated two-view sheets without altering their artwork."""
from pathlib import Path
from PIL import Image
import json
R=Path(__file__).resolve().parents[1]
plan=json.loads((R/'src/dressing_plan.json').read_text())
for a in plan:
 p=R/'work/dressing'/f"{a['id']}_sheet.png"
 if not p.exists():continue
 im=Image.open(p).convert('RGB');w,h=im.size
 for suffix,rect in [('34',(0,0,w//2,h)),('front',(w//2,0,w,h))]:
  # Cropping separates the two expressly requested generated views.
  im.crop(rect).save(R/'concept'/f"{a['id']}_{suffix}.png")
print('Dressing concepts packaged')
