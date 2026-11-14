"""Hand-authored Egyptian hub exteriors. Concepts must exist before construction.
Run Blender 5.2 --background --python art/buildings/src/hub_egyptian/build.py.
No Rodin requests; root serializes texturing. All UVs are authored and reusable.
"""
import bpy,bmesh,json,math,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];S=R/'src/hub_egyptian';W=R/'work/hub_egyptian'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.save_version=0
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
# Dedicated padded roles in a 2048 master; repeat matching wall/roof panels.
slots={'wall':(16,16,1000,740,50),'column':(1040,16,350,600,50),'door':(1410,16,620,500,50),'frieze':(16,780,1000,160,50),'mural':(1410,550,620,740,50),'roof':(16,980,520,520,25),'side':(560,980,800,740,25),'tile':(16,1530,520,480,50),'trim':(1390,1330,640,160,50),'plant':(1390,1530,640,500,50)}
class Mesh:
 def __init__(self):self.v=[];self.f=[];self.uv=[];self.roles=[];self.densities=[]
 def face(self,vs,role='wall',uv=None):
  if len(vs)<3:return
  vv=[Vector(p) for p in vs];s=len(self.v);self.v.extend(vs);self.f.append(tuple(range(s,s+len(vs))));self.roles.append(role)
  if uv is None:
   edge=vv[1]-vv[0];u=edge.normalized();normal=(vv[1]-vv[0]).cross(vv[-1]-vv[0]).normalized();v=normal.cross(u)
   pu=[(p-vv[0]).dot(u) for p in vv];pv=[(p-vv[0]).dot(v) for p in vv]
   x,y,ww,hh,px=slots[role];width=max(pu)-min(pu);height=max(pv)-min(pv)
   density=min(px,ww/max(width,.00001),hh/max(height,.00001));self.densities.append((role,density))
   uv=[((x+(a-min(pu))*density)/2048,(y+(b-min(pv))*density)/2048) for a,b in zip(pu,pv)]
  self.uv.extend(uv)
 def front(self,x0,x1,z0,z1,y,role='wall'):self.face([(x0,y,z0),(x1,y,z0),(x1,y,z1),(x0,y,z1)],role)
 def side(self,x,y0,y1,z0,z1,role='side',flip=False):
  v=[(x,y0,z0),(x,y1,z0),(x,y1,z1),(x,y0,z1)]
  self.face(list(reversed(v)) if flip else v,role)
 def top(self,x0,x1,y0,y1,z,role='roof'):self.face([(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)],role)
 def box(self,x0,x1,y0,y1,z0,z1,role='trim',bottom=False,back=True):
  n=max(1,math.ceil((x1-x0)/12))
  for i in range(n):
   a=x0+(x1-x0)*i/n;b=x0+(x1-x0)*(i+1)/n
   self.front(a,b,z0,z1,y0,role)
   if back:self.face([(b,y1,z0),(a,y1,z0),(a,y1,z1),(b,y1,z1)],role)
   self.top(a,b,y0,y1,z1,role)
   if bottom:self.face([(a,y1,z0),(b,y1,z0),(b,y0,z0),(a,y0,z0)],role)
  self.side(x0,y0,y1,z0,z1,role,True);self.side(x1,y0,y1,z0,z1,role)
 def bands(self,w,d,levels,front=True):
  # Ring skin with an integrated cavetto profile, no buried box faces.
  for (z0,ins0),(z1,ins1) in zip(levels,levels[1:]):
   a=[(-w/2+ins0,-d/2+ins0,z0),(w/2-ins0,-d/2+ins0,z0),(w/2-ins0,d/2-ins0,z0),(-w/2+ins0,d/2-ins0,z0)]
   b=[(-w/2+ins1,-d/2+ins1,z1),(w/2-ins1,-d/2+ins1,z1),(w/2-ins1,d/2-ins1,z1),(-w/2+ins1,d/2-ins1,z1)]
   for i in range(4):
    if i==0 and not front:continue
    # Split long cornice runs to preserve 50 px/m repeated motifs.
    length=(Vector(a[(i+1)%4])-Vector(a[i])).length;n=max(1,math.ceil(length/16))
    for j in range(n):
     p=Vector(a[i]).lerp(Vector(a[(i+1)%4]),j/n);q=Vector(a[i]).lerp(Vector(a[(i+1)%4]),(j+1)/n);r=Vector(b[i]).lerp(Vector(b[(i+1)%4]),(j+1)/n);s=Vector(b[i]).lerp(Vector(b[(i+1)%4]),j/n)
     self.face([p,q,r,s],'frieze')
 def column(self,cx,cy,h,rad=1):
  levels=[(0,.86),(.035,.86),(.06,.70),(.80,.60),(.835,.67),(.85,.72),(.93,.96),(.975,1),(1,1)]
  rings=[[(cx+rad*r*math.cos(2*math.pi*j/12),cy+rad*r*math.sin(2*math.pi*j/12),h*z) for j in range(12)] for z,r in levels]
  x,y,ww,hh,px=slots['column'];circ=2*math.pi*rad
  for k,(a,b) in enumerate(zip(rings,rings[1:])):
   for j in range(12):
    uu0=x+circ*50*j/12;uu1=x+circ*50*(j+1)/12;v0=y+levels[k][0]*h*50;v1=y+levels[k+1][0]*h*50
    self.face([a[j],a[(j+1)%12],b[(j+1)%12],b[j]],'column',[(uu0/2048,v0/2048),(uu1/2048,v0/2048),(uu1/2048,v1/2048),(uu0/2048,v1/2048)])
  # top touches lintel and bottom touches pavement: invisible caps omitted.
 def palm(self,cx,cy):
  # Opaque, faceted leaves: no transparency or cross-card intersections.
  rings=[]
  for z,r in [(0,.48),(.1,.55),(.75,.50),(.88,.37),(.95,.43)]:rings.append([(cx+r*math.cos(j*math.pi/4),cy+r*math.sin(j*math.pi/4),z) for j in range(8)])
  for a,b in zip(rings,rings[1:]):
   for j in range(8):self.face([a[j],a[(j+1)%8],b[(j+1)%8],b[j]],'plant')
  self.face(rings[-1],'plant')
  self.box(cx-.11,cx+.11,cy-.11,cy+.11,.95,2.0,'plant')
  for j in range(8):
   t=j*math.pi/4;dx,dy=math.cos(t),math.sin(t);px,py=-dy*.25,dx*.25
   root=(cx,cy,2);tip=(cx+dx*.7,cy+dy*.7,1.55);mid=(cx+dx*.38,cy+dy*.38,2.30)
   self.face([root,(mid[0]+px,mid[1]+py,mid[2]),tip],'plant');self.face([root,tip,(mid[0]-px,mid[1]-py,mid[2])],'plant')

def roofgrid(g,w,d,z,ymin=None):
 yy=-d/2 if ymin is None else ymin
 nx=math.ceil(w/18);ny=math.ceil((d/2-yy)/18)
 for i in range(nx):
  for j in range(ny):g.top(-w/2+w*i/nx,-w/2+w*(i+1)/nx,yy+(d/2-yy)*j/ny,yy+(d/2-yy)*(j+1)/ny,z)
def walls(g,w,d,h,front=True):
 for side in [-1,1]:
  n=math.ceil(d/16)
  for i in range(n):g.side(side*w/2,-d/2+d*i/n,-d/2+d*(i+1)/n,0,h,'side',side<0)
 n=math.ceil(w/16)
 for i in range(n):g.face([(w/2-w*i/n,d/2,0),(w/2-w*(i+1)/n,d/2,0),(w/2-w*(i+1)/n,d/2,h),(w/2-w*i/n,d/2,h)],'side')
 if front:
  for i in range(n):g.front(-w/2+w*i/n,-w/2+w*(i+1)/n,0,h,-d/2)

def main():
 g=Mesh();w,d=82,50;walls(g,w,d,13.3,False)
 # Monumental front wall divided into reusable broad painted panels.
 for x0,x1 in [(-41,-37),(-37,-28),(-28,-18),(18,28),(28,37),(37,41)]:
  for z0,z1 in [(0,.6),(.6,10.2),(10.2,13.3)]:g.front(x0,x1,z0,z1,-25,'mural' if x0 in [-28,18] and z0==.6 else 'wall')
 # Central recessed wall, dark portal remains a surface for dungeon interaction.
 for x0,x1 in [(-18,-5),(-5,5),(5,18)]:
  for z0,z1 in [(0,7),(7,9.8)]:g.front(x0,x1,z0,z1,-23,'door' if x0==-5 and z0==0 else 'wall')
 for x,flip in [(-18,True),(18,False)]:g.side(x,-25,-23,0,9.8,'wall',flip)
 # Portico slab and sign fascia; bottom is visible over the recessed entrance.
 g.box(-18,18,-25,-23,9.8,11.7,'trim',True,False)
 for x0,x1 in [(-18,0),(0,18)]:g.front(x0,x1,11.7,13.3,-25,'frieze')
 # Four papyrus columns stand entirely within the two metre recess.
 for x in [-14,-8,8,14]:g.column(x,-24,9.8,1)
 g.bands(w,d,[(13.3,0),(13.6,.2),(14.1,0),(14.4,0),(14.5,.15)])
 # Roof cells explicitly omit the portion beneath the raised red-tile canopy.
 for xa,xb in zip([-41,-27,-18,0,18,27],[-27,-18,0,18,27,41]):
  for ya,yb in zip([-25,-20,-5,10],[-20,-5,10,25]):
   if xa>=-18 and xb<=18 and ya==-25:continue
   g.top(xa,xb,ya,yb,14.5)
 # Central tile pent canopy; explicit roof slope and fascia top, tiled by paint.
 for x0,x1 in [(-18,-9),(-9,0),(0,9),(9,18)]:g.face([(x0,-25,14.5),(x1,-25,14.5),(x1,-20,15),(x0,-20,15)],'tile')
 for x,flip in [(-18,True),(18,False)]:
  vs=[(x,-25,14.5),(x,-20,15),(x,-20,14.5)];g.face(list(reversed(vs)) if flip else vs,'trim')
 # Corner crest caps sit on the finished roof and reach exact nominal height.
 for x in [-39.5,39.5]:
  for y in [-23.5,23.5]:g.box(x-1.5,x+1.5,y-1.5,y+1.5,14.5,15,'trim')
 return g
def wing():
 g=Mesh();w,d=26,28;walls(g,w,d,5.5,False)
 for x0,x1 in [(-13,-12),(12,13)]:g.front(x0,x1,0,5.5,-14,'mural')
 for x0,x1 in [(-12,-4),(-4,4),(4,12)]:g.front(x0,x1,0,4.8,-12.5,'door')
 g.side(-12,-14,-12.5,0,4.8,'wall',True);g.side(12,-14,-12.5,0,4.8,'wall')
 g.box(-12,12,-14,-12.5,4.8,5.5,'frieze',True,False)
 for x in [-7.5,-2.5,2.5,7.5]:g.column(x,-13.25,4.8,.72)
 g.bands(w,d,[(5.5,0),(5.8,.2),(6.35,0),(6.6,0),(6.75,.1),(7,0)])
 roofgrid(g,w,d,7)
 # Two pots and small palms stay inside the footprint and below parapet height.
 for x in [-10.5,10.5]:g.palm(x,-13.2)
 return g
def gateway():
 g=Mesh()
 # Inner pylon edges remain x +/-6 all the way to the lintel: exact clear width.
 for side in [-1,1]:
  levels=[(0,15),(0.5,15),(7.6,14.25),(8,14.25),(8.5,14.7),(9,15),(10,15)]
  for (z0,outer0),(z1,outer1) in zip(levels,levels[1:]):
   xl0,xr0=sorted([side*6,side*outer0]);xl1,xr1=sorted([side*6,side*outer1])
   g.face([(xl0,-1,z0),(xr0,-1,z0),(xr1,-1,z1),(xl1,-1,z1)],'mural' if z0==.5 else 'frieze')
   g.face([(xr0,1,z0),(xl0,1,z0),(xl1,1,z1),(xr1,1,z1)],'mural' if z0==.5 else 'frieze')
   g.face([(side*outer0,-1,z0),(side*outer0,1,z0),(side*outer1,1,z1),(side*outer1,-1,z1)],'wall')
   if z1<=8:g.side(side*6,-1,1,z0,z1,'wall',side>0)
  g.top(-15 if side<0 else 6,-6 if side<0 else 15,-1,1,10,'trim')
 # Lintel shares boundary edges with pylons: no hidden end faces.
 g.front(-6,6,8,10,-1,'frieze');g.face([(6,1,8),(-6,1,8),(-6,1,10),(6,1,10)],'frieze');g.top(-6,6,-1,1,10,'trim');g.face([(-6,1,8),(6,1,8),(6,-1,8),(-6,-1,8)],'trim')
 return g

plan=[('LM_Egyptian',[82,50,15],20000,main,{'fits_box':'The Egyptian Theatre','recess':[],'sign_attachments':[{'id':'SIGN_Egyptian','position':[0,-25.015,10.75],'size':[30,1.4],'front':'-Y'},{'id':'SIGN_Egyptian_Marquee','position':[0,-23.015,8.1],'size':[10,1.25],'front':'-Y'}]}),('LM_Egyptian_Wing',[26,28,7],10000,wing,{'fits_box':'The Egyptian Theatre courtyard wing','also_fits':[{'size':[26,28,7],'mirror':True}],'sign_attachments':[]}),('LM_Egyptian_Gateway',[30,2,10],10000,gateway,{'fits_box':None,'non_colliding':True,'clear_opening':{'x':[-6,6],'z':[0,8],'y':[-1,1]},'sign_attachments':[{'id':'SIGN_Egyptian_Gate','position':[0,-1.015,9.0],'size':[11.5,1.3],'front':'-Y'}]})]
entries=[];queue=[]
for aid,nominal,budget,fn,extra in plan:
 assert all((R/'concept'/f'{aid}_{v}.png').exists() for v in ['34','front']),aid+' lacks concept'
 g=fn();me=bpy.data.meshes.new(aid);me.from_pydata(g.v,[],g.f);me.update();uv=me.uv_layers.new(name='UVMap')
 for i,co in enumerate(g.uv):uv.data[i].uv=co
 ob=bpy.data.objects.new(aid,me);bpy.context.collection.objects.link(ob)
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True;mat.diffuse_color=(.66,.48,.27,1);mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.66,.48,.27,1);mat.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.95;me.materials.append(mat)
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 source=S/f'{aid}.blend';bpy.ops.wm.save_as_mainfile(filepath=str(source))
 output=R/'work'/f'{aid}_input.glb';bpy.ops.export_scene.gltf(filepath=str(output),export_format='GLB',use_selection=True)
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(output));ck=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');vv=[ck.matrix_world@v.co for v in ck.data.vertices]
 bounds=[[round(fn(v[k] for v in vv),6) for k in range(3)] for fn in [min,max]]
 bm=bmesh.new();bm.from_mesh(ck.data);audit={'bounds':bounds,'tris':sum(len(p.vertices)-2 for p in ck.data.polygons),'material_slots':len(ck.material_slots),'zero_area_faces':sum(f.calc_area()<1e-9 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'triangulated':all(len(p.vertices)==3 for p in ck.data.polygons),'scale':list(ob.scale),'rotation':list(ob.rotation_euler)};bm.free();bpy.data.objects.remove(ck,do_unlink=True)
 assert audit['tris']<=budget,(aid,audit)
 assert audit['zero_area_faces']==audit['branch_edges']==audit['loose_vertices']==0,(aid,audit)
 for k,n in enumerate(nominal):assert abs(bounds[1][k]-bounds[0][k]-n)<.0001,(aid,nominal,bounds)
 entry={'id':aid,'kind':'landmark','nominal':nominal,'origin':'footprint-centre-ground','material':'M_'+aid,'source':str(source.relative_to(R)).replace('\\','/'),'model':str(output.relative_to(R)).replace('\\','/'),'concept':[f'concept/{aid}_34.png',f'concept/{aid}_front.png'],'prompts':[f'src/hub_egyptian/{aid}_{v}_prompt.txt' for v in ['34','front']],'audit':audit,'bounds':bounds,'tris':audit['tris'],'uv':{'method':'role-separated repeated planar panels and cylinder unwrap','master':[2048,2048],'padding_px':16,'street_px_m':50,'roof_back_side_px_m':25,'minimum_measured_role_density':round(min(v for _,v in g.densities),3),'slots':slots},'notes':['No real photograph copied to texture; original generated concepts only.','Open ground-facing surfaces and column caps touching lintels are deliberately omitted; their boundary edges are intentional.','Paint Egyptian motifs, window reflections, tile channels and roof variation; do not add geometry or text.','SPEC exact game box enforced; conceptual aspect and original historical dimensions are secondary.'],**extra}
 (W/f'{aid}_metadata.json').write_text(json.dumps(entry,indent=2));entries.append(entry)
 queue.append({'id':aid,'model':entry['model'],'reference':entry['concept'][0],'secondary_reference':entry['concept'][1],'material':'Shaded','resolution':'Basic','prompt':'Hand-painted early classic MMO Egyptian Revival theatre, ochre sandstone cream plaster with teal/oxblood/gold painted geometric friezes, dark recessed doors, painted opaque window reflections, terracotta tile and tar roof. Warm overhead paint light and cool crevice AO. No text, backgrounds, people, modern branding, hive resin or photographic textures. Preserve authored geometry and UVs.','source':entry['source'],'bake_to_authored_uvs':True})
 (W/'texture_request_queue.json').write_text(json.dumps(queue,indent=2));(W/'assets.json').write_text(json.dumps(entries,indent=2))
 print('FIRST_READY',aid,entry['tris'],bounds,flush=True)
 bpy.data.objects.remove(ob,do_unlink=True)
print('All Egyptian authored inputs ready',flush=True)
