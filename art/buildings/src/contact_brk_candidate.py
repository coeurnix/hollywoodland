from pathlib import Path
from PIL import Image,ImageDraw
import json,sys
FAM=sys.argv[1] if len(sys.argv)>1 else 'BRK'
R=Path(__file__).resolve().parents[1];P=R/f'work/{FAM}_candidate/preview'
entries=json.loads((R/f'work/{FAM}_kit_assets.json').read_text());sheet=Image.new('RGB',(1800,((len(entries)+1)//2)*330),(55,55,55));d=ImageDraw.Draw(sheet)
for i,a in enumerate(entries):
 for j,view in enumerate(['front','back','top']):
  x=((i%2)*3+j)*300;y=(i//2)*330
  im=Image.open(P/f"{a['id']}_{view}.png").convert('RGB');im.thumbnail((300,300));sheet.paste(im,(x,y+25));d.text((x+6,y+6),a['id']+' '+view,fill='white')
sheet.save(P/f'{FAM}_contact.png')
print('Candidate contact sheet complete')
