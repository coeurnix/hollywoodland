"""Combine the geometry-only hotel views into a labelled contact sheet."""
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
R=Path(__file__).resolve().parents[2];P=R/'work/hub_hotels/preview'
ids=['LM_Roosevelt','LM_HollywoodHotel','LM_HollywoodHotel_Wing','LM_HollywoodHotel_CourtWall']
out=Image.new('RGB',(1440,4*352),(34,34,31));d=ImageDraw.Draw(out)
for r,aid in enumerate(ids):
 for c,view in enumerate(['front34','back34','roof45']):
  im=Image.open(P/f'{aid}_{view}.png').convert('RGB');im.thumbnail((480,320));out.paste(im,(c*480,r*352+24));d.text((c*480+10,r*352+5),aid+' | '+view,fill=(240,220,180))
out.save(P/'hotels_geometry_contact.png')
