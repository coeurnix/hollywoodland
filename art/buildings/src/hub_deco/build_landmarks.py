"""Authored phase3 Deco/civic landmark meshes; concepts must exist first.

Blender --background --python .../build_landmarks.py -- LM_Pantages
Outputs one authored mesh/material, source blend, own-UV texture input, metadata.
"""
import bpy,bmesh,math,json,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];OUT=R/'work/hub_deco';SRC=R/'src/hub_deco'
aid=sys.argv[sys.argv.index('--')+1]
for view in ['34','front']: assert (R/f'concept/{aid}_{view}.png').exists()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC'
V=[];F=[];UV=[]
def face(v,uv):
 base=len(V);V.extend(v);F.append(tuple(range(base,base+len(v))));UV.extend(uv)
def quad(v,role='wall',span=None):
 # Repeated facade bays/ornament intentionally stack. Roof has its own field.
 rects={'wall':(.715,.015,.985,.495),'upper':(.715,.515,.985,.985),
       'roof':(.008,.008,.692,.692),'stone':(.012,.715,.245,.985),
       'metal':(.265,.715,.365,.985),'glass':(.385,.715,.485,.985),
       'sign':(.505,.715,.695,.985),'bulb':(.65,.95,.68,.98)}
 if aid in ['LM_Taft','LM_FirstNational']:
  rects={'roof':(.008,.008,.62,.62),'wall':(.64,.008,.82,.12),
         'base':(.64,.15,.99,.45),'upper':(.64,.48,.99,.75),
         'stone':(.008,.65,.20,.99),'metal':(.22,.65,.32,.99),
         'glass':(.34,.65,.44,.99),'sign':(.46,.65,.62,.99),
         'bulb':(.84,.80,.98,.98)}
 rect=rects[role]
 u0,v0,u1,v1=rect
 face(v,[(u0,v0),(u1,v0),(u1,v1),(u0,v1)])
def box(x0,x1,y0,y1,z0,z1,role='stone',omit=()):
 vv=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
 for name,idx in [('front',(0,1,5,4)),('right',(1,2,6,5)),('back',(2,3,7,6)),('left',(3,0,4,7)),('top',(4,5,6,7)),('bottom',(3,2,1,0))]:
  if name not in omit:quad([vv[i] for i in idx],role)
def shell(w,d,h,nx,ny,toprole='roof'):
 # A complete single surface shell; no inaccessible bottom face.
 for i in range(nx):
  x0=-w/2+w*i/nx;x1=-w/2+w*(i+1)/nx
  quad([(x0,-d/2,0),(x1,-d/2,0),(x1,-d/2,h),(x0,-d/2,h)])
  quad([(x1,d/2,0),(x0,d/2,0),(x0,d/2,h),(x1,d/2,h)])
 for i in range(ny):
  y0=-d/2+d*i/ny;y1=-d/2+d*(i+1)/ny
  quad([(w/2,y0,0),(w/2,y1,0),(w/2,y1,h),(w/2,y0,h)])
  quad([(-w/2,y1,0),(-w/2,y0,0),(-w/2,y0,h),(-w/2,y1,h)])
 quad([(-w/2,-d/2,h),(w/2,-d/2,h),(w/2,d/2,h),(-w/2,d/2,h)],toprole)
def front_pier(x0,x1,y,z0,z1,depth=.32):
 box(x0,x1,y-depth,y,z0,z1,omit=('back','bottom','top'))
 # Cap/foot flare on the outer edge, with hidden attachment faces removed.
 box(x0,x1,y-depth-.08,y-depth,z0,z0+.35,omit=('back','bottom'))
 box(x0,x1,y-depth-.08,y-depth,z1-.45,z1,omit=('back','bottom'))
def roof_vent(x,y,z):
 box(x-1,x+1,y-1,y+1,z,z+1.7,'metal',omit=('bottom',))
 box(x-1.2,x+1.2,y-1.2,y+1.2,z+1.7,z+1.95,'stone',omit=('bottom',))
def front_arch(x,y,z,width,height,role='stone',pointed=False):
 # Visible radial arch trim; glazed opening below stays painted on wall.
 radius=width/2;th=.32;spring=z+height-radius
 points=[(-radius,z),(-radius,spring)]
 if pointed: points += [(-radius*.5,spring+radius*.7),(0,spring+radius*1.4),(radius*.5,spring+radius*.7)]
 else:points += [(radius*math.cos(t),spring+radius*math.sin(t)) for t in [math.pi*5/6,math.pi*2/3,math.pi/2,math.pi/3,math.pi/6]]
 points += [(radius,spring),(radius,z)]
 for (a,b),(c,d) in zip(points,points[1:]):
  dx,dz=c-a,d-b;ln=math.hypot(dx,dz);nx,nz=-dz/ln*th,dx/ln*th
  quad([(x+a,y,z0) for a,z0 in [(a,b),(c,d),(c+nx,d+nz),(a+nx,b+nz)]],role)
def pantes():
 w,d,h=56,58,18;shell(w,d,h,8,8)
 # Front fluted piers: large bold authored profiles, open against masonry.
 for x in [-27.5,-20.5,-13.5,12.5,19.5,26.5]:
  front_pier(x,x+1,-29,0,18)
  for shift in [.12,.42,.72]:box(x+shift,x+shift+.12,-29.43,-29.32,7.5,17.4,'stone',omit=('back','bottom','top'))
 # The central stepped architectural crown sits above the required box.
 for x0,x1,z in [(-12,12,19),(-9,9,20),(-6,6,21),(-3,3,22)]:
  box(x0,x1,-29.25,-28.65,18,z,'stone',omit=('bottom','back'))
 # Wide shallow marquee, permitted1.5m street projection, lower edge5.2m.
 box(-14,14,-30.35,-29,5.2,6.7,'metal',omit=('back',))
 # Blank faces for independent sign cards; source has no lettering.
 quad([(-13.5,-30.365,5.45),(13.5,-30.365,5.45),(13.5,-30.365,6.45),(-13.5,-30.365,6.45)],'sign')
 # Bulb pyramids along chaser rails, all outward faces only.
 for z in [5.3,6.6]:
  for i in range(39):
   x=-13.3+i*.7;r=.10
   tip=(x,-30.49,z)
   pts=[(x-r,-30.355,z-r),(x+r,-30.355,z-r),(x+r,-30.355,z+r),(x-r,-30.355,z+r)]
   for k in range(4):face([pts[k],pts[(k+1)%4],tip],[(.65,.95),(.68,.95),(.665,.98)])
 box(-1.5,1.5,-30.45,-29.95,6.8,32,'metal',omit=('bottom',))
 quad([(-1.25,-30.46,8),(1.25,-30.46,8),(1.25,-30.46,30.5),(-1.25,-30.46,30.5)],'sign')
 # Blade crest geometric sunburst silhouette rather than baked typography.
 for x,z in [(-1.5,32),(-.75,33),(0,34),(.75,33),(1.5,32)]:
  box(x-.18,x+.18,-30.45,-30,31.8,z+.3,'metal',omit=('bottom',))
 for x in [-12,-4,4,12]:
  box(x-.12,x+.12,-29.16,-29.01,.4,4.7,'metal',omit=('bottom','back','top'))
 for x,y in [(-12,5),(12,16)]:roof_vent(x,y,18)
 return [w,d,h],[{'id':'SIGN_Pantages','rect':[[-1.25,-30.47,8],[1.25,-30.47,30.5]],'normal':[0,-1,0],'size_m':[2.5,22.5]}, {'id':'SIGN_Pantages_Marquee','rect':[[-13.5,-30.38,5.45],[13.5,-30.38,6.45]],'normal':[0,-1,0],'size_m':[27,1]}]

def tiled_wall(start,end,z0,z1,count,role):
 for i in range(count):
  a=Vector(start).lerp(Vector(end),i/count);b=Vector(start).lerp(Vector(end),(i+1)/count)
  quad([(a.x,a.y,z0),(b.x,b.y,z0),(b.x,b.y,z1),(a.x,a.y,z1)],role)
def perimeter(w,d,z0,z1,role,nx=10,ny=12):
 tiled_wall((-w/2,-d/2),(w/2,-d/2),z0,z1,nx,role)
 tiled_wall((w/2,-d/2),(w/2,d/2),z0,z1,ny,role)
 tiled_wall((w/2,d/2),(-w/2,d/2),z0,z1,nx,role)
 tiled_wall((-w/2,d/2),(-w/2,-d/2),z0,z1,ny,role)
def west_arch(y,z,width,height,pointed=False):
 old=len(V);front_arch(y,21,z,width,height,pointed=pointed)
 # Rotate the front-facing local arch90deg clockwise to the west facade.
 for i in range(old,len(V)):
  x0,y0,z0=V[i];V[i]=(-y0,x0,z0)
def taft():
 w,d,h=42,48,36
 perimeter(w,d,0,7.5,'base')
 for floor in range(8):
  z=7.5+floor*2.8125;perimeter(w,d,z,z+2.8125,'wall')
  # Front and Vine-street stone sills. Hidden wall-facing backs removed.
  for i in range(10):
   x=-21+(i+.5)*4.2;box(x-1,x+1,-24.25,-24,z+.65,z+.85,'stone',omit=('back','bottom'))
  for i in range(12):
   y=-24+(i+.5)*4;box(-21.25,-21,y-1,y+1,z+.65,z+.85,'stone',omit=('right','bottom'))
 perimeter(w,d,30,36,'upper')
 quad([(-21,-24,36),(21,-24,36),(21,24,36),(-21,24,36)],'roof')
 # Heavy crown and lower entablatures around the street facades.
 for z,thick,dep in [(4.4,.25,.25),(7.2,.3,.3),(29.8,.3,.25),(35.2,.3,.4),(35.65,.35,.5)]:
  box(-21,21,-24-dep,-24,z,z+thick,'stone',omit=('back','bottom'))
  box(-21-dep,-21,-24,24,z,z+thick,'stone',omit=('right','bottom'))
 for i in range(11):front_pier(max(-21,-21+i*4.2-.14), min(21,-21+i*4.2+.14),-24,30.2,35.1,.25)
 for i in range(12):
  y=-24+(i+.5)*4;box(-21.22,-21,y-.14,y+.14,30.2,35.1,'stone',omit=('right','top','bottom'))
 for i in range(10):front_arch(-21+(i+.5)*4.2,-24.05,.25,3.35,3.9)
 for i in range(12):
  # Same arc trim on secondary street; local facade x21 maps to westx-21.
  old=len(V);front_arch(-24+(i+.5)*4,21.05,.25,3.2,3.9)
  for k in range(old,len(V)):
   xx,yy,zz=V[k];V[k]=(-yy,xx,zz)
 # Cornice dentils read in silhouette from the player's high camera.
 for i in range(42):
  x=-20.5+i;box(x-.20,x+.20,-24.42,-24,34.9,35.2,'stone',omit=('back','top'))
 for i in range(48):
  y=-23.5+i;box(-21.42,-21,y-.20,y+.20,34.9,35.2,'stone',omit=('right','top'))
 box(-4,4,4,9,36,39,'stone',omit=('bottom',));roof_vent(-12,13,36);roof_vent(12,-4,36)
 quad([(-4,-24.32,4.8),(4,-24.32,4.8),(4,-24.32,6.5),(-4,-24.32,6.5)],'sign')
 return [w,d,h],[{'id':'SIGN_Taft','rect':[[-4,-24.34,4.8],[4,-24.34,6.5]],'normal':[0,-1,0],'size_m':[8,1.7]}]

def bank_wall(start,end,h,base=True,door=False):
 length=(Vector(end)-Vector(start)).length;count=max(1,round(length/4))
 if base:
  for i in range(count):
   a=Vector(start).lerp(Vector(end),i/count);b=Vector(start).lerp(Vector(end),(i+1)/count)
   if door and i==1:
    # The actual entrance is0.75m recessed, not painted over a solid front.
    quad([(a.x,a.y+.75,0),(b.x,b.y+.75,0),(b.x,b.y+.75,5),(a.x,a.y+.75,5)],'glass')
    quad([(a.x,a.y,5),(b.x,b.y,5),(b.x,b.y,6),(a.x,a.y,6)],'stone')
    quad([(a.x,a.y,0),(a.x,a.y+.75,0),(a.x,a.y+.75,5),(a.x,a.y,5)],'stone')
    quad([(b.x,b.y+.75,0),(b.x,b.y,0),(b.x,b.y,5),(b.x,b.y+.75,5)],'stone')
    quad([(a.x,a.y,5),(a.x,a.y+.75,5),(b.x,b.y+.75,5),(b.x,b.y,5)],'stone')
   else:quad([(a.x,a.y,0),(b.x,b.y,0),(b.x,b.y,6),(a.x,a.y,6)],'base')
 z=6
 while z<h-3:
  nz=min(z+3,h-3);tiled_wall(start,end,z,nz,count,'wall');z=nz
 tiled_wall(start,end,h-3,h,count,'upper')
def pyramid(x,y,z,r,height,role='stone'):
 pts=[(x-r,y-r,z),(x+r,y-r,z),(x+r,y+r,z),(x-r,y+r,z)];tip=(x,y,z+height)
 for k in range(4):face([pts[k],pts[(k+1)%4],tip],[(.01,.75),(.18,.75),(.095,.98)])
def firstnational():
 w,d,h=42,38,22;xs=[-21,-9,7,21];ys=[-19,-7,9,19]
 heights=[[22,19,16],[19,16,16],[16,16,16]]
 # Height-field surface: only exposed step faces and roofs, no interior boxes.
 for i in range(3):
  for j in range(3):
   x0,x1=xs[i:i+2];y0,y1=ys[j:j+2];z=heights[i][j]
   quad([(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)],'roof')
   if j==0:bank_wall((x0,y0),(x1,y0),z,door=i==0)
   if i==0:bank_wall((x0,y1),(x0,y0),z)
   if j==2:bank_wall((x1,y1),(x0,y1),z)
   if i==2:bank_wall((x1,y0),(x1,y1),z)
   if i<2 and z>heights[i+1][j]:tiled_wall((x1,y0),(x1,y1),heights[i+1][j],z,3,'upper')
   if j<2 and z>heights[i][j+1]:tiled_wall((x1,y1),(x0,y1),heights[i][j+1],z,3,'upper')
 # Street banking arches and bold vertical buttress piers.
 for i in range(10):
  x=-19+(i)*4;top=22 if x<-9 else 19 if x<7 else 16
  if i!=1:front_arch(x,-19.05,.4,3.25,5.2)
  front_pier(x-1.9,x-1.55,-19,0,top+.4,.30)
  pyramid(x-1.72,-19.2,top+.4,.22,.7)
 for i in range(9):
  y=-17+i*4;top=22 if y<-7 else 19 if y<9 else 16
  old=len(V);front_arch(y,21.05,.4,3.2,5.2)
  for k in range(old,len(V)):
   xx,yy,zz=V[k];V[k]=(-yy,xx,zz)
  box(-21.3,-21,y-1.9,y-1.55,0,top+.4,'stone',omit=('right','bottom','top'))
  pyramid(-21.2,y-1.72,top+.4,.22,.7)
 # Wing parapet panels deliberately have a scalloped Gothic top contour.
 for x,top in [(x,19 if x<7 else 16) for x in [-7,-3,1,5,9,13,17]]:
  pts=[(x-1.8,-19.08,top-.35),(x+1.8,-19.08,top-.35),(x+1.8,-19.08,top+.7),(x+.9,-19.08,top+.2),(x,-19.08,top+.05),(x-.9,-19.08,top+.2),(x-1.8,-19.08,top+.7)]
  face(pts,[(.008+(p[0]-(x-1.8))/3.6*.192,.65+(p[2]-top+.35)/1.05*.34) for p in pts])
 # Pyramidal historic corner tower roof, no base/internal faces.
 pyramid(-15,-13,22,5.65,5,'metal');pyramid(-15,-13,27,.18,1,'metal')
 # Open belfry-niche outlines on both street faces; figures stay painted relief.
 for x in [-18,-14,-10]:front_arch(x,-19.1,18.1,2.3,3.5,pointed=True)
 for y in [-16,-12,-8]:
  old=len(V);front_arch(y,21.1,18.1,2.3,3.5,pointed=True)
  for k in range(old,len(V)):
   xx,yy,zz=V[k];V[k]=(-yy,xx,zz)
 # Corner clock disk/rim, no words/numerals, with authored brass hands.
 cx,cy,cz=-19.4,-20.0,6.4;n=24
 for i in range(n):
  a=2*math.pi*i/n;b=2*math.pi*(i+1)/n
  p=(cx+.8*math.cos(a),cy,cz+.8*math.sin(a));q=(cx+.8*math.cos(b),cy,cz+.8*math.sin(b))
  face([(cx,cy,cz),q,p],[(.84,.8),(.98,.8),(.91,.98)])
  quad([p,q,(cx+.96*math.cos(b),cy,cz+.96*math.sin(b)),(cx+.96*math.cos(a),cy,cz+.96*math.sin(a))],'metal')
 box(cx-.045,cx+.045,cy-.035,cy-.01,cz,cz+.64,'metal',omit=('back','bottom'))
 box(cx-.48,cx+.04,cy-.035,cy-.01,cz-.04,cz+.04,'metal',omit=('back','bottom'))
 for x,y,z in [(12,10,16),(1,13,16),(10,-10,16)]:roof_vent(x,y,z)
 quad([(-18,-19.34,5.3),(-12,-19.34,5.3),(-12,-19.34,6.3),(-18,-19.34,6.3)],'sign')
 return[w,d,h],[{'id':'SIGN_FirstNational','rect':[[-18,-19.36,5.3],[-12,-19.36,6.3]],'normal':[0,-1,0],'size_m':[6,1]}]

def low_shell(w,d,h,door_w=6):
 # Intact shell with a shallow real central entrance recess.
 xx=[-w/2,-door_w/2,door_w/2,w/2]
 for x0,x1 in [(xx[0],xx[1]),(xx[2],xx[3])]:
  count=max(1,round((x1-x0)/4));tiled_wall((x0,-d/2),(x1,-d/2),0,h,count,'wall')
 x0,x1=-door_w/2,door_w/2;y=-d/2;recess=.65
 quad([(x0,y+recess,0),(x1,y+recess,0),(x1,y+recess,4.5),(x0,y+recess,4.5)],'glass')
 quad([(x0,y,4.5),(x1,y,4.5),(x1,y,h),(x0,y,h)],'stone')
 quad([(x0,y,0),(x0,y+recess,0),(x0,y+recess,4.5),(x0,y,4.5)],'stone')
 quad([(x1,y+recess,0),(x1,y,0),(x1,y,4.5),(x1,y+recess,4.5)],'stone')
 quad([(x0,y,4.5),(x0,y+recess,4.5),(x1,y+recess,4.5),(x1,y,4.5)],'stone')
 tiled_wall((w/2,-d/2),(w/2,d/2),0,h,max(1,round(d/4)),'wall')
 tiled_wall((w/2,d/2),(-w/2,d/2),0,h,max(1,round(w/4)),'wall')
 tiled_wall((-w/2,d/2),(-w/2,-d/2),0,h,max(1,round(d/4)),'wall')
 quad([(-w/2,-d/2,h),(w/2,-d/2,h),(w/2,d/2,h),(-w/2,d/2,h)],'roof')
def extrude_profile(profile,yfront,yback,role='stone'):
 # Both exposed sides, no hidden bottom; profile coordinates x,z.
 front=[(x,yfront,z) for x,z in profile];back=[(x,yback,z) for x,z in profile]
 # Fan UVs use the role's dedicated stone/small-sign field.
 for pts in [front,list(reversed(back))]:
  xmin=min(p[0] for p in pts);xmax=max(p[0] for p in pts);zmin=min(p[2] for p in pts);zmax=max(p[2] for p in pts)
  rect=(.012,.715,.245,.985) if role=='stone' else(.505,.715,.695,.985)
  face(pts,[(rect[0]+(p[0]-xmin)/(xmax-xmin)*(rect[2]-rect[0]),rect[1]+(p[2]-zmin)/(zmax-zmin)*(rect[3]-rect[1])) for p in pts])
 for i in range(1,len(profile)):
  j=(i+1)%len(profile);quad([front[i],front[j],back[j],back[i]],role)
def brownderby():
 w,d,h=30,24,8;low_shell(w,d,h)
 for x in [-14.75,-3.5,3,14.25]:front_pier(x,x+.5,-12,0,8,.22)
 # Low barrel-tile pent roof silhouette, painted tile repeat rather than tubes.
 quad([(-15,-12.04,7.3),(15,-12.04,7.3),(15,-13.25,6.6),(-15,-13.25,6.6)],'upper')
 quad([(-15,-13.25,6.6),(15,-13.25,6.6),(15,-13.25,6.4),(-15,-13.25,6.4)],'upper')
 profile=[(-15,8),(15,8),(15,8.3),(11,8.3),(8,8.5),(6,9.0),(4,9.1),(3,9.5),(2,10.0),(0,10.35),(-2,10),(-3,9.5),(-4,9.1),(-6,9),(-8,8.5),(-11,8.3),(-15,8.3)]
 extrude_profile(profile,-12.08,-11.65)
 for x in [-10.3,-5.1,5.1,10.3]:
  front_arch(x,-12.07,.6,4.35,4.7)
  quad([(x-2.15,-12.08,4.6),(x+2.15,-12.08,4.6),(x+2.15,-13.20,3.8),(x-2.15,-13.20,3.8)],'metal')
  quad([(x-2.15,-13.20,3.8),(x+2.15,-13.20,3.8),(x+2.15,-13.20,3.55),(x-2.15,-13.20,3.55)],'metal')
 front_arch(0,-12.12,0,5.9,5.2)
 # Small hat SIGN backing, never a hat-shaped building mass.
 for x in [-1.7,1.7]:box(x-.08,x+.08,-8,-7.8,8,11.1,'metal',omit=('bottom',))
 hat=[(-3.2,10.9),(3.2,10.9),(3.6,11.15),(3.2,11.4),(2.1,11.4),(2.1,12),(1.8,12.7),(1.1,13.3),(0,13.55),(-1.1,13.3),(-1.8,12.7),(-2.1,12),(-2.1,11.4),(-3.2,11.4),(-3.6,11.15)]
 extrude_profile(hat,-8.1,-7.75,'sign');roof_vent(7,5,8)
 return[w,d,h],[{'id':'SIGN_BrownDerby','rect':[[-3.1,-8.12,11],[3.1,-8.12,13.5]],'normal':[0,-1,0],'size_m':[6.2,2.5],'notes':'Small independent hat-shaped roof sign; local front-Y, integrator rotates toward Vine.'}]
def pole(x,y,z,r,height,role='metal',n=8):
 for i in range(n):
  a=2*math.pi*i/n;b=2*math.pi*(i+1)/n
  quad([(x+r*math.cos(a),y+r*math.sin(a),z),(x+r*math.cos(b),y+r*math.sin(b),z),(x+r*math.cos(b),y+r*math.sin(b),z+height),(x+r*math.cos(a),y+r*math.sin(a),z+height)],role)
def policestation():
 w,d,h=46,34,10;low_shell(w,d,h)
 # Cast-stone belts and cornice remain within width/depth, front projection0.35m.
 for z,thick in [(0,.7),(4.5,.25),(9.15,.35),(9.7,.3)]:
  if z==0:
   for x0,x1 in [(-23,-3),(3,23)]:box(x0,x1,-17.28,-17,z,z+thick,'stone',omit=('back','bottom'))
  else:box(-23,23,-17.35,-17,z,z+thick,'stone',omit=('back','bottom'))
 for x in [-20.5,-15,-9,9,15,20.5]:
  front_arch(x,-17.08,.8,3.6,3.4)
  # Bold lower security bars, not transparency or baked modern graphics.
  for off in [-1.15,-.58,0,.58,1.15]:box(x+off-.025,x+off+.025,-17.13,-17.08,1.05,3.25,'metal',omit=('back','bottom','top'))
  box(x-1.5,x+1.5,-17.2,-17,5.7,5.9,'stone',omit=('back','bottom'))
 for x in [-3.4,2.9]:front_pier(x,x+.5,-17,0,5.3,.25)
 front_arch(0,-17.15,0,6.1,5.9)
 # Arched pediment above the door; chunky low-poly profile and matching back.
 ped=[(-4.5,5.2),(4.5,5.2),(4.5,5.65),(3,5.8),(1.7,6.5),(0,6.9),(-1.7,6.5),(-3,5.8),(-4.5,5.65)]
 extrude_profile(ped,-17.26,-17.01)
 for x in [-4.4,4.4]:
  box(x-.45,x+.45,-17.3,-17,0,.85,'stone',omit=('bottom','back'));pole(x,-17.14,.85,.14,1.85)
  # Octagonal opaque globe at2.65m minimum street clearance.
  box(x-.07,x+.07,-17.5,-17.14,2.65,2.75,'metal',omit=('bottom',))
  pole(x,-17.5,2.65,.36,.65,'glass');box(x-.40,x+.40,-17.90,-17.10,3.3,3.4,'metal',omit=('bottom',))
 pole(0,-10,10,.075,4.5);pyramid(0,-10,14.5,.12,.35,'metal')
 for x in range(-21,22,3):box(x-.30,x+.30,-17.28,-17,8.85,9.15,'stone',omit=('back','top'))
 quad([(-4,-17.365,7.7),(4,-17.365,7.7),(4,-17.365,8.6),(-4,-17.365,8.6)],'sign')
 return[w,d,h],[{'id':'SIGN_Police','rect':[[-4,-17.38,7.7],[4,-17.38,8.6]],'normal':[0,-1,0],'size_m':[8,.9]}]

BUILD={'LM_Pantages':pantes,'LM_Taft':taft,'LM_FirstNational':firstnational,'LM_BrownDerby':brownderby,'LM_PoliceStation':policestation}
nominal,signs=BUILD[aid]()
mesh=bpy.data.meshes.new(aid);mesh.from_pydata(V,[],F);mesh.update();layer=mesh.uv_layers.new(name='UVMap')
for l in mesh.loops:layer.data[l.index].uv=UV[l.vertex_index]
obj=bpy.data.objects.new(aid,mesh);bpy.context.collection.objects.link(obj)
bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True
image=bpy.data.images.new('T_'+aid+'_Input',width=2048,height=2048,alpha=False);image.generated_color=(.64,.49,.29,1)
node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=image
shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED');mat.node_tree.links.new(node.outputs['Color'],shader.inputs['Base Color']);mesh.materials.append(mat)
obj.select_set(True);bpy.context.view_layer.objects.active=obj
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bm=bmesh.new();bm.from_mesh(mesh)
audit={'tris':len(mesh.polygons),'bounds':[[min(v.co[k] for v in mesh.vertices) for k in range(3)],[max(v.co[k] for v in mesh.vertices) for k in range(3)]],'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'material_slots':len(mesh.materials),'boundary_edges':sum(e.is_boundary for e in bm.edges)};bm.free()
if audit['branch_edges']:
 bm=bmesh.new();bm.from_mesh(mesh);print('BRANCH_COORDS',[[list(v.co) for v in e.verts] for e in bm.edges if len(e.link_faces)>2]);bm.free()
assert audit['tris']<=20000 and audit['zero_area_faces']==audit['branch_edges']==audit['loose_vertices']==0,audit
bpy.ops.export_scene.gltf(filepath=str(R/f'work/{aid}_input.glb'),export_format='GLB',use_selection=True)
meta={'id':aid,'kind':'landmark','nominal':nominal,'origin':'footprint-centre-ground','front':'-Y','up':'+Z','units':'meters','source':f'src/hub_deco/{aid}.blend','object':aid,'material':'M_'+aid,'input_glb':f'work/{aid}_input.glb','concept':[f'concept/{aid}_34.png',f'concept/{aid}_front.png'],'prompts':f'src/hub_deco/{aid}_prompt.json','sign_attachment_rectangles':signs,'audit':audit,'notes':'Own authored geometry and UVs; repeated facade bays intentionally stacked at54-80px/m; roof own1390px field approximately24px/m. Source photo consulted for architecture only; no photographic texture or baked lettering. Open attachment faces omit hidden backs; bounds include permitted street marquee and roof blade.'}
(SRC/f'{aid}_metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(SRC/f'{aid}.blend'))
qpath=SRC/'texture_request_queue.json';queue=json.loads(qpath.read_text()) if qpath.exists() else []
queue=[q for q in queue if q['id']!=aid];queue.append({'id':aid,'input':f'work/{aid}_input.glb','reference':f'concept/{aid}_34.png','second_reference':f'concept/{aid}_front.png','metadata':f'src/hub_deco/{aid}_metadata.json','material':'Shaded','resolution':'Basic','status':'ready'})
qpath.write_text(json.dumps(queue,indent=2)+'\n');print('READY',aid,json.dumps(audit))
