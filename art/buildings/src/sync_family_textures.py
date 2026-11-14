"""Refresh packed source bytes and embedded GLB textures from the final atlas PNG."""
import bpy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];FAM=sys.argv[sys.argv.index('--')+1];stage=R/f'work/{FAM}_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
im=next(n.image for n in bpy.data.materials['M_ATL_'+FAM].node_tree.nodes if n.type=='TEX_IMAGE')
if im.packed_file:im.unpack(method='REMOVE')
im.filepath=str(stage/f'textures/T_ATL_{FAM}.png');im.reload();im.pack()
entries=json.loads((R/f'work/{FAM}_kit_assets.json').read_text());audits={}
budgets={a['id']:a['budget'] for a in json.loads((R/'work/neutral_assets.json').read_text())} if FAM=='NEUTRAL' else {}
for a in entries:
 entry,audit=export_asset(bpy.data.objects[a['id']],a,budgets.get(a['id'],600 if a['kind']=='module' else 60),output_root=stage);a.update(entry);audits[a['id']]=audit
(R/f'work/{FAM}_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/f'src/{FAM}_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
