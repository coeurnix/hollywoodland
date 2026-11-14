"""Author and export the four hotel landmarks. Blender 5.2 background script.
All parts have authored geometry, UVs and flat prep colour. No service calls.
Congruent repeating bays stack; distinct roles have disjoint padded UV patches.
"""
import bpy,bmesh,math,json,sys
from mathutils import Vector
from pathlib import Path
R=Path(__file__).resolve().parents[2];SRC=R/'src/hub_hotels';WORK=R/'work/hub_hotels'
WORK.mkdir(exist_ok=True)
PATCH={
 'upper':((.01,.01,.24,.24),(7,7)), 'door':((.26,.01,.49,.24),(7,7)),
 'arc':((.51,.01,.74,.24),(7,7)), 'entry':((.76,.01,.99,.24),(7,7)),
 'wall':((.01,.26,.24,.49),(6,6)), 'roof':((.26,.26,.49,.49),(6,6)),
 'trim':((.51,.26,.74,.49),(4,4)), 'timber':((.76,.26,.99,.49),(7,7)),
 'canvas':((.01,.51,.24,.74),(6.5,6.5)), 'metal':((.26,.51,.365,.74),(4,8.761904762)), 'tile':((.385,.51,.49,.74),(6,13.142857143)),
 'gable':((.51,.51,.99,.74),(18,8.5)), 'sign':((.01,.76,.49,.99),(32,15)),
 'ground':((.51,.76,.74,.99),(6,6)), 'archupper':((.76,.76,.99,.99),(3.1,3.1))}
COL={'wall':(.70,.60,.43,1),'trim':(.84,.73,.54,1),'roof':(.22,.19,.18,1),'tile':(.52,.19,.09,1),'timber':(.18,.09,.045,1),'glass':(.045,.075,.07,1),'door':(.22,.095,.035,1),'amber':(.76,.40,.095,1),'canvas':(.11,.24,.095,1),'metal':(.10,.17,.15,1),'ground':(.40,.23,.11,1)}
V=[];F=[];U=[];C=[];TAGS=[]
def face(points,role='wall',uv=None,color=None):
 n=len(V);V.extend(points);F.append(tuple(n+i for i in range(len(points))));TAGS.append(role)
 rect,span=PATCH[role]
 if uv is None:
  edges=[Vector(points[(i+1)%len(points)])-Vector(points[i]) for i in range(len(points))];a=max(edges,key=lambda e:e.length).normalized();normal=(Vector(points[1])-Vector(points[0])).cross(Vector(points[2])-Vector(points[0]));normal.normalize();b=normal.cross(a)
  uv=[((Vector(p)-Vector(points[0])).dot(a),(Vector(p)-Vector(points[0])).dot(b)) for p in points]
  mu=min(p[0] for p in uv);mv=min(p[1] for p in uv);uv=[(u-mu,v-mv) for u,v in uv]
 U.append([(rect[0]+u/span[0]*(rect[2]-rect[0]),rect[1]+v/span[1]*(rect[3]-rect[1])) for u,v in uv]);C.append(COL[color or (role if role in COL else 'wall')])
def box(c,d,role='trim',color=None,omit=()):
 x,y,z=c;a,b,h=[v/2 for v in d]
 p=[(x+sx*a,y+sy*b,z+sz*h) for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
 inds=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
 for i,inds0 in enumerate(inds):
  if i not in omit:face([p[j] for j in inds0],role,color=color)
def beam(a,b,width,role='metal'):
 a,b=Vector(a),Vector(b);v=b-a;rot=v.to_track_quat('Z','Y');right=rot@Vector((width/2,0,0));forward=rot@Vector((0,width/2,0))
 count=max(1,math.ceil(v.length/3.5))
 for j in range(count):
  p=[p+sx*right+sy*forward for p in [a+v*j/count,a+v*(j+1)/count] for sx,sy in [(-1,-1),(1,-1),(1,1),(-1,1)]]
  inds0=[(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
  if j==0:inds0.append((0,3,2,1))
  if j==count-1:inds0.append((4,5,6,7))
  for inds in inds0:face([p[i] for i in inds],role)
def wall_point(side,t,z,depth=0):
 # t walks along each facade; positive depth goes into the building.
 if side=='front':return(t,-D/2+depth,z)
 if side=='back':return(-t,D/2-depth,z)
 if side=='right':return(W/2-depth,t,z)
 return(-W/2+depth,-t,z)
def panel(side,t0,t1,z0,z1,role='wall',depth=0,color='wall'):
 face([wall_point(side,t,z,depth) for t,z in [(t0,z0),(t1,z0),(t1,z1),(t0,z1)]],role,[(0,0),(t1-t0,0),(t1-t0,z1-z0),(0,z1-z0)],color)
def window_bay(side,t0,t1,z0,z1,door=False,depth=0,lit=False):
 role='door' if door else 'upper';bw=t1-t0;bh=z1-z0
 hw=min(1.0,bw*.25) if not door else min(1.2,bw*.29)
 lower=.43 if not door else .08;upper=min(bh-.35,lower+(1.85 if not door else 2.65))
 cx=(t0+t1)/2;a=cx-hw-.14;b=cx+hw+.14;za=max(z0,z0+lower-.10);zb=z0+upper+.10
 def q(coords,r=role,col='wall',dep=depth):face([wall_point(side,t,z,dep) for t,z in coords],r,[(t-t0,z-z0) for t,z in coords],col)
 q([(t0,z0),(a,z0),(a,z1),(t0,z1)]);q([(b,z0),(t1,z0),(t1,z1),(b,z1)])
 q([(a,z0),(b,z0),(b,za),(a,za)]);q([(a,zb),(b,zb),(b,z1),(a,z1)])
 inner=[(cx-hw,z0+lower),(cx+hw,z0+lower),(cx+hw,z0+upper),(cx-hw,z0+upper)]
 outer=[(a,za),(b,za),(b,zb),(a,zb)]
 for i in range(4):
  j=(i+1)%4;pts=[wall_point(side,*outer[i],depth),wall_point(side,*outer[j],depth),wall_point(side,*inner[j],depth+.15),wall_point(side,*inner[i],depth+.15)]
  face(pts,'trim',color='trim')
 q(inner,role,'amber' if lit else ('door' if door else 'glass'),depth+.15)
def arcbay(side,t0,t1,z0,z1,depth=1.0,role='arc',lit=False):
 bw=t1-t0;cx=(t0+t1)/2;r=min(bw*.365,(z1-z0)*.43);spring=z0+(z1-z0)-r-.30
 n=4 if role=='archupper' else 8;arc=[(cx-r*math.cos(math.pi*i/n),spring+r*math.sin(math.pi*i/n)) for i in range(n+1)]
 def q(coords,col='wall',dep=0):face([wall_point(side,t,z,dep) for t,z in coords],role,[(t-t0,z-z0) for t,z in coords],col)
 q([(t0,z0),(cx-r,z0),(cx-r,z1),(t0,z1)]);q([(cx+r,z0),(t1,z0),(t1,z1),(cx+r,z1)])
 for i in range(n):
  a,b=arc[i],arc[i+1];q([a,b,(b[0],z1),(a[0],z1)])
  face([wall_point(side,*a,0),wall_point(side,*b,0),wall_point(side,*b,depth),wall_point(side,*a,depth)],'trim',color='trim')
 for t in [cx-r,cx+r]:face([wall_point(side,t,z0),wall_point(side,t,spring),wall_point(side,t,spring,depth),wall_point(side,t,z0,depth)],'trim',color='trim')
 opening=[(cx-r,z0),(cx+r,z0)]+list(reversed(arc))
 # The bottom two vertices duplicate the arc ends only when spring==z0 (never here).
 q(opening,'amber' if lit else ('glass' if role=='archupper' else 'door'),depth)
 face([wall_point(side,cx-r,z0),wall_point(side,cx+r,z0),wall_point(side,cx+r,z0,depth),wall_point(side,cx-r,z0,depth)],'ground',color='ground')
def strip(side,length,z0,z1,depth=0,role='trim',color='trim'):
 n=math.ceil(length/3.5)
 for i in range(n):panel(side,-length/2+length*i/n,-length/2+length*(i+1)/n,z0,z1,role,depth,color)
def cylinder(side,t,z0,z1,r=.23,n=6):
 p0=Vector(wall_point(side,t,z0,.40));p1=Vector(wall_point(side,t,z1,.40))
 for i in range(n):
  a=2*math.pi*i/n;b=2*math.pi*(i+1)/n
  face([(p0.x+r*math.cos(a),p0.y+r*math.sin(a),z0),(p0.x+r*math.cos(b),p0.y+r*math.sin(b),z0),(p1.x+r*math.cos(b),p1.y+r*math.sin(b),z1),(p1.x+r*math.cos(a),p1.y+r*math.sin(a),z1)],'trim',[(i*2*math.pi*r/n,0),((i+1)*2*math.pi*r/n,0),((i+1)*2*math.pi*r/n,z1-z0),(i*2*math.pi*r/n,z1-z0)],'trim')
def canopy(side,t0,t1,z,projection=.95):
 # Four broad alternating canvas stripes are visible geometry with congruent UVs.
 for i in range(6):
  a=t0+(t1-t0)*i/6;b=t0+(t1-t0)*(i+1)/6
  p=[wall_point(side,a,z,1.25),wall_point(side,b,z,1.25),wall_point(side,b,z-.35,1.25-projection),wall_point(side,a,z-.35,1.25-projection)]
  face(p,'canvas',[(i*(t1-t0)/6,0),((i+1)*(t1-t0)/6,0),((i+1)*(t1-t0)/6,.99),(i*(t1-t0)/6,.99)],'canvas' if i%2==0 else 'trim')
  p=[wall_point(side,a,z-.35,1.25-projection),wall_point(side,b,z-.35,1.25-projection),wall_point(side,b,z-.53,1.25-projection),wall_point(side,a,z-.53,1.25-projection)]
  face(p,'canvas',[(i*(t1-t0)/6,1.05),((i+1)*(t1-t0)/6,1.05),((i+1)*(t1-t0)/6,1.23),(i*(t1-t0)/6,1.23)],'canvas' if i%2==0 else 'trim')
def railing(side,t0,t1,z):
 # Real bars; no alpha/translucency or full opaque sheet masking the veranda.
 for zz in [z+.16,z+.93]:
  a=Vector(wall_point(side,t0,zz,.24));b=Vector(wall_point(side,t1,zz,.24));beam(a,b,.065,'timber')
 for j in range(1,3):
  t=t0+(t1-t0)*j/3;beam(wall_point(side,t,z+.18,.24),wall_point(side,t,z+.92,.24),.05,'timber')
def roof_flat(z):
 nx=math.ceil(W/5.8);ny=math.ceil(D/5.8)
 for i in range(nx):
  for j in range(ny):
   a=-W/2+W*i/nx;b=-W/2+W*(i+1)/nx;c=-D/2+D*j/ny;d=-D/2+D*(j+1)/ny
   face([(a,c,z),(b,c,z),(b,d,z),(a,d,z)],'roof',[(0,0),(b-a,0),(b-a,d-c),(0,d-c)],'roof')
def hip_roof(eave,ridge):
 # A long hip with ridge inset at each end. Split broad planes into tiles.
 inset=min(5,W*.2,D*.2)
 outline=[(-W/2,-D/2,eave),(W/2,-D/2,eave),(W/2,D/2,eave),(-W/2,D/2,eave)]
 ridgepts=[(-W/2+inset,0,ridge),(W/2-inset,0,ridge)] if W>=D else [(0,-D/2+inset,ridge),(0,D/2-inset,ridge)]
 if W>=D:polys=[[outline[0],outline[1],ridgepts[1],ridgepts[0]],[outline[1],outline[2],ridgepts[1]],[outline[2],outline[3],ridgepts[0],ridgepts[1]],[outline[3],outline[0],ridgepts[0]]]
 else:polys=[[outline[0],outline[1],ridgepts[0]],[outline[1],outline[2],ridgepts[1],ridgepts[0]],[outline[2],outline[3],ridgepts[1]],[outline[3],outline[0],ridgepts[0],ridgepts[1]]]
 # Subdivide planes before UV assignment to retain at least25px/m on large roofs.
 for poly in polys:
  if len(poly)==3:poly=[poly[0],poly[1],poly[2],poly[2]]
  a,b,c,d=map(Vector,poly);nu=max(1,math.ceil(max((b-a).length,(c-d).length)/5.5));nv=max(1,math.ceil(max((d-a).length,(c-b).length)/5.5))
  for i in range(nu):
   for j in range(nv):
    def p(u,v):return a*(1-u)*(1-v)+b*u*(1-v)+c*u*v+d*(1-u)*v
    q=[p(i/nu,j/nv),p((i+1)/nu,j/nv),p((i+1)/nu,(j+1)/nv),p(i/nu,(j+1)/nv)]
    if (q[3]-q[2]).length<1e-6:q=q[:3]
    if len(q)==3 and (q[2]-q[1]).length<1e-6:continue
    face(q,'roof',color='roof')
 # Bold ridge without copying thousands of tile cylinders.
 beam(ridgepts[0],ridgepts[1],.16,'tile')
def gable(side,centre,width,base,height):
 # Symmetric stepped/scalloped silhouette with eleven samples.
 profile=[(-.5,0),(-.42,.04),(-.34,.10),(-.28,.31),(-.18,.36),(-.13,.66),(-.07,.91),(0,1),(.07,.91),(.13,.66),(.18,.36),(.28,.31),(.34,.10),(.42,.04),(.5,0)]
 bottom=base-.12;pts=[(centre-width/2,bottom),(centre+width/2,bottom)]+[(centre+u*width,base+v*height) for u,v in reversed(profile)]
 face([wall_point(side,t,z,.035) for t,z in pts],'gable',[(t-centre+width/2,z-bottom) for t,z in pts],'wall')
 # Finished top/back edges, shallow solid parapet returns into the roof.
 face([wall_point(side,t,z,.42) for t,z in reversed(pts)],'gable',[(t-centre+width/2,z-bottom) for t,z in reversed(pts)],'wall')
 for i in range(len(profile)-1):
  a,b=profile[i],profile[i+1];aa=(centre+a[0]*width,base+a[1]*height);bb=(centre+b[0]*width,base+b[1]*height)
  face([wall_point(side,*aa,.035),wall_point(side,*bb,.035),wall_point(side,*bb,.42),wall_point(side,*aa,.42)],'tile',color='tile')
def roosevelt():
 global W,D;W,D=74,58;ground=4.5;floor=3.0;eave=22.5
 for side,length,n in [('front',W,12),('back',W,12),('right',D,10),('left',D,10)]:
  pitch=length/n
  for i in range(n):
   a=-length/2+i*pitch;b=a+pitch
   arcbay(side,a,b,0,ground,1.15,'entry' if side=='front' and i in [5,6] else 'arc',side=='front' and i in [5,6])
   for f in range(6):
    lo=ground+f*floor
    if f==5 and side in ['front','back']:
     # Triple arches are genuine geometry, each under2m wide.
     for k in range(3):arcbay(side,a+pitch*k/3,a+pitch*(k+1)/3,lo,lo+floor,.15,'archupper')
    else:window_bay(side,a,b,lo,lo+floor)
  strip(side,length,4.42,4.62,0,'trim');strip(side,length,22.5,23.05,0,'wall')
  # A full-width terracotta pent roof stays inside collision planes.
  n=math.ceil(length/5.5)
  for i in range(n):
   a=-length/2+length*i/n;b=-length/2+length*(i+1)/n
   face([wall_point(side,a,23.0,0),wall_point(side,b,23.0,0),wall_point(side,b,24.0,1.75),wall_point(side,a,24.0,1.75)],'tile',color='tile')
 roof_flat(23.08)
 # Entry portal raised trim and a narrow blank roof-sign backing.
 for x in [-6.1,6.1]:box((x,-28.90,2.8),(.55,.20,5.6),'trim',omit=(0,4))
 for x in [-9,-3,3,9]:
  beam((x,-25,23.1),(x,-25,27.1),.14);beam((x,-25,26.9),(x,-22.6,23.1),.13)
 box((0,-25,26.0),(25,.16,2.0),'sign',color='metal')
 return {'nominal':[74,58,24],'budget':20000,'fits_box':'Roosevelt Hotel','sign_attachments':[{'id':'SIGN_Roosevelt','text':'HOTEL ROOSEVELT','centre':[0,-25.09,26.0],'size_m':[24.6,1.8],'front':'-Y','separate_card':True}],'notes':'Seven readable floors:4.5m ground arcade and six3m upper floors. Original asymmetrical T footprint is adapted to SPEC rectangular collision envelope. Ground arches are1.15m shallow recesses; all four sides and complete roof finished.'}
def hollywood(w=116,d=26,h=14,wing=False):
 global W,D;W,D=w,d;eave=10.2 if not wing else 9.5;top=h-.10
 for side,length,n in [('front',W,19 if not wing else 4),('back',W,19 if not wing else 4),('right',D,5 if not wing else 9),('left',D,5 if not wing else 9)]:
  pitch=length/n;veranda=side in (['front','back'] if not wing else ['right','left'])
  for i in range(n):
   a=-length/2+i*pitch;b=a+pitch
   for f,(lo,hi) in enumerate([(0,3.6),(3.6,6.9),(6.9,eave)]):
    if f==0 and i==n//2 and (not veranda or (not wing and side=='front')):arcbay(side,a,b,lo,hi,1.25 if veranda else .35,'entry',side=='front')
    else:window_bay(side,a,b,lo,hi,door=f<2,depth=1.25 if veranda and f<2 else 0)
   if veranda:
    if not (not wing and side=='front' and i==n//2):canopy(side,a+.12,b-.12,7.0)
    railing(side,a+.15,b-.15,3.6)
    # Exposed veranda floor and upper soffit provide a real shallow porch.
    for z in [0,3.6,6.9]:face([wall_point(side,a,z,0),wall_point(side,b,z,0),wall_point(side,b,z,1.25),wall_point(side,a,z,1.25)],'ground' if z==0 else 'timber',color='ground' if z==0 else 'timber')
   if veranda and i<n:
    t=a+.40;cylinder(side,t,.25,3.45)
    p=wall_point(side,t,.13,.40);box(p,(.68,.68,.26),'trim',omit=(0,))
    p=wall_point(side,t,3.48,.40);box(p,(.70,.70,.20),'trim',omit=(0,1))
    beam(wall_point(side,t,3.62,.40),wall_point(side,t,6.85,.40),.13,'timber')
  # Horizontal eave band and broad base are split into small repeat lengths.
  strip(side,length,eave-.16,eave,0,'timber','timber')
 hip_roof(eave,top)
 if wing:
  for side in ['front','back']:gable(side,0,10,9.2,h-9.2)
 else:
  for side in ['front','back']:
   gable(side,0,16,10.1,3.9)
   for x in [-36,36]:gable(side,x,10,10.1,2.1)
  box((0,-12.97,10.3),(11,.10,1.3),'sign',color='trim')
 return {'nominal':[w,d,h],'budget':10000,'fits_box':'Hollywood Hotel Wing' if wing else 'Hollywood Hotel','also_fits':[{'size':[24,52,12],'mirror':True}] if wing else [],'sign_attachments':[] if wing else [{'id':'SIGN_HollywoodHotel','text':'HOLLYWOOD HOTEL','centre':[0,-13.035,10.3],'size_m':[10.5,1.1],'front':'-Y','separate_card':True}],'notes':'Three readable floors. Shaded shallow verandas with round columns, timber rails/French doors, green/cream striped canvas; low slate-brown hipped roof with Mission gables. Front/back and both sides finished, including courtyard-facing faces. Veranda setback1.25m stays within box.'}
def courtwall():
 global W,D;W,D=26,6
 for side,length,n in [('front',26,4),('back',26,4),('right',6,1),('left',6,1)]:
  for i in range(n):
   a=-length/2+length*i/n;b=-length/2+length*(i+1)/n
   if side in ['front','back']:arcbay(side,a,b,0,4.65,.32,'door',i==1)
   else:window_bay(side,a,b,0,4.65)
   if side=='front' and i in [1,2]:canopy(side,a+.55,b-.55,3.8,.95)
  strip(side,length,4.65,4.85,0,'trim')
 hip_roof(4.85,5.7)
 for side in ['front','back']:gable(side,0,10,4.8,1.2)
 return {'nominal':[26,6,6],'budget':10000,'fits_box':'Hollywood Hotel Court Wall','also_fits':[{'size':[26,6,6],'mirror':True}],'sign_attachments':[],'notes':'One solid26x6x6m courtyard closure range, used twice. Centres atx=-21,+21 produce an exact16m central gap; no gateway opening in the individual mesh. Four arched shallow French-door recesses, finished roof and reverse facade.'}
BUILD={'LM_Roosevelt':roosevelt,'LM_HollywoodHotel':hollywood,'LM_HollywoodHotel_Wing':lambda:hollywood(24,52,12,True),'LM_HollywoodHotel_CourtWall':courtwall}
def output(aid,meta):
 me=bpy.data.meshes.new(aid);me.from_pydata(V,[],F);me.update();ob=bpy.data.objects.new(aid,me);bpy.context.collection.objects.link(ob)
 uv=me.uv_layers.new(name='UVMap');co=me.color_attributes.new(name='PrepColor',type='FLOAT_COLOR',domain='CORNER')
 for p,coords,color in zip(me.polygons,U,C):
  for li,xy in zip(p.loop_indices,coords):uv.data[li].uv=xy;co.data[li].color=color
 mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Roughness'].default_value=.9;node=mat.node_tree.nodes.new('ShaderNodeVertexColor');node.layer_name='PrepColor';mat.node_tree.links.new(node.outputs['Color'],bs.inputs['Base Color']);me.materials.append(mat)
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0000001);bm.to_mesh(me);bm.free()
 bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
 bm=bmesh.new();bm.from_mesh(me)
 audit={'tris':len(me.polygons),'bounds':[[round(fn(v.co[i] for v in me.vertices),6) for i in range(3)] for fn in [min,max]],'material_slots':len(me.materials),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'uv_min':[min(x.uv[i] for x in me.uv_layers.active.data) for i in range(2)],'uv_max':[max(x.uv[i] for x in me.uv_layers.active.data) for i in range(2)],'scale':list(ob.scale),'rotation':list(ob.rotation_euler),'origin':'footprint-centre-ground'}
 if audit['branch_edges']:print('BRANCHES',[[list(v.co) for v in e.verts] for e in bm.edges if len(e.link_faces)>2][:20])
 bm.free();assert audit['tris']<=meta['budget'],audit;assert audit['zero_area_faces']==audit['loose_vertices']==0,audit
 assert all(v>=0 for v in audit['uv_min']) and all(v<=1.00001 for v in audit['uv_max']),audit
 bpy.ops.export_scene.gltf(filepath=str(R/f'work/{aid}_input.glb'),export_format='GLB',use_selection=True)
 # Reimport actual GLB to audit delivered axes and triangle count.
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(R/f'work/{aid}_input.glb'));q=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');vv=[q.matrix_world@v.co for v in q.data.vertices];audit['export_bounds']=[[round(fn(v[i] for v in vv),6) for i in range(3)] for fn in [min,max]];audit['export_tris']=sum(len(p.vertices)-2 for p in q.data.polygons)
 for o in set(bpy.data.objects)-old:bpy.data.objects.remove(o,do_unlink=True)
 bpy.ops.wm.save_as_mainfile(filepath=str(SRC/f'{aid}.blend'))
 rec={'id':aid,'kind':'landmark','phase':3,'model':f'work/{aid}_input.glb','source':f'src/hub_hotels/{aid}.blend','concept':[f'concept/{aid}_34.png',f'concept/{aid}_front.png'],'reference':f'concept/{aid}_34.png','prompts':json.loads((SRC/'concept_prompts.json').read_text())[aid],'material':'Shaded','resolution':'Basic','metadata':meta,'audit':audit,'uv_policy':'One padded2K atlas; disjoint semantic patches; only congruent repeated bays/features stack. Wall regions >=67px/m, broad roof repeating patches >=78px/m at2K. Gable54.6px/m. Sign backing30px/m; separate lettering card. Bake repeated regions from one chosen congruent representative per patch if a full-mesh returned UV bake differs by face. No wall-role blanket stacking.'}
 (SRC/f'{aid}.json').write_text(json.dumps(rec,indent=2)+'\n')
 queue=[json.loads(p.read_text()) for p in SRC.glob('LM_*.json')]
 (SRC/'texture_request_queue.json').write_text(json.dumps({'version':1,'assets':queue},indent=2)+'\n')
 print('HOTEL_READY',aid,audit,flush=True)
targets=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else list(BUILD)
for aid in targets:
 bpy.ops.wm.read_factory_settings(use_empty=True);V.clear();F.clear();U.clear();C.clear();TAGS.clear();output(aid,BUILD[aid]())
(SRC/'uv_layout.json').write_text(json.dumps(PATCH,indent=2)+'\n')
