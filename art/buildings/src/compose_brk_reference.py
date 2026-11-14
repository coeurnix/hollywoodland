"""Compose generated concept elevations into the family reference sheet allowed by 9.4."""
from pathlib import Path
from PIL import Image,ImageDraw
import json,numpy as np,sys
R=Path(__file__).resolve().parents[1]
layout=json.loads((R/'work/BRK_texture_layout.json').read_text());view=sys.argv[1] if len(sys.argv)>1 else 'front'
sheet=Image.new('RGB',(2048,2048),(100,100,100));scale=44
for a in layout:
 im=Image.open(R/f"concept/{a['id']}_{view}.png").convert('RGB')
 ar=np.asarray(im,dtype=float);chroma=ar.max(axis=2)-ar.min(axis=2)
 yy,xx=np.where(chroma>38)
 if len(xx):im=im.crop((max(0,xx.min()-8),max(0,yy.min()-8),min(im.width,xx.max()+9),min(im.height,yy.max()+9)))
 w,h=a['nominal'];base=0
 if 'Awning' in a['id']:h=.8;base=2.7
 if 'Corner' in a['id']:w=.5
 im=im.resize((round(w*scale),round(h*scale)),Image.Resampling.LANCZOS)
 x=round(40+a['offset'][0]*scale);y=2048-round(40+(a['offset'][2]+base+h)*scale)
 sheet.paste(im,(x,y))
sheet.save(R/f'concept/ATL_BRK_{view}.png')
print('Composed shared-atlas reference sheet')
