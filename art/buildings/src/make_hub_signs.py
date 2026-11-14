"""SPEC10.1: original generated paint with deterministic OFL period lettering.

Two new pages; the published phase-one page is deliberately retained.
Pixel rectangles exclude the eight-pixel gutters. Rectangles in records use
normalized top-left coordinates. Card proportions match the physical sign.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, PngImagePlugin, ImageEnhance
import json

R=Path(__file__).resolve().parents[1]
F=R.parents[1]/'hollywoodland-ue/Content/Data/Fonts'
SOURCE='concept/SIGN_HubBoards_34.png'
paint=Image.open(R/SOURCE).convert('RGB')
background_job=json.loads((R/'work/hub_sign_background_request.json').read_text())
records=[]

def font(size,display=True):
    return ImageFont.truetype(str(F/('Limelight-Regular.ttf' if display else 'AlegreyaSans-Bold.ttf')),max(8,int(size)))

def fit(draw,text,box,display=True):
    w,h=box;size=int(h*.95)
    while size>8:
        f=font(size,display);bb=draw.textbbox((0,0),text,font=f)
        if bb[2]-bb[0]<=w and bb[3]-bb[1]<=h:return f
        size-=1
    return font(8,display)

pages={n:Image.new('RGB',(2048,2048),(33,36,32)) for n in ['02','03']}

def card(aid,page,rect,size,text,lines=None,vertical=False,hat=False,dark=False):
    x,y,w,h=rect;w=int(w);h=int(h)
    bg=paint.resize((w,h),Image.Resampling.LANCZOS)
    if dark:
        # Multiply the generated painted ivory field by period bottle-green.
        import numpy as np
        a=np.array(bg,dtype=float)/255
        a*=np.array([.16,.29,.23]);bg=Image.fromarray((a*255).astype('uint8'))
    d=ImageDraw.Draw(bg);margin=max(3,min(w,h)//25)
    ink=(239,210,145) if dark else (48,66,48)
    border=(177,137,71) if dark else (132,99,55)
    d.rectangle((margin,margin,w-margin-1,h-margin-1),outline=border,width=max(2,margin//3))
    if not text: # a deliberately blank configurable bulb-bordered marquee
        step=max(14,h//3)
        for xx in range(margin+step//2,w-margin,step):
            for yy in [margin,h-margin-1]:
                rad=max(2,h//15);d.ellipse((xx-rad,yy-rad,xx+rad,yy+rad),fill=(246,211,129))
    elif vertical:
        letters=list(text);slot=(h-4*margin)/len(letters)
        for i,ch in enumerate(letters):
            if ch==' ':continue
            f=fit(d,ch,(w*.72,slot*.82));d.text((w/2,2*margin+slot*(i+.5)),ch,font=f,anchor='mm',fill=ink)
    else:
        rows=lines or [text]
        # Hat lettering lies inside the crown, with the brim unobstructed.
        if hat:
            area=(w*.20,h*.12,w*.80,h*.77)
        else:area=(2*margin,2*margin,w-2*margin,h-2*margin)
        x0,y0,x1,y1=area;slot=(y1-y0)/len(rows)
        for i,row in enumerate(rows):
            f=fit(d,row,((x1-x0)*.94,slot*.78),display=aid!='SIGN_RedCarTimetable')
            d.text(((x0+x1)/2,y0+slot*(i+.5)),row,font=f,anchor='mm',fill=ink)
    pages[page].paste(bg,(x,y))
    rec={'id':aid,'page':f'signs/T_SIGN_{page}.png','rect':[x/2048,y/2048,w/2048,h/2048],
         'text':text,'card_mesh':f'meshes/{aid}.fbx','nominal':size,'concept':[SOURCE],
         'prompts':{'background_receipt':'src/fal_receipts/HubSignBoards_flux3_klein.json',
                    'generated_background':background_job['prompt'],
                    'lettering':'Project OFL Limelight and Alegreya Sans Bold; deterministic spelling, SPEC10.1'},
         'font_sources':['hollywoodland-ue/Content/Data/Fonts/Limelight-Regular.ttf',
                         'hollywoodland-ue/Content/Data/Fonts/AlegreyaSans-Bold.ttf']}
    if hat:
        rec['profile_m']=[[-3.1,0],[3.1,0],[3.5,.15],[3.1,.4],[2,.4],[2,1],
                          [1.7,1.7],[1.05,2.3],[0,2.5],[-1.05,2.3],[-1.7,1.7],[-2,1],[-2,.4],[-3.1,.4],[-3.5,.15]]
        # Translate profile to the required bottom-left origin of its 7m span.
        rec['profile_m']=[[xx+3.5,zz] for xx,zz in rec['profile_m']]
        rec['nominal']=[7,2.5]
    records.append(rec)

card('SIGN_Pantages','02',(16,16,220,1980),[2.5,22.5],'PANTAGES',vertical=True,dark=True)
card('SIGN_Graumans_Blade','02',(252,16,480,960),[4,8],"GRAUMAN'S CHINESE",lines=["GRAUMAN'S",'C','H','I','N','E','S','E'],dark=True)
card('SIGN_Egyptian','02',(748,16,1260,60),[30,1.4],'EGYPTIAN')
card('SIGN_Egyptian_Gate','02',(748,92,1260,142),[11.5,1.3],'EGYPTIAN')
card('SIGN_Roosevelt','02',(748,250,1260,92),[24.6,1.8],'HOTEL ROOSEVELT',dark=True)
card('SIGN_HollywoodHotel','02',(748,358,1260,132),[10.5,1.1],'HOLLYWOOD HOTEL',dark=True)
card('SIGN_Graumans_Marquee','02',(748,506,1260,126),[10,1],"HELL'S ANGELS")
card('SIGN_Pantages_Marquee','02',(748,648,1260,64),[27,1],'')

card('SIGN_Taft','03',(16,16,1980,420),[8,1.7],'TAFT BUILDING')
card('SIGN_FirstNational','03',(16,452,1980,330),[6,1],'SECURITY FIRST NATIONAL BANK',lines=['SECURITY FIRST','NATIONAL BANK'])
card('SIGN_Police','03',(16,798,1980,223),[8,.9],'HOLLYWOOD POLICE')
card('SIGN_PacificElectric','03',(16,1037,1980,132),[4.2,.28],'PACIFIC ELECTRIC',dark=True)
card('SIGN_Egyptian_Marquee','03',(16,1185,1980,247),[10,1.25],'ROBIN HOOD · DOUGLAS FAIRBANKS',lines=['ROBIN HOOD','DOUGLAS FAIRBANKS'])
card('SIGN_BrownDerby','03',(16,1448,1008,360),[7,2.5],'THE BROWN DERBY',lines=['THE','BROWN','DERBY'],hat=True,dark=True)
card('SIGN_RedCarTimetable','03',(1040,1448,270,513),[.5,.95],'PACIFIC ELECTRIC / HOLLYWOOD / LOS ANGELES / DAILY SERVICE / ASK AGENT',lines=['PACIFIC','ELECTRIC','','HOLLYWOOD','LOS ANGELES','','DAILY SERVICE','','ASK AGENT'])

# Mechanical page/gutter validation; no accidental card overlap.
for page in ['02','03']:
    rs=[a for a in records if a['page'].endswith(page+'.png')]
    assert 4<=len(rs)<=12
    boxes=[]
    for a in rs:
        x,y,w,h=[round(v*2048) for v in a['rect']];assert x>=8 and y>=8 and x+w<=2040 and y+h<=2040
        assert all(x+w+8<=xx or xx+ww+8<=x or y+h+8<=yy or yy+hh+8<=y for xx,yy,ww,hh in boxes)
        boxes.append((x,y,w,h))
    meta=PngImagePlugin.PngInfo();meta.add(b'sRGB',b'\x00');pages[page].save(R/f'signs/T_SIGN_{page}.png',pnginfo=meta)
    thumb=pages[page].resize((1024,1024),Image.Resampling.LANCZOS);thumb.save(R/f'work/hub_sign_page_{page}_review.png')
(R/'work/hub_sign_records.json').write_text(json.dumps(records,indent=2)+'\n')
print('Composed',len(records),'correct-lettering cards on two generated-paint sign pages')
