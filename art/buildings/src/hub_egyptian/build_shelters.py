"""Authored Pacific Electric shelter and auction kiosk from root's concepts.
Blender --background --python art/buildings/src/hub_egyptian/build_shelters.py
"""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];S=R/'src/hub_egyptian';W=R/'work/hub_egyptian'
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.preferences.filepaths.save_version=0
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
class Mesh:
 def __init__(self):self.v=[];self.f=[]
 def face(self,vs):
  s=len(self.v);self.v.extend(vs);self.f.append(tuple(range(s,s+len(vs))))
 def box(self,x0,x1,y0,y1,z0,z1,top=True,bottom=False):
  self.face([(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1)]);self.face([(x1,y1,z0),(x0,y1,z0),(x0,y1,z1),(x1,y1,z1)])
  self.face([(x0,y1,z0),(x0,y0,z0),(x0,y0,z1),(x0,y1,z1)]);self.face([(x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1)])
  if top:self.face([(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)])
  if bottom:self.face([(x0,y1,z0),(x1,y1,z0),(x1,y0,z0),(x0,y0,z0)])
 def lathe(self,cx,cy,levels,n=8,top=False):
  rr=[[(cx+r*math.cos(j*2*math.pi/n),cy+r*math.sin(j*2*math.pi/n),z) for j in range(n)] for z,r in levels]
  for a,b in zip(rr,rr[1:]):
   for j in range(n):self.face([a[j],a[(j+1)%n],b[(j+1)%n],b[j]])
  if top:self.face(rr[-1])
 def diagonal(self,a,b,width=.07):
  a,b=Vector(a),Vector(b);u=(b-a).normalized();v=u.cross(Vector((0,0,1))).normalized()*width/2
  if v.length<.00001:v=Vector((width/2,0,0))
  w=u.cross(v).normalized()*width/2;aa=[a+v+w,a-v+w,a-v-w,a+v-w];bb=[b+v+w,b-v+w,b-v-w,b+v-w]
  for j in range(4):self.face([aa[j],aa[(j+1)%4],bb[(j+1)%4],bb[j]])
def stop():
 g=Mesh()
 # Six dark iron posts rise from cream plinths, with no buried caps.
 for x in [-3.6,0,3.6]:
  for y in [-1,1]:
   g.box(x-.35,x+.35,y-.35,y+.35,0,.4,False)
   g.lathe(x,y,[(.4,.24),(.52,.23),(.60,.15),(2.35,.12),(2.42,.18),(2.50,.22),(2.67,.25),(2.73,.25)])
   g.diagonal((x,y,2.35),(x+(.4 if x<=0 else -.4),y,2.73),.1)
 # Gabled roof is a closed thin prism with finished visible underside.
 profile=[(-1.5,2.85),(0,3.5),(1.5,2.85),(1.5,2.73),(0,3.38),(-1.5,2.73)]
 g.face([(-4,y,z) for y,z in reversed(profile)]);g.face([(4,y,z) for y,z in profile])
 for i,(y,z) in enumerate(profile):
  yy,zz=profile[(i+1)%len(profile)];g.face([(-4,y,z),(4,y,z),(4,yy,zz),(-4,yy,zz)])
 # Chunky transverse beams under roof, gaps between beams and roof remain visible.
 for x in [-3.6,0,3.6]:g.box(x-.10,x+.10,-1.25,1.25,2.63,2.73,True,True)
 # Three broad timber seat slats and two back slats; simple iron supports.
 for y in [.37,.60,.83]:g.box(-2.75,2.75,y,y+.19,.52,.60,True,True)
 for z in [.78,1.04]:g.box(-2.75,2.75,.98,1.06,z,z+.18,True,True)
 for x in [-2.45,2.45]:
  for y in [.46,.86]:g.box(x-.07,x+.07,y-.07,y+.07,0,.52,False)
  g.box(x-.07,x+.07,.98,1.06,.25,1.22,True,True)
 # Timetable panel sits at one end and is legible from front/back.
 g.box(2.8,3.4,-.12,.02,1.0,2.1,True,True)
 # Blank name-board under front eave, deliberately clear of all supports.
 g.box(-2.2,2.2,-1.36,-1.23,2.35,2.70,True,True)
 return g
def kiosk():
 g=Mesh();g.box(-2,2,-2,2,0,.15)
 # Outer booth skin; side and rear lettering/details are painted on opaque wood.
 g.face([(-1.7,1.7,.15),(-1.7,-1.2,.15),(-1.7,-1.2,3),(-1.7,1.7,3)])
 g.face([(1.7,-1.2,.15),(1.7,1.7,.15),(1.7,1.7,3),(1.7,-1.2,3)])
 g.face([(1.7,1.7,.15),(-1.7,1.7,.15),(-1.7,1.7,3),(1.7,1.7,3)])
 g.face([(-1.7,-1.2,.15),(1.7,-1.2,.15),(1.7,-1.2,1.05),(-1.7,-1.2,1.05)])
 for x0,x1 in [(-1.7,-1.4),(1.4,1.7)]:g.face([(x0,-1.2,1.05),(x1,-1.2,1.05),(x1,-1.2,2.55),(x0,-1.2,2.55)])
 g.face([(-1.7,-1.2,2.55),(1.7,-1.2,2.55),(1.7,-1.2,3),(-1.7,-1.2,3)])
 # Window return lining makes the service opening solid from every angle.
 for x in [-1.4,1.4]:g.face([(x,-1.2,1.05),(x,-1.05,1.05),(x,-1.05,2.55),(x,-1.2,2.55)])
 g.face([(-1.4,-1.2,2.55),(1.4,-1.2,2.55),(1.4,-1.05,2.55),(-1.4,-1.05,2.55)])
 # Visible interior surfaces, separate from outer wall skins by timber thickness.
 g.face([(-1.55,-1.05,.15),(1.55,-1.05,.15),(1.55,1.55,.15),(-1.55,1.55,.15)])
 g.face([(-1.55,1.55,.15),(1.55,1.55,.15),(1.55,1.55,2.98),(-1.55,1.55,2.98)])
 for x in [-1.55,1.55]:g.face([(x,-1.05,.15),(x,1.55,.15),(x,1.55,2.98),(x,-1.05,2.98)])
 # Counter projects into shallow front recess, within exact4m platform bounds.
 g.box(-1.6,1.6,-1.50,-.95,1.05,1.16,True,True)
 # Roof underside/four visible hip slopes end at the exact nominal crest3.5m.
 g.face([(-2,2,3), (2,2,3),(2,-1.2,3),(-2,-1.2,3)])
 corners=[(-2,-1.2,3.12),(2,-1.2,3.12),(2,2,3.12),(-2,2,3.12)]
 for i,p in enumerate(corners):
  q=corners[(i+1)%4];g.face([p,q,(0,.4,3.5)]);g.face([(p[0],p[1],3),(q[0],q[1],3),q,p])
 # Striped canopy: closed flat solid with angled top and straight valance.
 top=[(-1.75,-1.2,2.98),(1.75,-1.2,2.98),(1.75,-2,2.80),(-1.75,-2,2.80)]
 g.face(top);g.face([(-1.75,-2,2.70),(1.75,-2,2.70),(1.75,-1.2,2.88),(-1.75,-1.2,2.88)])
 for i,p in enumerate(top):
  q=top[(i+1)%4];g.face([p,(p[0],p[1],p[2]-.1),(q[0],q[1],q[2]-.1),q])
 # Blank auction sign board backs onto the front roof plane.
 g.box(-1.40,1.40,-1.22,-1.10,3.02,3.45,True,True)
 # Low brass service bell,8-sided stylized dome and button.
 g.lathe(.95,-1.30,[(1.16,.12),(1.19,.14),(1.23,.11),(1.29,.065),(1.30,.025),(1.34,.025)],8,True)
 for x in [-1.65,1.65]:g.diagonal((x,-1.2,2.35),(x,-1.90,2.75),.1)
 return g
plan=[('LM_RedCarStop',[8,3,3.5],stop,[{'id':'SIGN_PacificElectric','position':[0,-1.375,2.525],'size':[4.2,.28],'front':'-Y'},{'id':'SIGN_RedCarTimetable','position':[3.1,-.135,1.55],'size':[.50,.95],'front':'-Y'}]),('LM_Kiosk_Auction',[4,4,3.5],kiosk,[{'id':'SIGN_Auction','position':[0,-1.235,3.235],'size':[2.6,.32],'front':'-Y'}])]
entries=[];queue=[]
for aid,nominal,fn,signs in plan:
 assert all((R/'concept'/f'{aid}_{v}.png').exists() for v in ['34','front'])
 g=fn();me=bpy.data.meshes.new(aid);me.from_pydata(g.v,[],g.f);me.update();ob=bpy.data.objects.new(aid,me);bpy.context.collection.objects.link(ob)
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.008,area_weight=1,correct_aspect=True,scale_to_bounds=True);bpy.ops.object.mode_set(mode='OBJECT')
 mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True;mat.diffuse_color=(.30,.33,.19,1);mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.30,.33,.19,1);mat.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.9;me.materials.append(mat)
 source=S/f'{aid}.blend';bpy.ops.wm.save_as_mainfile(filepath=str(source));out=R/'work'/f'{aid}_input.glb';bpy.ops.export_scene.gltf(filepath=str(out),export_format='GLB',use_selection=True)
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(out));ck=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');vv=[ck.matrix_world@v.co for v in ck.data.vertices]
 bounds=[[round(fn(v[k] for v in vv),6) for k in range(3)] for fn in [min,max]];bm=bmesh.new();bm.from_mesh(ck.data)
 audit={'bounds':bounds,'tris':sum(len(p.vertices)-2 for p in ck.data.polygons),'material_slots':len(ck.material_slots),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'triangulated':all(len(p.vertices)==3 for p in ck.data.polygons),'scale':list(ob.scale),'rotation':list(ob.rotation_euler)};bm.free();bpy.data.objects.remove(ck,do_unlink=True)
 assert audit['tris']<=1500 and audit['material_slots']==1,(aid,audit)
 assert audit['zero_area_faces']==audit['branch_edges']==audit['loose_vertices']==0,(aid,audit)
 for k,n in enumerate(nominal):assert abs(bounds[1][k]-bounds[0][k]-n)<.0001,(aid,nominal,bounds)
 # Meaningful per-face UV density measured from final authored triangles.
 densities=[]
 for p in me.polygons:
  vv=[Vector(uv.uv) for uv in [me.uv_layers.active.data[i] for i in p.loop_indices]];ua=abs((vv[1].x-vv[0].x)*(vv[2].y-vv[0].y)-(vv[1].y-vv[0].y)*(vv[2].x-vv[0].x))*.5
  if p.area>1e-10:densities.append(math.sqrt(ua/p.area)*2048)
 entry={'id':aid,'kind':'landmark','nominal':nominal,'fits_box':None,'non_colliding':True,'origin':'footprint-centre-ground','material':'M_'+aid,'source':str(source.relative_to(R)).replace('\\','/'),'model':str(out.relative_to(R)).replace('\\','/'),'concept':[f'concept/{aid}_34.png',f'concept/{aid}_front.png'],'prompt':f'src/hub_root/{aid}_prompt.json','bounds':bounds,'tris':audit['tris'],'audit':audit,'sign_attachments':signs,'uv':{'master':[2048,2048],'method':'unique smart-projected islands with approximately16px padding','minimum_px_m':round(min(densities),3),'median_px_m':round(sorted(densities)[len(densities)//2],3)},'notes':['Concepts generated and researched by root before this authored model.','All lettering panels blank, no texturing calls by subagent.','Roof underside and accessible backs are finished. Ground-facing and buried caps omitted.','SPEC exact dimensions enforced; hidden joints and layered timber braces may retain tiny overlap at joinery.']}
 (W/f'{aid}_metadata.json').write_text(json.dumps(entry,indent=2));entries.append(entry)
 queue.append({'id':aid,'model':entry['model'],'source':entry['source'],'reference':entry['concept'][0],'secondary_reference':entry['concept'][1],'material':'Shaded','resolution':'Basic','bake_to_authored_uvs':True,'prompt':'Classic early MMO hand-painted1930 '+('Pacific Electric red-car stop, terracotta barrel-tile gabled roof, bottle-green iron posts, cream bases, warm wood slat bench, blank cream nameboard and timetable' if aid=='LM_RedCarStop' else 'auction kiosk, weathered dark timber and bottle-green hip roof, oxblood-and-cream striped front canopy, dark painted opaque side glass, little brass service bell, blank cream auction sign')+'. Warm painted top light and cool AO, no text or logos, no people, modern objects, background, ground, transparency or photographic texture. Preserve authored mesh and UVs.'})
 (W/'shelter_assets.json').write_text(json.dumps(entries,indent=2));(W/'shelter_texture_request_queue.json').write_text(json.dumps(queue,indent=2));print('FIRST_READY',aid,audit['tris'],bounds,entry['uv'],flush=True);bpy.data.objects.remove(ob,do_unlink=True)
