// prints per GLB: triangles, vertices, primitives, textures (w x h), file size
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';
const dir = process.argv[2];
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
let T = 0;
for (const f of readdirSync(dir).filter((f) => f.endsWith('.glb')).sort()) {
  const doc = await io.read(join(dir, f));
  let tris = 0, verts = 0, prims = 0;
  for (const m of doc.getRoot().listMeshes()) for (const p of m.listPrimitives()) {
    prims++; verts += p.getAttribute('POSITION').getCount();
    tris += (p.getIndices() ? p.getIndices().getCount() : p.getAttribute('POSITION').getCount()) / 3;
  }
  T += tris;
  const tex = doc.getRoot().listTextures().map((t) => (t.getSize() || []).join('x')).join(' ');
  console.log(f.padEnd(44), String(tris).padStart(7), 'tris', String(verts).padStart(7), 'v', prims, 'prim', (statSync(join(dir, f)).size / 1e6).toFixed(2), 'MB', tex);
}
console.log('total tris', T);
