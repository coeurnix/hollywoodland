"""Authored Chinese Theatre and forecourt wing from root's reviewed concepts.
Inputs are released only after positional welding AND actual FBX round-trip.
Blender --background --python art/buildings/src/hub_egyptian/build_graumans.py
"""
import bpy,bmesh,json,math,sys,ast
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];S=R/'src/hub_egyptian';W=R/'work/hub_egyptian'
sys.path.insert(0,str(R/'src'));from asset_io import export_asset
# Reuse the already-reviewed face construction and semantic UV machinery only;
# executing selected definitions never runs the Egyptian builder/export loop.
tree=ast.parse((S/'build.py').read_text());wanted=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['Mesh','roofgrid','walls']]
exec(compile(ast.Module(body=wanted,type_ignores=[]),str(S/'build.py'),'exec'))
slots={'wall':(16,16,1000,600,50),'column':(1040,16,400,600,50),'door':(1460,16,570,400,50),'mural':(16,650,1000,400,50),'frieze':(1040,650,990,200,50),'roof':(16,1090,520,520,25),'side':(560,1090,800,740,25),'tile':(16,1650,520,380,25),'trim':(1380,890,650,200,50),'plant':(1380,1130,650,900,100)}
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.preferences.filepaths.save_version=0;bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1

def coral_column(g,cx,cy):
 levels=[(0,.95),(.25,.95),(.4,.82),(.60,.78),(9.6,.76),(9.8,.86),(10.2,1),(10.5,1),(11.5,.85)]
 rings=[[(cx+r*math.cos(j*2*math.pi/20),cy+r*math.sin(j*2*math.pi/20),z) for j in range(20)] for z,r in levels]
 for k,(a,b) in enumerate(zip(rings,rings[1:])):
  for j in range(20):
   x,y,_,_,px=slots['column'];u0=x+2*math.pi*50*j/20;u1=x+2*math.pi*50*(j+1)/20;v0=y+levels[k][0]*50;v1=y+levels[k+1][0]*50
   g.face([a[j],a[(j+1)%20],b[(j+1)%20],b[j]],'column',[(u0/2048,v0/2048),(u1/2048,v0/2048),(u1/2048,v1/2048),(u0/2048,v1/2048)])

def dog(g,cx,cy):
 # Single connected chunky guardian: seated haunch, thick chest, wide mane/head,
 # squared front muzzle and two raised ears are built into the same ring skin.
 g.box(cx-.9,cx+.9,cy-.72,cy+.72,0,.35,'trim')
 levels=[(.35,.72,.59),(.55,.75,.62),(1.0,.60,.52),(1.35,.58,.49),(1.55,.72,.55),(1.95,.78,.63),(2.25,.67,.62),(2.48,.53,.48)]
 rings=[]
 for k,(z,rx,ry) in enumerate(levels):
  ring=[]
  for j in range(12):
   t=j*math.pi/6;x=cx+rx*math.cos(t);y=cy+ry*math.sin(t);zz=z
   if k in [5,6] and j in [8,9,10]:y-=.12 # protruding readable front muzzle
   if k==7 and j in [0,6]:zz+=.27 # broad pointed stone ears
   ring.append((x,y,zz))
  rings.append(ring)
 for a,b in zip(rings,rings[1:]):
  for j in range(12):g.face([a[j],a[(j+1)%12],b[(j+1)%12],b[j]],'plant')
 g.face(rings[-1],'plant')

def pagoda(g):
 # Hipped square-curved canopy compressed under16m; front remains y=-29.
 # Four16/32-sided rectangles keep the upturned corner sweep readable overhead.
 rings=[];n=8
 for rx,ry,z,rise in [(4,1.7,16,0),(8,3.1,14.8,.15),(14,5.2,13.1,.45),(20,7,12.9,1.7)]:
  rr=[]
  corners=[(-rx,-ry),(rx,-ry),(rx,ry),(-rx,ry)]
  for k in range(4):
   a,b=Vector(corners[k]),Vector(corners[(k+1)%4])
   for j in range(n):
    t=j/n;p=a.lerp(b,t);rr.append((p.x,p.y-22,z+rise*(abs(2*t-1)**3)))
  rings.append(rr)
 for top in [True,False]:
  off=0 if top else -.18
  for a,b in zip(rings,rings[1:]):
   for j in range(32):
    vs=[a[j],b[j],b[(j+1)%32],a[(j+1)%32]]
    if not top:vs.reverse()
    g.face([(x,y,z+off) for x,y,z in vs],'tile')
  # The visible crown is closed; its underside is an interface to the support
  # drum, so neither a hidden underside nor a coincident drum top is authored.
  if top:
   for i in range(4):
    x0=-4+2*i;x1=x0+2;vs=[(x0,-23.7,16),(x1,-23.7,16),(x1,-20.3,16),(x0,-20.3,16)]
    g.face(vs,'tile')
 # Segment the support drum to the SAME crown-ring vertices; UV-preserving
 # positional welding then joins the roof/drum interface without T junctions.
 inner=rings[0]
 for j,a in enumerate(inner):
  b=inner[(j+1)%32]
  g.face([(a[0],a[1],12.3),(b[0],b[1],12.3),(b[0],b[1],15.82),(a[0],a[1],15.82)],'wall')
 # Exposed perimeter thickness, all faces visible when viewed from below.
 outer=rings[-1]
 for j,a in enumerate(outer):
  b=outer[(j+1)%32];g.face([a,(a[0],a[1],a[2]-.18),(b[0],b[1],b[2]-.18),b],'trim')

def theatre():
 g=Mesh();w,d=82,58;walls(g,w,d,11.5,False)
 for x0,x1 in [(-41,-37),(-37,-28),(-28,-20),(-20,-14),(14,20),(20,28),(28,37),(37,41)]:
  for z0,z1 in [(0,.6),(.6,8.8),(8.8,10.5),(10.5,11.5)]:g.front(x0,x1,z0,z1,-29,'wall')
 # Recess2m, with broad dragon panel over the dark10m central entry.
 for x0,x1 in [(-14,-8),(-8,-5),(-5,5),(5,8),(8,14)]:
  for z0,z1 in [(0,5.5),(5.5,11.5)]:
   if x0>=-8 and x1<=8 and z0==5.5:continue
   g.front(x0,x1,z0,z1,-27,'door' if x0==-5 and z0==0 else 'wall')
 # One continuous island for the unique16m dragon panel, no repeated fragments.
 g.front(-8,8,5.5,11.5,-27,'mural')
 for x,flip in [(-14,True),(14,False)]:g.side(x,-29,-27,0,11.5,'wall',flip)
 # Lintel underside shares only boundary edges with wall returns; end caps omitted.
 for x0,x1 in [(-14,-5),(-5,5),(5,14)]:
  g.face([(x0,-27,11.5),(x1,-27,11.5),(x1,-29,11.5),(x0,-29,11.5)],'trim');g.top(x0,x1,-29,-27,12.3,'trim')
 for x in [-11.5,11.5]:coral_column(g,x,-28)
 for x in [-8.5,8.5]:dog(g,x,-28.1)
 g.bands(w,d,[(11.5,0),(11.8,.15),(12.1,0),(12.3,0)])
 # Main roof explicitly omits canopy footprint; no invisible roof trapped below it.
 xx=[-41,-27,-20,0,20,27,41];yy=[-29,-15,0,15,29]
 for a,b in zip(xx,xx[1:]):
  for c,e in zip(yy,yy[1:]):
   if a>=-20 and b<=20 and c==-29:continue
   g.top(a,b,c,e,12.3)
 # Pagoda authors its own open-topped support drum at the canopy interface.
 pagoda(g)
 # Chunky exposed red corbel brackets along side/front eaves.
 for x in [-35,-29,-23,23,29,35]:
  g.box(x-.28,x+.28,-28.95,-28.70,10.65,11.5,'trim',False,True)
 for x in [-39.5,39.5]:
  for y in [-27.5,27.5]:g.box(x-1.5,x+1.5,y-1.5,y+1.5,12.3,12.75,'trim')
 return g

def wall():
 g=Mesh();w,d=6,20
 # Integrated pilaster skin along both long sides; no buried duplicate pier faces.
 yy=sorted(set([-10,10]+[c+a for c in [-9.6,-5,0,5,9.6] for a in [-.4,.4] if -10<c+a<10]))
 for side in [-1,1]:
  for i,(a,b) in enumerate(zip(yy,yy[1:])):
   mid=(a+b)/2;pier=any(abs(mid-c)<.401 for c in [-9.6,-5,0,5,9.6]);x=side*(3 if pier else 2.82)
   for z0,z1 in [(0,.4),(.4,3.8),(3.8,4.35)]:g.side(x,a,b,z0,z1,'trim' if pier else 'wall',side<0)
  # Profile returns connect differing offsets at each pier edge.
  for i,y in enumerate(yy[1:-1],1):
   left=any(abs((yy[i-1]+y)/2-c)<.401 for c in [-9.6,-5,0,5,9.6]);right=any(abs((y+yy[i+1])/2-c)<.401 for c in [-9.6,-5,0,5,9.6])
   if left!=right:g.front(min(side*(3 if left else 2.82),side*(3 if right else 2.82)),max(side*(3 if left else 2.82),side*(3 if right else 2.82)),0,4.35,y,'trim')
 g.front(-3,3,0,4.35,-10,'mural');g.face([(3,10,0),(-3,10,0),(-3,10,4.35),(3,10,4.35)],'wall')
 g.bands(6,20,[(4.35,0),(4.6,.1),(4.75,0)])
 # Low verdigris curved coping rises to exact5m at the outer roof corners.
 g.top(-3,3,-10,10,4.75)
 # Opaque, integrated narrow roof-edge strips with lifted short-end tips.
 for side in [-1,1]:
  for a,b in zip([-10,-8,0,8],[-8,0,8,10]):
   za=5 if abs(a)==10 else 4.85;zb=5 if abs(b)==10 else 4.85
   g.face([(side*3,a,za),(side*2.7,a,4.75),(side*2.7,b,4.75),(side*3,b,zb)],'tile')
 for y in [-10,10]:g.face([(-3,y,5),(3,y,5),(2.7,y,4.75),(-2.7,y,4.75)],'tile')
 return g

reports=[];entries=[];queue=[]
for aid,nominal,budget,fn in [('LM_Graumans',[82,58,16],20000,theatre),('LM_Graumans_ForecourtWall',[6,20,5],2000,wall)]:
 assert all((R/'concept'/f'{aid}_{view}.png').exists() for view in ['34','front'])
 g=fn();me=bpy.data.meshes.new(aid);me.from_pydata(g.v,[],g.f);me.update();uv=me.uv_layers.new(name='UVMap')
 for i,co in enumerate(g.uv):uv.data[i].uv=co
 ob=bpy.data.objects.new(aid,me);bpy.context.collection.objects.link(ob);bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True;mat.diffuse_color=(.49,.47,.23,1);mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.49,.47,.23,1);mat.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.9;me.materials.append(mat)
 entry,audit=export_asset(ob,{'id':aid},budget,W/'graumans_roundtrip')
 # FBX audit includes positions welded again, independent of UV seam duplication.
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(W/'graumans_roundtrip'/entry['file']));ck=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');bm=bmesh.new();bm.from_mesh(ck.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 welded={'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'inconsistent_shared_edges':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges)};bm.free();bpy.data.objects.remove(ck,do_unlink=True);assert all(v==0 for v in welded.values()),(aid,welded)
 for k,n in enumerate(nominal):assert abs(audit['bounds'][1][k]-audit['bounds'][0][k]-n)<.0001,(aid,nominal,audit['bounds'])
 source=S/f'{aid}.blend';bpy.ops.wm.save_as_mainfile(filepath=str(source));out=R/'work'/f'{aid}_input.glb';bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob;bpy.ops.export_scene.gltf(filepath=str(out),export_format='GLB',use_selection=True)
 signs=([{'id':'SIGN_Graumans_Marquee','position':[0,-27.015,5.2],'size':[10,1],'front':'-Y'},{'id':'SIGN_Graumans_Blade','position':[-35,-29.015,6],'size':[4,8],'front':'-Y'}] if aid=='LM_Graumans' else [{'id':'SIGN_Graumans_Poster','position':[0,-10.015,2.3],'size':[3.5,3],'front':'-Y'}])
 metadata={'id':aid,'kind':'landmark','nominal':nominal,'origin':'footprint-centre-ground','fits_box':"Grauman's Chinese Theatre" if aid=='LM_Graumans' else None,'also_fits':[] if aid=='LM_Graumans' else [{'size':nominal,'mirror':True}],'material':'M_'+aid,'source':str(source.relative_to(R)).replace('\\','/'),'model':str(out.relative_to(R)).replace('\\','/'),'concept':[f'concept/{aid}_34.png',f'concept/{aid}_front.png'],'prompt':f'src/hub_root/{aid}_prompt.json','bounds':audit['bounds'],'tris':audit['tris'],'audit':audit,'welded_fbx_audit':welded,'sign_attachments':signs,'uv':{'master':[2048,2048],'slots':slots,'street_px_m':50,'roof_back_side_px_m':25,'padding_px':16,'minimum_role_density':min(v for _,v in g.densities)},'notes':['Root generated/reviewed concepts and institutional historical research before modeling.','Exact collision box wins over compressed historical roof proportions.','Original painted dragon panel, gold iron-mask accents, glass reflections and stone dog face/paws are texture detail.','No photograph copied to texture, no lettering, transparency or Rodin calls here.','All surfaces accessible from roof/back/side are finished; ground-facing caps and buried wall/roof portions omitted.','Main portal recessed2m; guardians sit inside box beside entrance.']}
 (W/f'{aid}_metadata.json').write_text(json.dumps(metadata,indent=2));entries.append(metadata);reports.append({'id':aid,'fbx_roundtrip':audit,'welded_fbx':welded})
 subject=("Broad low Chinese Theatre main block: yellow-green plaster, coral-red columns and trim, cream stone, dark navy/gold doors, verdigris copper pagoda roof and one original painted dragon frieze across the central16m upper-entry panel. Two stone guardian dogs have painted carved faces, manes and paws." if aid=='LM_Graumans' else "One long Chinese Theatre forecourt pavilion wall: yellow-green plaster with repeating coral-red Chinese geometric painted borders, cream concrete pilasters, dark green stone base, narrow verdigris roof coping. The short front end has a dark blank inset poster backing, long sides repeat simple decorative geometric panels. Finish top and back.")
 queue.append({'id':aid,'model':metadata['model'],'source':metadata['source'],'reference':metadata['concept'][0],'secondary_reference':metadata['concept'][1],'material':'Shaded','resolution':'Basic','bake_to_authored_uvs':True,'prompt':"Classic early MMO hand-painted1930 Grauman's Chinese Theatre architecture. "+subject+" Warm painted top light, cool AO. No text, background, people, modern objects, hive growth, transparency or photographic texture. Preserve authored geometry and UVs."})
 (W/'graumans_assets.json').write_text(json.dumps(entries,indent=2));(W/'graumans_texture_request_queue.json').write_text(json.dumps(queue,indent=2));(W/'graumans_audit.json').write_text(json.dumps(reports,indent=2));print('READY VERIFIED FBX',aid,audit['tris'],audit['bounds'],flush=True);bpy.data.objects.remove(ob,do_unlink=True)
