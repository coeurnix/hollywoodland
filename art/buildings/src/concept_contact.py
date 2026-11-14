from pathlib import Path
from PIL import Image,ImageDraw
import json,sys
R=Path(__file__).resolve().parents[1]
FAM=sys.argv[1] if len(sys.argv)>1 else 'BRK'
rows=[a for a in json.loads((R/'src/kit_plan.json').read_text()) if a['family']==FAM]
for view in ['front','34']:
 sheet=Image.new('RGB',(1400,2800),(55,55,55));d=ImageDraw.Draw(sheet)
 for i,a in enumerate(rows):
  x=(i%4)*350;y=(i//4)*400;d.text((x+8,y+8),a['id'],fill='white')
  path=R/f"concept/{a['id']}_{view}.png"
  if path.exists():
   im=Image.open(path).convert('RGB');im.thumbnail((330,360));sheet.paste(im,(x+(350-im.width)//2,y+30+(360-im.height)//2))
  else:d.text((x+8,y+50),'pending',fill=(180,150,100))
 sheet.save(R/f'preview/{FAM}_concepts_{view}.png')
