"""Authored facade geometry. Run only after all BRK concepts have been reviewed.
Builds a combined texture-only input with overlapping reusable UV islands.
Final export and source-to-own-UV bake are separate steps.
"""
import bpy,bmesh,json,math,sys
from mathutils import Vector
from pathlib import Path
R=Path(__file__).resolve().parents[1]
FAM=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'BRK'
plan=[a for a in json.loads((R/'src/kit_plan.json').read_text()) if a['family']==FAM]
missing=[a['id']+'_'+v for a in plan for v in ['front','34'] if not (R/'concept'/f"{a['id']}_{v}.png").exists()]
if missing:raise RuntimeError('Concepts incomplete: '+', '.join(missing))
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
m=bpy.data.materials.new('M_ATL_'+FAM);m.use_nodes=True
atlas_path=R/f'textures/T_ATL_{FAM}.png'
if atlas_path.exists():im=bpy.data.images.load(str(atlas_path))
else:
 im=bpy.data.images.new('Untextured_'+FAM,width=2048,height=2048,alpha=False);im.generated_color=(.65,.55,.35,1)
t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=im
p=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED');m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color'])
def uvground(x,z):return ((16+200*x)/2048,(16+200*z)/2048)
def uvupper(x,z):return ((1248+200*x)/2048,(16+200*z)/2048)
def crest_outline(aid,w,h):
 # Trace only the large, quarter-metre silhouette from the generated front concept.
 import numpy as np
 im=bpy.data.images.load(str(R/f'concept/{aid}_front.png'),check_existing=True)
 pixels=np.empty(im.size[0]*im.size[1]*4,dtype=np.float32);im.pixels.foreach_get(pixels);pixels=pixels.reshape(im.size[1],im.size[0],4)[:,:,:3]
 mask=(pixels.max(axis=2)-pixels.min(axis=2)>.065)&(pixels.max(axis=2)>.1)
 yy,xx=np.where(mask);xmin,xmax=xx.min(),xx.max();ymin,ymax=yy.min(),yy.max();half=max(2,int((xmax-xmin)/32))
 top=[]
 for i in range(16,-1,-1):
  cx=round(xmin+(xmax-xmin)*i/16);local=mask[:,max(0,cx-half):min(mask.shape[1],cx+half+1)];active=np.where(local)[0]
  z=h*(active.max()-ymin)/max(1,ymax-ymin) if len(active) else .25
  top.append((w*i/16,max(.25,min(h,round(z*4)/4))))
 compact=[]
 for p in top:
  while len(compact)>1:
   a,b=compact[-2:];cross=(b[0]-a[0])*(p[1]-b[1])-(b[1]-a[1])*(p[0]-b[0])
   if abs(cross)>1e-8:break
   compact.pop()
  compact.append(p)
 return [(0,0),(w,0)]+compact
def build(a):
 aid=a['id'];w,h=a['nominal'];verts=[];faces=[];uvs=[]
 def face(v,uv):
  first=len(verts);verts.extend(v);faces.append(tuple(range(first,first+len(v))));uvs.extend(uv)
 def panel(x0,x1,z0,z1,y0,y1,ux0,ux1,uz0,uz1,upper=False):
  uv=uvupper if upper else uvground
  face([(x0,y0,z0),(x1,y0,z0),(x1,y1,z1),(x0,y1,z1)],[uv(ux0,uz0),uv(ux1,uz0),uv(ux1,uz1),uv(ux0,uz1)])
 upper='_U_' in aid or aid.endswith('_U') or 'Damaged_U' in aid
 if '_G_' in aid or 'Damaged_G' in aid:
  zz=[0,.2,.75,2.9,3.2,3.55,3.75,4.05,4.3,4.5]
  if aid.endswith('B'):
   xx=[0,.5,w/2-.75,w/2+.75,w-.5,w];sample=[(0,.5),(.5,3.9),(4.15,5.45),(.5,3.9),(5.45,6)]
  elif 'Entry' in aid:
   xx=[0,.5,w-.5,w];sample=[(0,.5),(4.15,5.45),(5.45,6)]
  else:xx=[0,.5,w-1.85,w-1.6,w-.55,w];sample=[(0,.5),(.5,3.9),(3.9,4.15),(4.15,5.45),(5.45,6)]
  for j in range(len(zz)-1):
   z0,z1=zz[j:j+2]
   def depth(z):return -.25 if z>=4.3 else (-.125 if z>=3.75 else 0)
   for i,(u0,u1) in enumerate(sample):
    y0,y1=depth(z0),depth(z1)
    if 'Entry' in aid and i==1 and z1<=3.55:y0=y1=1
    panel(xx[i],xx[i+1],z0,z1,y0,y1,u0,u1,z0,z1)
  if 'Entry' in aid:
   for x,flip in [(.5,False),(w-.5,True)]:
    for z0,z1 in zip(zz[:5],zz[1:6]):
     vs=[(x,0,z0),(x,1,z0),(x,1,z1),(x,0,z1)]
     if flip:vs.reverse()
     face(vs,[uvground(0,z0),uvground(.5,z0),uvground(.5,z1),uvground(0,z1)])
   face([(.5,0,3.55),(w-.5,0,3.55),(w-.5,1,3.55),(.5,1,3.55)],[uvground(0,3.55),uvground(3,3.55),uvground(3,3.75),uvground(0,3.75)])
 elif '_U_' in aid or 'Damaged_U' in aid:
  if FAM=='SPN':
   xx=sorted(set([0,.25,w-.25,w]+([w/2] if aid.endswith('B') else [])));zz=[0,1,1.25,3,3.25,3.5]
   for j in range(len(zz)-1):
    for i in range(len(xx)-1):
     z0,z1=zz[j:j+2];x0,x1=xx[i:i+2]
     if j==1 and x0>=.25 and x1<=w-.25:continue
     root=0 if x1<=w/2 else w/2
     u0,u1=(((x0-root)/(w/2)*3,(x1-root)/(w/2)*3) if aid.endswith('B') else (x0/w*3,x1/w*3))
     panel(x0,x1,z0,z1,0,0,u0,u1,z0,z1,True)
   # Open-backed balcony ledge, with its own top/side UV patches.
   top=[(.25,-.4,1.25),(w-.25,-.4,1.25),(w-.25,0,1.25),(.25,0,1.25)]
   face(top,[(1080/2048,1400/2048),(1580/2048,1400/2048),(1580/2048,1480/2048),(1080/2048,1480/2048)])
   panel(.25,w-.25,1,1.25,-.4,-.4,.25,2.75,1,1.25,True)
   for x,flip in [(.25,False),(w-.25,True)]:
    v=[(x,0,1),(x,-.4,1),(x,-.4,1.25),(x,0,1.25)]
    if flip:v.reverse()
    face(v,[(1080/2048,1520/2048),(1160/2048,1520/2048),(1160/2048,1570/2048),(1080/2048,1570/2048)])
   # A partitioned wrought-iron frame: only exterior faces, no intersecting beam solids.
   x0,x1=.25,w-.25;centres=[x0+(x1-x0)*k/6 for k in range(1,6)]
   grid=sorted([x0,x0+.06,x1-.06,x1]+[c+d for c in centres for d in [-.025,.025]])
   levels=[1.25,1.32,1.68,1.75];filled={}
   for j in range(3):
    for i in range(len(grid)-1):
     mid=(grid[i]+grid[i+1])/2;filled[i,j]=(j!=1 or mid<x0+.061 or mid>x1-.061 or any(abs(mid-c)<.026 for c in centres))
   def metal(v):
    a,b,c,d=map(Vector,v);u=(b-a).length*200;vlen=(d-a).length*200
    if abs(b.x-a.x)>.00001:u*=2.5/(w-.5)
    face(v,[(1208/2048,1528/2048),((1208+u)/2048,1528/2048),((1208+u)/2048,(1528+vlen)/2048),(1208/2048,(1528+vlen)/2048)])
   for (i,j),solid in filled.items():
    if not solid:continue
    xl,xr=grid[i:i+2];zl,zh=levels[j:j+2]
    metal([(xl,-.43,zl),(xr,-.43,zl),(xr,-.43,zh),(xl,-.43,zh)])
    metal([(xr,-.37,zl),(xl,-.37,zl),(xl,-.37,zh),(xr,-.37,zh)])
    if not filled.get((i-1,j),False):metal([(xl,-.37,zl),(xl,-.43,zl),(xl,-.43,zh),(xl,-.37,zh)])
    if not filled.get((i+1,j),False):metal([(xr,-.43,zl),(xr,-.37,zl),(xr,-.37,zh),(xr,-.43,zh)])
    if not filled.get((i,j+1),False):metal([(xl,-.43,zh),(xr,-.43,zh),(xr,-.37,zh),(xl,-.37,zh)])
    if j>0 and not filled.get((i,j-1),False):metal([(xl,-.37,zl),(xr,-.37,zl),(xr,-.43,zl),(xl,-.43,zl)])
  else:
   for j,(z0,z1,y0,y1) in enumerate([(0,3,0,0),(3,3.25,0,-.125),(3.25,3.5,-.125,-.125)]):
    if aid.endswith('B'):
     for x0 in [0,w/2]:panel(x0,x0+w/2,z0,z1,y0,y1,0,3,z0,z1,True)
    else:panel(0,w,z0,z1,y0,y1,0,3,z0,z1,True)
 elif 'Pilaster' in aid:
  panel(0,1,0,h,-.125,-.125,0,.5,0,4.5)
 elif 'Corner' in aid:
  face([(0,0,0),(.5,0,0),(.5,0,h),(0,0,h)],[uvground(0,0),uvground(.5,0),uvground(.5,4.5),uvground(0,4.5)])
  face([(.5,0,0),(.5,.5,0),(.5,.5,h),(.5,0,h)],[uvground(0,0),uvground(.5,0),uvground(.5,4.5),uvground(0,4.5)])
 elif 'Parapet' in aid:
  if FAM in ['MSN','DECO']:
   top=([(w,1.05),(w*.75,1.05),(w*.75,1.5),(w*.25,1.5),(w*.25,1.05),(0,1.05)] if FAM=='DECO' else [(round((w/2+w/2*math.cos(i*math.pi/4))*4)/4,1.05+.45*math.sin(i*math.pi/4)) for i in range(5)])
   outline=[(0,0),(w,0)]+top
   face([(x,0,z) for x,z in outline],[uvground(.5+x/w*3.4,3.3+z/1.5*.9) for x,z in outline])
  else:
   panel(0,w,0,1.05,0,0,.5,3.9,3.3,3.55)
   panel(0,w,1.05,1.25,0,-.25,0,6,3.75,4.05)
   panel(0,w,1.25,1.5,-.25,-.25,0,6,4.05,4.5)
 elif 'Crest' in aid:
  outline=[(0,0),(w,0),(w,1),(w*.75,1),(w*.75,1.5),(w*.6,1.5),(w*.6,2),(w*.4,2),(w*.4,1.5),(w*.25,1.5),(w*.25,1),(0,1)]
  if aid.endswith('A'):outline=crest_outline(aid,w,h)
  face([(x,-.125,z) for x,z in outline],[uvground(.5+x/w*3.4,3.3+z/h*.4) for x,z in outline])
 elif 'Awning' in aid:
  face([(0,0,3.5),(w,0,3.5),(w,-1.25,2.9),(0,-1.25,2.9)],[(.63,.55),(.98,.55),(.98,.8),(.63,.8)])
  face([(0,-1.25,2.9),(w,-1.25,2.9),(w,-1.25,2.7),(0,-1.25,2.7)],[(.63,.8),(.98,.8),(.98,.85),(.63,.85)])
 elif a['kind']=='wall':panel(0,w,0,h,0,0,.5,3.9,1,3.3)
 elif a['kind']=='roof':face([(0,0,0),(w,0,0),(w,h,0),(0,h,0)],[(0,0),(1,0),(1,1),(0,1)])
 # Dedicated regions prevent unrelated architectural roles from overwriting each other.
 if 'Pilaster' in aid or 'Corner' in aid:
  uvs=[((16+(x+y)*200)/2048,(960+z/h*900)/2048) for x,y,z in verts]
 elif 'Parapet' in aid:
  uvs=[((248+x/w*800)/2048,(960+z/h*300)/2048) for x,y,z in verts]
 elif 'Crest' in aid:
  sx,sy=(248,1300) if aid.endswith('A') else (1080,960)
  uvs=[((sx+x/w*800)/2048,(sy+z/h*400)/2048) for x,y,z in verts]
 elif 'Awning' in aid:
  uvs=[((248+x/w*1200)/2048,(1720+(320 if y>-.01 else 40+(z-2.9)*200))/2048) for x,y,z in verts]
 if 'Damaged' in aid:
  # Raised plank overlays remain separate visible surfaces within the same mesh/material.
  x0,x1=(.7,w-2.1) if '_G_' in aid else (.5,w-.5)
  for z,tilt in [(1.3,.25),(2.05,-.2)]:
   face([(x0,-.05,z),(x1,-.05,z+tilt),(x1,-.05,z+tilt+.25),(x0,-.05,z+.25)],
        [(1264/2048,748/2048),(1864/2048,748/2048),(1864/2048,800/2048),(1264/2048,800/2048)])
 assigned_material=m
 if a['kind'] in ['wall','roof']:
  assigned_material=bpy.data.materials.new('M_'+aid);assigned_material.use_nodes=True
  tile=({'BRK':'Brick_Red','SPN':'Stucco_White','DECO':'Brick_Buff','MSN':'Stucco_Cream'}[FAM]) if a['kind']=='wall' else 'Roof_TarGravel'
  node=assigned_material.node_tree.nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(str(R/f'textures/T_TILE_{tile}.png'))
  shader=next(n for n in assigned_material.node_tree.nodes if n.type=='BSDF_PRINCIPLED');assigned_material.node_tree.links.new(node.outputs['Color'],shader.inputs['Base Color'])
  if a['kind']=='wall':uvs=[(0,0),(1,0),(1,h/4),(0,h/4)]
 mesh=bpy.data.meshes.new(aid);mesh.from_pydata(verts,[],faces);mesh.update();layer=mesh.uv_layers.new(name='UVMap')
 for loop in mesh.loops:layer.data[loop.index].uv=uvs[loop.vertex_index]
 obj=bpy.data.objects.new(aid,mesh);bpy.context.collection.objects.link(obj);mesh.materials.append(assigned_material)
 bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
 if len(mesh.polygons)>600:raise RuntimeError(aid+' over budget')
 obj['nominal']=a['nominal'];obj['kind']=a['kind'];return obj
for a in plan:build(a)
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{FAM}_kit_authored.blend'))
print('Authored',len(plan),FAM,'modules; texture and damage passes still required before export.')
