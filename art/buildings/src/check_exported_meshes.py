"""Independent round-trip certification of the manifest's actual binary FBXs."""
import bpy,bmesh,json,struct,sys,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];manifest=json.loads((R/'manifest.json').read_text());checks={};failures=[]
selected=set(sys.argv[sys.argv.index('--')+1:]) if '--' in sys.argv else set()
if selected:
 previous=json.loads((R/'src/exported_mesh_selfcheck.json').read_text());checks=previous['checks']
for a in manifest['assets']:
 if selected and a['id'] not in selected:continue
 bpy.ops.wm.read_factory_settings(use_empty=True)
 path=R/a['file'];header=path.read_bytes()[:27]
 assert header[:23]==b'Kaydara FBX Binary  \x00\x1a\x00' and struct.unpack('<I',header[23:27])[0]==7400,a['id']
 bpy.ops.import_scene.fbx(filepath=str(path));objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];assert len(objects)==1,(a['id'],'mesh count')
 o=objects[0];bm=bmesh.new();bm.from_mesh(o.data);vv=[o.matrix_world@v.co for v in o.data.vertices]
 bounds=[[round(fn(v[i] for v in vv),6) for i in range(3)] for fn in [min,max]]
 audit={'fbx_version':7400,'bounds':bounds,'tris':sum(len(p.vertices)-2 for p in o.data.polygons),'triangulated':all(len(p.vertices)==3 for p in o.data.polygons),'material_slots':len(o.material_slots),'material':o.material_slots[0].name,'scale':list(o.scale),'rotation':list(o.rotation_euler),'location':list(o.location),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'inconsistent_shared_edges':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'boundary_edges':sum(e.is_boundary for e in bm.edges),'uv_layers':len(o.data.uv_layers)}
 bm.free();issues=[]
 if audit['tris']!=a['tris'] or bounds!=a['bounds']:issues.append('manifest measurement mismatch')
 if not audit['triangulated'] or audit['material_slots']!=1:issues.append('triangles/material slots')
 if audit['material']!=a['material']:issues.append('material name')
 if any(audit[k] for k in ['zero_area_faces','branch_edges','inconsistent_shared_edges','loose_vertices']):issues.append('topology')
 if max(abs(v-1) for v in o.scale)>1e-5 or max(abs(v) for v in (*o.rotation_euler,*o.location))>1e-5:issues.append('unapplied transform/origin')
 if not audit['uv_layers']:issues.append('missing UVs')
 budget=600 if a['kind']=='module' else (60 if a['kind'] in ['wall','roof'] else (50 if a['kind']=='sign_card' else (10000 if a['kind']=='landmark' else 1500)))
 if a['id'] in ['RF_ChimneyBrick','RF_VentMetal','DR_BoardedWindow15','DR_BoardedWindow23']:budget=300
 if a['id'] in ['DR_SootA','DR_SootB','DR_Crack','DR_MayorNotice','DR_BrokenGlass']:budget=50
 if audit['tris']>budget:issues.append('triangle budget')
 audit['budget']=budget;audit['issues']=issues;audit['file_sha256']=hashlib.sha256(path.read_bytes()).hexdigest();checks[a['id']]=audit
checks={a['id']:checks[a['id']] for a in manifest['assets']}
failures=[{'id':aid,'issues':c['issues']} for aid,c in checks.items() if c['issues']]
report={'checked_assets':len(checks),'all_passed':not failures,'checks':checks,'failures':failures,'boundary_policy':'Intentional open wall-facing and ground-facing boundaries are documented in README.'}
(R/'src/exported_mesh_selfcheck.json').write_text(json.dumps(report,indent=2)+'\n')
print('EXPORTED_MESH_SELFCHECK',len(checks),'assets; failures:',failures)
assert not failures,'See src/exported_mesh_selfcheck.json'
