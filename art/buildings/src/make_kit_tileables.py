"""Period wall and roof masters, painted procedurally with periodic brush fields.
Tileables are explicitly exempt from Rodin in SPEC 9.4.
"""
import numpy as np,json
from pathlib import Path
from PIL import Image,PngImagePlugin
R=Path(__file__).resolve().parents[1];N=2048
y,x=np.mgrid[0:N,0:N];xx=x/N;yy=y/N
# Every field is periodic in both axes. No random independent image-edge noise.
brush=(np.sin(xx*2*np.pi*37+np.sin(yy*2*np.pi*17))*1.2+np.cos(yy*2*np.pi*53+np.sin(xx*2*np.pi*23))*.8+np.sin((xx*19+yy*29)*2*np.pi)*.7)
meta=PngImagePlugin.PngInfo();meta.add(b'sRGB',b'\x00')
out={}
def save(name,color,tile):
 p='textures/T_TILE_'+name+'.png';Image.fromarray(np.uint8(np.clip(color,0,255)),'RGB').save(R/p,pnginfo=meta);out['TILE_'+name]={'texture':p,'tile_m':tile}
def brick(name,base,rows=16,cols=8):
 row=np.floor(yy*rows).astype(int);u=(xx*cols+(row%2)*.5)%1;v=(yy*rows)%1
 mortar=(u<.028)|(v<.05)
 brick_id=np.floor(xx*cols+(row%2)*.5).astype(int)%cols
 shade=brush*2+np.sin(brick_id*4.53+row*1.37)*6
 shade+=(v<.15)*7-(v>.84)*9
 c=np.array(base,dtype=float)[None,None,:]+shade[:,:,None]
 c[mortar]=np.array([129,111,87])+brush[mortar,None]
 save(name,c,[4,4])
brick('Brick_Red',[139,76,48]);brick('Brick_Sepia',[112,84,64]);brick('Brick_Buff',[177,150,101])
for name,base in [('Stucco_White',[218,205,171]),('Stucco_Cream',[199,175,129])]:
 c=np.array(base)[None,None,:]+brush[:,:,None]*2.4+np.sin((xx*5+yy*3)*2*np.pi)[:,:,None]*1.2;save(name,c,[4,4])
c=np.array([70,65,62])[None,None,:]+brush[:,:,None]*2.5+np.sin(xx*2*np.pi*113)[:,:,None]*np.cos(yy*2*np.pi*139)[:,:,None]*4
save('Roof_TarGravel',c,[4,4])
u=(xx*16)%1;v=(yy*16)%1
shade=np.cos((u-.5)*np.pi)*21-(u<.045)*32-(v<.045)*29+(v<.12)*6+brush*1.7
c=np.array([154,75,43])[None,None,:]+shade[:,:,None];save('Roof_RedTile',c,[4,4])
(R/'work/kit_tileables.json').write_text(json.dumps(out,indent=2)+'\n')
print('Painted',len(out),'periodic 2K masters; tile size 4 x 4 m')
