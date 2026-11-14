"""Texture-only representatives for atlas Option B. UV roles never overlap.
Extra tessellation is confined to texture proxies, retaining the authored final meshes.
"""
import bpy,bmesh,math,json,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
FAM=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'BRK'
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_authored.blend'))
suffixes=['Pilaster1_G','Parapet4','CrestA','CrestB','Awning6']
if FAM!='BRK':suffixes=['G_Bay6A','U_Bay3A']+suffixes
masters=[FAM+'_'+s for s in suffixes]
records=[]
for aid in masters:
 original=bpy.data.objects[aid];o=original.copy();o.data=original.data.copy();bpy.context.collection.objects.link(o);o.name='TextureProxy_'+aid
 bm=bmesh.new();bm.from_mesh(o.data);cuts=max(1,min(7,int(math.sqrt(256/max(1,len(bm.faces))))-1))
 bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=cuts,use_grid_fill=True);bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.gltf(filepath=str(R/'work'/f'{aid}_input.glb'),export_format='GLB',use_selection=True)
 records.append({'id':aid,'final_tris':len(original.data.polygons),'proxy_tris':len(o.data.polygons),'reason':'Texture proxy subdivision preserves silhouette and UV interpolation; final mesh stays authored and minimal.'})
 bpy.data.objects.remove(o,do_unlink=True)
(R/f'src/{FAM}_texture_masters.json').write_text(json.dumps(records,indent=2)+'\n')
print('Prepared',len(masters),'representative texture inputs')
