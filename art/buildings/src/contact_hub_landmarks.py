"""One phase-three contact containing each published hub asset in three views."""
from pathlib import Path
from PIL import Image,ImageDraw
import json,shutil
R=Path(__file__).resolve().parents[1];m=json.loads((R/'manifest.json').read_text())
entries=[a for a in m['assets'] if a['kind']=='landmark' and a['id']!='LM_Drugstore']
sheet=Image.new('RGB',(1800,((len(entries)+1)//2)*330),(55,55,55));d=ImageDraw.Draw(sheet)
for i,a in enumerate(entries):
    for j,view in enumerate(['front','back','top']):
        x=((i%2)*3+j)*300;y=(i//2)*330
        path=R/f"work/{a['id']}_candidate/preview/{a['id']}_{view}.png"
        im=Image.open(path).convert('RGB');im.thumbnail((300,300));sheet.paste(im,(x,y+25));d.text((x+6,y+6),a['id']+' '+view,fill='white')
    for p in (R/f"work/{a['id']}_candidate/preview").glob('*.png'):shutil.copy2(p,R/'preview'/p.name)
sheet.save(R/'preview/Phase3_contact.png')
print('Published signed contact and four-view previews for',len(entries),'hub landmarks')
