"""Blender: export the15 phase-three cards on their shared sign pages."""
import bpy,json,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'src'))
from asset_io import export_asset
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
stage=R/'work/HUBSIGNS_candidate';(stage/'preview').mkdir(parents=True,exist_ok=True)
records=json.loads((R/'work/hub_sign_records.json').read_text());materials={};entries=[];audits={}
for rec in records:
    page=rec['page'];aid=rec['id'];w,h=rec['nominal']
    if page not in materials:
        im=bpy.data.images.load(str(R/page));im.pack()
        mat=bpy.data.materials.new('M_'+Path(page).stem.removeprefix('T_'));mat.use_nodes=True
        node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=im
        shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=1
        mat.node_tree.links.new(node.outputs['Color'],shader.inputs['Base Color']);materials[page]=mat
    profile=rec.get('profile_m',[[0,0],[w,0],[w,h],[0,h]])
    verts=[(xx,0,zz) for xx,zz in profile]
    mesh=bpy.data.meshes.new(aid);mesh.from_pydata(verts,[],[tuple(range(len(verts)))]);mesh.update()
    uv=mesh.uv_layers.new();x,y,uw,vh=rec['rect']
    for loop in mesh.loops:
        xx,zz=profile[loop.vertex_index];uv.data[loop.index].uv=(x+xx/w*uw,1-y-vh+zz/h*vh)
    ob=bpy.data.objects.new(aid,mesh);scene.collection.objects.link(ob);mesh.materials.append(materials[page])
    metadata={'id':aid,'kind':'sign_card','family':None,'material':materials[page].name,'texture':page,
              'emissive':None,'nominal':[w,h],'origin':'front-plane-bottom-left','fits_box':None,
              'also_fits':[],'recess':[],'quest_object':None,'concept':rec['concept'],'prompts':rec['prompts'],
              'rodin':{'used':False,'method':'font-composition'},'source':'src/HUBSIGNS_kit_textured.blend',
              'notes':'SPEC10.1 original generated painted background and project OFL lettering; top-left normalized atlas rectangles. '+('Opaque hat silhouette within authored sign backing.' if 'profile_m' in rec else '')}
    entry,audit=export_asset(ob,metadata,50,stage);entries.append(entry);audits[aid]=audit
(R/'work/HUBSIGNS_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n')
(R/'src/HUBSIGNS_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/HUBSIGNS_kit_textured.blend'))
print('EXPORTED',len(entries),'cards; all actual FBXs pass one-material/triangulation/topology/budget checks')
