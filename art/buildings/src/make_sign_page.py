"""Compose period lettering with project OFL font, as explicitly allowed by SPEC 10.1."""
from PIL import Image,ImageDraw,ImageFont,PngImagePlugin
from pathlib import Path
import json
R=Path(__file__).resolve().parents[1];(R/'signs').mkdir(exist_ok=True)
fontpath=R.parents[1]/'hollywoodland-ue/Content/Data/Fonts/Limelight-Regular.ttf'
im=Image.new('RGB',(2048,2048),(226,201,148));draw=ImageDraw.Draw(im)
reference=Image.open(R/'concept/BRK_G_Bay6A_front.png').convert('RGB')
rw,rh=reference.size
# Painted stone from our generated concept, with independent OFL lettering composed over it.
stone=reference.crop((round(rw*.14),round(rh*.84),round(rw*.55),round(rh*.93)))
texts=[('SIGN_Drugstore','DRUGSTORE',12,1.1),('SIGN_SodaFountain','SODA FOUNTAIN',12,.7),('SIGN_Auction','AUCTION',3,.75),('SIGN_Haberdashery','HABERDASHERY',8,1)]
records=[]
for i,(aid,label,w,h) in enumerate(texts):
 y=i*512;draw.rectangle((16,y+16,2031,y+495),fill=(38,65,46));im.paste(stone.resize((1984,448),Image.Resampling.LANCZOS),(32,y+32))
 size=220
 while draw.textbbox((0,0),label,font=ImageFont.truetype(str(fontpath),size))[2]>1860:size-=2
 font=ImageFont.truetype(str(fontpath),size);draw.text((1024,y+245),label,font=font,anchor='mm',fill=(27,61,43))
 records.append({'id':aid,'page':'signs/T_SIGN_01.png','rect':[0,i*.25,1,.25],'text':label,'card_mesh':f'meshes/{aid}.fbx','nominal':[w,h]})
meta=PngImagePlugin.PngInfo();meta.add(b'sRGB',b'\x00');im.save(R/'signs/T_SIGN_01.png',pnginfo=meta)
(R/'work/sign_records.json').write_text(json.dumps(records,indent=2))
