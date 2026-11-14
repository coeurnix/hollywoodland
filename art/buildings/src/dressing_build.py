"""Hand-authored phase-2 occupation dressing. Blender 5.2 background script.
Only models an asset after both generated concept views exist. No paid calls.
Run: blender --background --python art/buildings/src/dressing_build.py
"""
import bpy,bmesh,json,math,sys,random
from pathlib import Path
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
R=Path(__file__).resolve().parents[1]
W=R/'work/dressing';W.mkdir(parents=True,exist_ok=True)
plan=json.loads((R/'src/dressing_plan.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
bpy.context.preferences.filepaths.save_version=0
out=[];queue=[]
class Mesh:
 def __init__(self):self.v=[];self.f=[]
 def face(self,vs):
  s=len(self.v);self.v.extend(vs);self.f.append(tuple(range(s,s+len(vs))))
 def prism(self,outline,y0,y1,back=True):
  # CCW x/z polygon has outward front normal -Y.
  self.face([(x,y0,z) for x,z in outline])
  if back:self.face([(x,y1,z) for x,z in reversed(outline)])
  for i,(x,z) in enumerate(outline):
   xx,zz=outline[(i+1)%len(outline)]
   self.face([(x,y0,z),(x,y1,z),(xx,y1,zz),(xx,y0,zz)])
 def box(self,x0,x1,y0,y1,z0,z1,back=True):self.prism([(x0,z0),(x1,z0),(x1,z1),(x0,z1)],y0,y1,back)
 def bag(self,cx,cy,z0,w,d,h):
  # Three octagonal latitude rings, broad silhouette with a pinched seam.
  rings=[]
  for z,r in [(.001,.72),(.48,1),(.999,.72)]:
   ring=[]
   for j in range(8):
    t=2*math.pi*j/8
    ring.append((cx+w*.5*r*math.cos(t),cy+d*.5*r*math.sin(t),z0+h*z))
   rings.append(ring)
  self.face(list(reversed(rings[0])))
  for a,b in zip(rings,rings[1:]):
   for j in range(8):self.face([a[j],a[(j+1)%8],b[(j+1)%8],b[j]])
  self.face(rings[-1])
 def board(self,w,h,cx,cz,y=-.07,thick=.05,angle=0):
  pts=[(-w/2,-h/2),(w*.22,-h/2),(w*.27,-h*.40),(w*.31,-h/2),(w/2,-h*.43),(w/2,h*.38),(w*.20,h/2),(-w/2,h*.44)]
  c,s=math.cos(angle),math.sin(angle)
  self.prism([(cx+c*x-s*z,cz+s*x+c*z) for x,z in pts],y,y+thick,False)

def make(a):
 aid=a['id'];w,d,h=a['size'];g=Mesh();notes=[]
 if 'BoardedWindow' in aid:
  count=3 if w==1.5 else 6
  for i in range(count):g.board(w,.34 if count==3 else .36,w/2,.18+(h-.36)*i/(count-1),y=-.075)
  # Two vertical rear rails, diagonal front brace; ends remain within nominal bounds.
  for x in [w*.12,w*.88]:g.box(x-.055,x+.055,-.025,0,0,h,False)
  p1=Vector((w*.12,h*.12));p2=Vector((w*.88,h*.88));mid=(p1+p2)/2
  g.board((p2-p1).length,.17,mid.x,mid.y,-d,.035,math.atan2(p2.y-p1.y,p2.x-p1.x))
  notes.append('Open wall-facing backs intentionally omitted; nail heads belong in painted texture.')
 elif aid=='DR_SandbagStack3':
  for row in range(3):
   widths=([.75,.75,.75,.75] if row==1 else [1,1,1]);cursor=-1.5
   for bw in widths:g.bag(cursor+bw/2,0,row/3,bw-.015,.3,1/3);cursor+=bw
 elif aid=='DR_SandbagDoorway4':
  for side in [-1,1]:
   for row in range(10):g.bag(side*1.5,0,row*.3,.985,.3,.3)
  g.board(4,.25,0,3.125,-.15,.3)
  notes.append('OPEN 2 m central passage; independent decorative dressing, no collision supplied.')
 elif aid=='DR_FallenAwning':
  # Torn folded canvas surface, visible from both sides using a thin closed solid.
  top=[(-2,-.75,.1),(2,-.75,.16),(2,.65,.48),(1.2,.75,.6),(.75,.45,.41),(.3,.75,.58),(-2,.75,.52)]
  g.face(top);g.face([(x,y,z-.035) for x,y,z in reversed(top)])
  for i,v in enumerate(top):
   vv=top[(i+1)%len(top)];g.face([v,(v[0],v[1],v[2]-.035),(vv[0],vv[1],vv[2]-.035),vv])
  for yy in [-.66,.58]:g.box(-2,2,yy-.035,yy+.035,0,.09)
 elif aid.startswith('DR_Soot'):
  outline=([(0,.1),(.12,0),(1.8,0),(2,.25),(1.7,.5),(1.65,.85),(1.48,.67),(1.40,1.2),(1.22,.85),(1.02,1.5),(.85,.9),(.6,1.25),(.47,.63),(.24,.8),(.25,.35)] if aid.endswith('A') else [(0,.1),(.18,0),(1.38,0),(1.5,.3),(1.2,.7),(1.3,1.4),(1.05,1.2),(.95,2.5),(.73,1.4),(.45,2),(.5,.9),(.22,1.2),(.25,.5)])
  g.face([(x,-.006,z) for x,z in outline]);notes.append('Opaque silhouette cutout; intentionally open single surface, no masked/translucent material. Hard outer stain edge is a documented approximation of feathered soot.')
 elif aid=='DR_Crack':
  # One coherent concave outline forms a connected forked crack; no alpha needed.
  p=[(.85,0),(.95,.5),(.65,.82),(.2,.95),(0,1.2),(.25,1.03),(.73,.94),(.99,.65),(1.1,1.0),(.85,1.35),(.95,2),(1.01,1.38),(1.2,1.1),(1.5,1.5),(2,1.65),(1.53,1.43),(1.24,.92),(1.08,.52),(.94,0)]
  # reverse to face -Y
  g.face([(x,-.006,z) for x,z in reversed(p)]);notes.append('Opaque geometry outline with broad stylized fissures; single open surface, no alpha or backing wall.')
 elif aid=='DR_MayorNotice':
  g.face([(0,-.008,0),(.75,-.008,0),(.75,-.008,1),(0,-.008,1)])
  notes.append('Lettering is deterministically composed from project OFL fonts; no Rodin call required under sign exception section 10.1.')
 elif aid.startswith('DR_Rubble'):
  rng=random.Random(614 if w==2 else 127);n=8 if w==2 else 19
  # Authored angular visible masonry chunks distributed inside exact footprint.
  for i in range(n):
   angle=2*math.pi*i/n;rad=.62 if i%3 else .22
   cx=math.cos(angle)*(w*.5-.42)*rad;cy=math.sin(angle)*(d*.5-.32)*rad
   bw=rng.uniform(.28,.65);bh=rng.uniform(.18,.4);dep=rng.uniform(.25,.48)
   zz=.02+(h-.4)*(1-rad)
   outline=[(cx-bw/2,zz),(cx+bw*.4,zz),(cx+bw/2,zz+bh*.65),(cx+bw*.25,zz+bh),(cx-bw*.4,zz+bh*.85)]
   g.prism(outline,cy-dep/2,cy+dep/2,True)
  notes.append('Undersides retained only on elevated chunks visible through pile gaps; hidden ground-facing faces removed after z normalization.')
 elif aid=='DR_BrokenGlass':
  # Four strips plus teeth around a genuinely empty center, all opaque.
  polys=[[(0,0),(3,0),(3,.3),(2.6,.3),(2.35,.72),(2.1,.22),(1.6,.22),(1.3,.48),(.9,.22),(.3,.35),(.2,.62),(0,.45)],[(0,.45),(.2,.62),(.25,1.1),(.58,1.4),(.23,1.7),(.34,2.2),(0,2.5)],[(0,2.5),(.34,2.2),(.9,2.32),(1.2,1.95),(1.5,2.3),(2.2,2.28),(2.6,2.05),(3,2.5)],[(3,.3),(3,2.5),(2.6,2.05),(2.77,1.8),(2.4,1.5),(2.75,1.18),(2.7,.65)]]
  for p in polys:g.face([(x,-.006,z) for x,z in p])
  notes.append('Opaque painted dark glass remains only in cutout perimeter shards. Hole contains no geometry; no translucency, refraction or alpha.')
 me=bpy.data.meshes.new(aid);me.from_pydata(g.v,[],g.f);me.update();ob=bpy.data.objects.new(aid,me);bpy.context.collection.objects.link(ob)
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 # Dissolve/triangulate author polygons robustly, then recalculate orientation.
 bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 # Nominal exact dimensions, quarter-meter overall module boundaries.
 bounds=[[fn(v.co[k] for v in me.vertices) for k in range(3)] for fn in [min,max]]
 if a['origin']=='ground-centre':
  for v in me.vertices:
   for k,target in [(0,w),(1,d),(2,h)]:
    lo,hi=bounds[0][k],bounds[1][k];v.co[k]=(v.co[k]-lo)/(hi-lo)*target-(target/2 if k<2 else 0)
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.012,area_weight=1,correct_aspect=True,scale_to_bounds=True);bpy.ops.object.mode_set(mode='OBJECT')
 mat=bpy.data.materials.new('M_'+aid);mat.diffuse_color=(.48,.31,.13,1);mat.use_nodes=True;mat.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.92;me.materials.append(mat)
 tris=len(me.polygons);assert tris<=a['budget'],(aid,tris,a['budget'])
 bm=bmesh.new();bm.from_mesh(me)
 audit={'tris':tris,'material_slots':len(me.materials),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts)};bm.free()
 assert audit['zero_area_faces']==audit['branch_edges']==audit['loose_vertices']==0,(aid,audit)
 bounds=[[round(fn(v.co[k] for v in me.vertices),6) for k in range(3)] for fn in [min,max]]
 bpy.ops.export_scene.gltf(filepath=str(W/f'{aid}_input.glb'),export_format='GLB',use_selection=True,export_yup=True)
 # Export/reimport a second GLB and measure delivered geometry, not intended values.
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(W/f'{aid}_input.glb'));check=next(o for o in set(bpy.data.objects)-old if o.type=='MESH')
 vv=[check.matrix_world@v.co for v in check.data.vertices];audit['bounds']=[[round(fn(v[k] for v in vv),6) for k in range(3)] for fn in [min,max]];audit['export_tris']=sum(len(p.vertices)-2 for p in check.data.polygons);assert audit['export_tris']==tris;bpy.data.objects.remove(check,do_unlink=True)
 entry={**a,'kind':'dressing','bounds':bounds,'tris':tris,'material':'M_'+aid,'input':f'work/dressing/{aid}_input.glb','concept':[f'concept/{aid}_34.png',f'concept/{aid}_front.png'],'audit':audit,'notes':notes}
 return ob,entry

for a in plan:
 if not all((R/'concept'/f"{a['id']}_{v}.png").exists() for v in ['34','front']):continue
 ob,entry=make(a);out.append(entry)
 if a['id']!='DR_MayorNotice':queue.append({'id':a['id'],'model':entry['input'],'reference':entry['concept'][0],'material':'Shaded','resolution':'Basic','prompt':'Texture only: '+a['subject']+'. Classic MMO painted warm top light, cool base shadows, restrained brush strokes; no background, characters, typography, modern objects, transparency or hive resin. Preserve authored geometry and UVs.','bake_to_authored_uvs':True,'atlas':'ATL_OCCUPATION'})
 ob.hide_set(True)
json.dumps(out)
(W/'dressing_assets.json').write_text(json.dumps(out,indent=2))
(W/'texture_request_queue.json').write_text(json.dumps(queue,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/dressing_authored.blend'))
print('DRESSING READY',[(a['id'],a['tris']) for a in out])
