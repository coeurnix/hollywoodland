"""Rebuild the seven authored phase-2 roof props and texture-only inputs.
Run Blender --background --python src/roof_clutter_build.py [-- RF_WaterTank ...].
Only roof_clutter-owned source/work files are written. No service calls.
"""
import bpy,bmesh,math,json,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
W=R/'work/roof_clutter';W.mkdir(parents=True,exist_ok=True)
PALETTE={'metal':(.115,.235,.215,1),'wood':(.30,.15,.065,1),'slate':(.145,.17,.22,1),'cream':(.66,.57,.39,1),'brick':(.43,.18,.105,1),'door':(.29,.065,.035,1),'glass':(.045,.13,.10,1),'dark':(.025,.033,.036,1)}
ROLE_MATS={}
for role,color in PALETTE.items():
 m=bpy.data.materials.new('PrepRole_'+role);m.diffuse_color=color;ROLE_MATS[role]=m
PARTS=[]
def mesh(name,verts,faces,role):
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update()
 ob=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(ob);ob.data.materials.append(ROLE_MATS[role]);PARTS.append(ob)
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 return ob
def box(name,c,d,role):
 x,y,z=c;a,b,h=[n/2 for n in d]
 v=[(x+sx*a,y+sy*b,z+sz*h) for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
 return mesh(name,v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],role)
def beam(name,a,b,width,depth=None,role='metal'):
 a,b=Vector(a),Vector(b);d=b-a;ob=box(name,(0,0,0),(width,depth or width,d.length),role);ob.rotation_euler=d.to_track_quat('Z','Y').to_euler();ob.location=(a+b)/2
 bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);ob.select_set(False);return ob
def lathe(name,rings,n=12,role='metal',caps=True):
 verts=[(r*math.cos(2*math.pi*i/n),r*math.sin(2*math.pi*i/n),z) for r,z in rings for i in range(n)]
 faces=[]
 for j in range(len(rings)-1):
  for i in range(n):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
 if caps:faces.extend([tuple(reversed(range(n))),tuple((len(rings)-1)*n+i for i in range(n))])
 return mesh(name,verts,faces,role)
def slopebox(name,w,d,z0,front,back,thickness,role):
 v=[(-w/2,-d/2,z0),(w/2,-d/2,z0),(w/2,d/2,z0),(-w/2,d/2,z0),(-w/2,-d/2,front),(w/2,-d/2,front),(w/2,d/2,back),(-w/2,d/2,back)]
 if thickness:
  v[:4]=[(-w/2,-d/2,front-thickness),(w/2,-d/2,front-thickness),(w/2,d/2,back-thickness),(-w/2,d/2,back-thickness)]
 return mesh(name,v,[(0,3,2,1),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)],role)
def water():
 lathe('stave_tank',[(1.46,2.04),(1.50,2.13),(1.50,4.55)],12,'wood')
 for z in [2.18,2.80,4.15]:lathe('broad_hoop',[(1.49,z),(1.55,z+.025),(1.55,z+.17),(1.49,z+.195)],12,'metal')
 lathe('conical_roof',[(1.58,4.52),(1.63,4.63),(.13,5.15),(.07,5.22)],12,'slate')
 lathe('finial',[(.09,5.15),(.14,5.23),(.05,5.35),(.025,5.38)],8,'metal')
 for x in [-1,1]:
  for y in [-1,1]:
   box('footplate',(x*1.49,y*1.49,.055),(.47,.47,.11),'metal')
   beam('splayed_leg',(x*1.49,y*1.49,.08),(x*1.05,y*1.05,2.18),.22,.24)
 for axis in [0,1]:
  for side in [-1,1]:
   a=[-1.4,side*1.4,.32];b=[1.09,side*1.09,1.96]
   if axis:a[0],a[1]=a[1],a[0];b[0],b[1]=b[1],b[0]
   beam('diagonal_brace',a,b,.11,.10)
   # One broad diagonal on each side reads clearly without crowded hidden joins.
 for yy in [-.26,.26]:beam('ladder_rail',(1.54,yy,2.13),(1.54,yy,4.53),.055)
 for z in [2.28+i*.32 for i in range(8)]:beam('ladder_rung',(1.56,-.26,z),(1.56,.26,z),.055)
 return {'nominal':[3.45,3.45,5.38],'budget':1500,'notes':'12-sided wooden stave tank, three steel hoops, splayed diagonally-braced frame, service ladder; roof prop.'}
def billboard(w,h,count):
 bottom=1.5 if w==8 else 1.8;top=bottom+h;rear=2.2 if w==8 else 3.1
 # The blank mounting panel is a backing surface. The exact-sized poster is a separate sign card.
 box('poster_backing',(0,0,bottom+h/2),(w,.12,h),'cream')
 for x in [-w/2-.11,w/2+.11]:box('side_frame',(x,-.015,bottom+h/2),(.22,.30,h+.44),'metal')
 for z in [bottom-.11,top+.11]:box('horizontal_frame',(0,-.015,z),(w,.30,.22),'metal')
 xs=[(-w/2+.25)+(w-.5)*i/(count-1) for i in range(count)]
 for x in xs:
  box('front_foot',(x,.14,.06),(.65,.66,.12),'metal');box('rear_foot',(x,rear,.06),(.65,.66,.12),'metal')
  beam('upright',(x,.18,.08),(x,.18,top+.1),.21,.25)
  beam('rear_brace',(x,rear,.10),(x,.18,top-.15),.19,.22)
  beam('rear_tie',(x,.18,bottom),(x,rear*.66,bottom),.13)
  beam('catwalk_bracket',(x,.15,bottom-.35),(x,-.64,bottom-.20),.12)
  beam('catwalk_strut',(x,.18,bottom-.85),(x,-.64,bottom-.20),.105)
 box('catwalk',(0,-.43,bottom-.19),(w+.40,.84,.12),'wood')
 for x in [-w/2]+xs[1:-1]+[w/2]:beam('rail_post',(x,-.81,bottom-.15),(x,-.81,bottom+.48),.055)
 beam('catwalk_handrail',(-w/2,-.81,bottom+.48),(w/2,-.81,bottom+.48),.06)
 for x in xs:
  beam('lamp_stem',(x,.08,top+.15),(x,.08,top+.67),.055)
  beam('lamp_overhang',(x,.08,top+.67),(x,-.45,top+.67),.055)
  ob=lathe('period_shade',[(.23,top+.40),(.23,top+.45),(.065,top+.61),(.05,top+.68)],8,'metal');ob.location.x=x;ob.location.y=-.45
 return {'nominal':[w+.44,rear+1.14,top+.68],'budget':1500,'poster_face':{'size_m':[w,h],'centre':[0,-.066,bottom+h/2],'front':'-Y','bottom_m':bottom,'separate_card':True},'support_dimensions_m':{'rear_depth':rear,'uprights':count,'catwalk_front':-.85},'notes':'Blank structural backing takes a separate sign-page poster. Poster face is exactly %sx%s metres; nominal and bounds include frame/supports.'%(w,h)}
def bulkhead():
 slopebox('stucco_hut',3,4,0,2.78,2.48,None,'cream')
 slopebox('shed_roof',3.22,4.22,0,3.0,2.70,.20,'slate')
 box('base_flashing',(0,0,.105),(3.12,4.12,.21),'metal')
 box('door',(0,-2.045,1.16),(1.1,.13,2.20),'door')
 for x in [-.61,.61]:box('door_jamb',(x,-2.08,1.18),(.15,.22,2.36),'cream')
 box('lintel',(0,-2.08,2.36),(1.37,.22,.16),'cream')
 box('door_drip',(0,-2.20,2.48),(1.56,.56,.12),'metal')
 box('latch',(.36,-2.135,1.1),(.22,.08,.075),'metal')
 box('vent_recess',(1.505,.45,1.8),(.045,.85,.60),'dark')
 for z in [1.57+i*.14 for i in range(4)]:box('vent_louver',(1.54,.45,z),(.11,.88,.055),'metal')
 return {'nominal':[3.22,4.48,3.0],'budget':1500,'notes':'Cream stucco exit hut with an oxblood door, rear-falling shed roof, drip hood, flashing and side louvers; no navigable interior.'}
def chimney():
 box('brick_stack',(0,0,.98),(.9,.9,1.96),'brick')
 box('brick_base',(0,0,.115),(1.20,1.20,.23),'brick')
 # A low tapered flashing shoulder replaces tiny brick geometry.
 slopebox('base_flashing',1.24,1.24,0,.33,.33,.12,'slate')
 box('cap_underlip',(0,0,1.87),(1.04,1.04,.16),'cream')
 # Five cap bars form two real square recessed flues.
 for y in [-.45,.45]:box('cap_edge',(0,y,2.03),(1.14,.24,.24),'cream')
 for x in [-.45,0,.45]:box('cap_web',(x,0,2.03),(.24,.66,.24),'cream')
 return {'nominal':[1.24,1.24,2.15],'budget':300,'notes':'Sepia brick chimney with cream stone cap and two geometric recessed flue openings. Brick courses and soot supplied in texture.'}
def vent():
 box('flashing_plate',(0,0,.04),(1.15,1.15,.08),'metal')
 lathe('tapered_boot',[(.52,.06),(.34,.35),(.34,.42)],8,'metal')
 lathe('vent_shaft',[(.33,.35),(.33,1.25),(.37,1.25),(.37,1.33)],8,'metal')
 # Rain-gap dark shaft top and four cap stanchions.
 for i in range(4):
  a=math.pi/4+i*math.pi/2;x=.28*math.cos(a);y=.28*math.sin(a)
  beam('hood_stanchion',(x,y,1.28),(x,y,1.57),.065)
 lathe('mushroom_cap',[(.58,1.53),(.58,1.60),(.17,1.79),(.10,1.80)],8,'metal')
 return {'nominal':[1.16,1.16,1.8],'budget':300,'notes':'Eight-sided galvanized metal shaft, open rain gap, four cap struts, broad low mushroom hood.'}
def skylight():
 box('masonry_curb',(0,0,.20),(2.90,3.90,.40),'cream')
 box('slate_flashing',(0,0,.06),(3.15,4.15,.12),'slate')
 v=[(-1.5,-2,.40),(1.5,-2,.40),(0,-2,1.25),(-1.5,2,.40),(1.5,2,.40),(0,2,1.25)]
 mesh('opaque_glazing',v,[(0,1,2),(3,5,4),(0,3,4,1),(0,2,5,3),(2,1,4,5)],'glass')
 for x in [-1.5,1.5]:beam('eave_frame',(x,-2.03,.43),(x,2.03,.43),.11)
 beam('ridge',(0,-2.06,1.25),(0,2.06,1.25),.12)
 for y in [-2,-1,0,1,2]:
  for x in [-1.5,1.5]:beam('sash_mullion',(x,y,.43),(0,y,1.25),.085)
 for y in [-2,2]:beam('gable_sill',(-1.5,y,.43),(1.5,y,.43),.10)
 return {'nominal':[3.15,4.15,1.31],'budget':1500,'notes':'Pitched 3x4m glazed lantern with masonry curb, slate flashing, ridge and transverse sash mullions. Opaque painted glass.'}
BUILDERS={'RF_WaterTank':water,'RF_Billboard8x4':lambda:billboard(8,4,3),'RF_Billboard12x5':lambda:billboard(12,5,4),'RF_StairBulkhead':bulkhead,'RF_ChimneyBrick':chimney,'RF_VentMetal':vent,'RF_Skylight':skylight}
def finish(aid,meta):
 # Manifold Boolean union removes buried contact faces. It is applied before UVs.
 base=PARTS[0];bpy.context.view_layer.objects.active=base
 for p in PARTS[1:]:
  mod=base.modifiers.new('Remove_buried_faces','BOOLEAN');mod.operation='UNION';mod.object=p;mod.solver='MANIFOLD'
  bpy.ops.object.modifier_apply(modifier=mod.name)
  bpy.data.objects.remove(p,do_unlink=True)
 base.name=aid;me=base.data
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
 bmesh.ops.dissolve_limit(bm,angle_limit=.001,verts=list(bm.verts),edges=list(bm.edges),delimit={'MATERIAL'})
 # Remove horizontal undersides against the roof; remaining exposed bottoms of raised structures stay.
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
 below=[f for f in bm.faces if all(v.co.z<.00002 for v in f.verts) and f.normal.z<-.8]
 bmesh.ops.delete(bm,geom=below,context='FACES');bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 lo=[min(v.co[i] for v in me.vertices) for i in range(3)];hi=[max(v.co[i] for v in me.vertices) for i in range(3)]
 centre=[(lo[i]+hi[i])/2 for i in range(2)]
 for v in me.vertices:v.co.x-=centre[0];v.co.y-=centre[1]
 if 'poster_face' in meta:
  meta['poster_face']['centre'][0]-=centre[0];meta['poster_face']['centre'][1]-=centre[1]
 meta['support_dimensions_m']={**meta.get('support_dimensions_m',{}),'ground_centre_shift_xy':centre}
 colors=me.color_attributes.new(name='PrepColor',type='FLOAT_COLOR',domain='CORNER')
 for p in me.polygons:
  mat=me.materials[p.material_index];col=mat.diffuse_color
  for li in p.loop_indices:colors.data[li].color=col
 mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True
 nt=mat.node_tree;bs=nt.nodes.get('Principled BSDF');bs.inputs['Roughness'].default_value=.85
 col=nt.nodes.new('ShaderNodeVertexColor');col.layer_name='PrepColor';nt.links.new(col.outputs['Color'],bs.inputs['Base Color'])
 me.materials.clear();me.materials.append(mat)
 for p in me.polygons:p.material_index=0;p.use_smooth=False
 bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.008,margin_method='FRACTION');bpy.ops.object.mode_set(mode='OBJECT')
 bm=bmesh.new();bm.from_mesh(me)
 audit={'id':aid,'tris':len(me.polygons),'budget':meta.pop('budget'),'material_slots':len(me.materials),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'bounds':[[round(fn(v.co[i] for v in me.vertices),6) for i in range(3)] for fn in [min,max]],'scale':list(base.scale),'rotation':list(base.rotation_euler),'uv_layers':len(me.uv_layers),'uv_padding_fraction':.008,'origin':'ground-centre'}
 if audit['branch_edges']:print('BRANCHES',[[list(v.co) for v in e.verts] for e in bm.edges if len(e.link_faces)>2])
 bm.free();assert audit['tris']<=audit['budget'],audit;assert audit['zero_area_faces']==audit['branch_edges']==audit['loose_vertices']==0,audit
 dest=W/aid;dest.mkdir(exist_ok=True)
 bpy.ops.export_scene.gltf(filepath=str(dest/f'{aid}_input.glb'),export_format='GLB',use_selection=True,export_yup=True)
 # Roundtrip the actual input and inspect geometry/bounds. glTF axes are correctly converted by importer.
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(dest/f'{aid}_input.glb'));checks=[o for o in set(bpy.data.objects)-old if o.type=='MESH'];assert len(checks)==1
 q=checks[0];vv=[q.matrix_world@v.co for v in q.data.vertices]
 audit['export_bounds']=[[round(fn(v[i] for v in vv),6) for i in range(3)] for fn in [min,max]];audit['export_tris']=sum(len(p.vertices)-2 for p in q.data.polygons);audit['export_material_slots']=len(q.material_slots)
 for o in set(bpy.data.objects)-old:bpy.data.objects.remove(o,do_unlink=True)
 bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
 bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/roof_clutter_{aid}.blend'))
 record={'id':aid,'phase':2,'kind':'prop','family':None,'model':f'work/roof_clutter/{aid}/{aid}_input.glb','reference':f'concept/{aid}_34.png','source':f'src/roof_clutter_{aid}.blend','material':'Shaded','resolution':'Basic','status':'ready_for_root_owned_texture_only','texture_notes':'Use hand-painted albedo, warm top light, cool corner AO. No background, people or lettering. Bake returned colour back onto this original UV mesh; then repack into shared family-neutral atlas. PrepColor is material guidance only. '+meta['notes'],'metadata':meta,'audit':audit}
 (dest/'preparation.json').write_text(json.dumps(record,indent=2)+'\n')
 print('ROOF_READY '+aid+' '+str(audit['tris'])+' triangles',flush=True)
 return record
targets=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else list(BUILDERS)
allrecords=[]
for aid in targets:
 bpy.ops.wm.read_factory_settings(use_empty=True);PARTS.clear()
 # Factory reset invalidates material handles, so remake the compact role palette.
 ROLE_MATS.clear()
 for role,color in PALETTE.items():
  m=bpy.data.materials.new('PrepRole_'+role);m.diffuse_color=color;ROLE_MATS[role]=m
 bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
 meta=BUILDERS[aid]();allrecords.append(finish(aid,meta))
records=[]
for aid in BUILDERS:
 p=W/aid/'preparation.json'
 if p.exists():records.append(json.loads(p.read_text()))
(R/'src/roof_clutter_texture_queue.json').write_text(json.dumps({'version':1,'atlas_target':'ATL_NEUTRAL','assets':records},indent=2)+'\n')
