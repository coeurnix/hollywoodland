"""Material-only GLB interoperability proxy; original positions/normals/UVs unchanged."""
import json,struct
from pathlib import Path
def prepare(source,target):
 source,target=Path(source),Path(target);blob=source.read_bytes();length,kind=struct.unpack_from('<II',blob,12);assert kind==0x4E4F534A
 data=json.loads(blob[20:20+length]);binary=blob[20+length:]
 for mesh in data.get('meshes',[]):
  for primitive in mesh['primitives']:
   primitive['attributes'].pop('COLOR_0',None);primitive['material']=0
 data['materials']=[{'name':'TextureOnlyNeutral','doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':[.65,.55,.4,1],'metallicFactor':0,'roughnessFactor':1}}]
 for field in ['textures','images','samplers']:data.pop(field,None)
 encoded=json.dumps(data,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
 target.write_bytes(struct.pack('<III',0x46546C67,2,12+8+len(encoded)+len(binary))+struct.pack('<II',len(encoded),0x4E4F534A)+encoded+binary)
 return target
