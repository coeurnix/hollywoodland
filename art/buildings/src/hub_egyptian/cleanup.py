"""Targeted final-mesh cleanup; frozen Rodin GLBs remain unchanged.

Call apply(obj) on the imported authored mesh before bake and final asset_io
export. Removes only the buried portico-lintel end caps on Egyptian main/wing.
Welding retains per-loop UVs, so shared positions never collapse UV seams.
Other delegated meshes receive the same strict weld/triangulate/audit only.
"""
import bmesh

def _counts(bm):
 return {'vertices':len(bm.verts),'tris':sum(len(f.verts)-2 for f in bm.faces),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'inconsistent_shared_edges':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges)}

def _uv_signature(face,uvlayer):
 # Rotation or reversed loop order does not change a face's semantic UV mapping.
 return tuple(sorted((tuple(round(c,7) for c in l.vert.co),tuple(round(c,7) for c in l[uvlayer].uv)) for l in face.loops))

def apply(obj):
 aid=obj.name.split('.')[0];bm=bmesh.new();bm.from_mesh(obj.data)
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 bmesh.ops.triangulate(bm,faces=list(bm.faces))
 before=_counts(bm);layers=list(bm.loops.layers.uv.values())
 # These caps lie entirely inside the flanking solid wall and must be absent.
 if aid=='LM_Egyptian':spec=(18.,-25.,-23.,9.8,11.7)
 elif aid=='LM_Egyptian_Wing':spec=(12.,-14.,-12.5,4.8,5.5)
 else:spec=None
 remove=[]
 if spec:
  half,y0,y1,z0,z1=spec
  for f in bm.faces:
   if any(all(abs(v.co.x-side*half)<2e-5 for v in f.verts) for side in [-1,1]) and all(y0-2e-5<=v.co.y<=y1+2e-5 and z0-2e-5<=v.co.z<=z1+2e-5 for v in f.verts):remove.append(f)
  assert len(remove) in [0,4],(aid,'Unexpected buried-cap triangle count',len(remove))
 removed=set(remove);uv_before=[sorted(_uv_signature(f,layer) for f in bm.faces if f not in removed) for layer in layers]
 if remove:bmesh.ops.delete(bm,geom=remove,context='FACES_ONLY')
 # FACES_ONLY preserves boundary vertices; remove no-longer-used edges if any.
 wire=[e for e in bm.edges if not e.link_faces]
 if wire:bmesh.ops.delete(bm,geom=wire,context='EDGES')
 loose=[v for v in bm.verts if not v.link_faces]
 if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
 uv_after=[sorted(_uv_signature(f,layer) for f in bm.faces) for layer in layers]
 assert uv_before==uv_after,(aid,'UV mapping changed on surviving face')
 after=_counts(bm)
 assert after['zero_area_faces']==after['branch_edges']==after['loose_vertices']==after['inconsistent_shared_edges']==0,(aid,after)
 bm.to_mesh(obj.data);bm.free();obj.data.update()
 return {'id':aid,'removed_buried_triangles':len(remove),'before':before,'after':after,'surviving_face_uvs_unchanged':True}
