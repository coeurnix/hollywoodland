"""Process-artifact contact sheets of the eight delegated staged reviews."""
from pathlib import Path
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[2]
ids=['LM_Egyptian','LM_Egyptian_Wing','LM_Egyptian_Gateway','LM_RedCarStop','LM_Kiosk_Auction','LM_Graumans','LM_Graumans_ForecourtWall','LM_HollywoodHotel_CourtWall']
for batch in range(2):
 sheet=Image.new('RGB',(1536,4*420),(210,210,210));draw=ImageDraw.Draw(sheet)
 for row,aid in enumerate(ids[batch*4:batch*4+4]):
  draw.text((10,row*420+6),aid,fill=(10,10,10))
  for col,view in enumerate(['front','back','top','box']):
   p=R/f'work/{aid}_candidate/preview/{aid}_{view}.png'
   if not p.exists():continue
   im=Image.open(p).convert('RGB');im.thumbnail((384,384));sheet.paste(im,(col*384,row*420+30));draw.text((col*384+8,row*420+405),view,fill=(10,10,10))
 sheet.save(R/f'work/hub_egyptian/stage_review_{batch+1}.png')
