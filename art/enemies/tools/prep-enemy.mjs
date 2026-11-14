// Simplify a Rodin GLB to a triangle budget and shrink its textures, ready for rigging in Blender.
// Usage: node prep-enemy.mjs <in.glb> <out.glb> <budget>
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { dedup, prune, weld, textureCompress, compactPrimitive, join, flatten } from '@gltf-transform/functions';
import { MeshoptSimplifier } from 'meshoptimizer';
import sharp from 'sharp';
const [inp, out, budgetArg] = process.argv.slice(2);
const budget = Number(budgetArg);
await MeshoptSimplifier.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(inp);
await doc.transform(dedup(), flatten(), join(), weld());
let before = 0, after = 0;
for (const mesh of doc.getRoot().listMeshes()) for (const prim of mesh.listPrimitives()) {
  const P = prim.getAttribute('POSITION').getArray(), N = prim.getAttribute('NORMAL')?.getArray(), U = prim.getAttribute('TEXCOORD_0')?.getArray();
  const n = P.length / 3, attrs = new Float32Array(n * 5);
  for (let i = 0; i < n; i++) {
    if (N) attrs.set([N[i * 3], N[i * 3 + 1], N[i * 3 + 2]], i * 5);
    if (U) attrs.set([U[i * 2], U[i * 2 + 1]], i * 5 + 3);
  }
  const idx = new Uint32Array(prim.getIndices().getArray());
  before += idx.length / 3;
  if (idx.length / 3 > budget) {
    const [o] = MeshoptSimplifier.simplifyWithAttributes(idx, P, 3, attrs, 5, [0.3, 0.3, 0.3, 1.5, 1.5], null, budget * 3, 1, ['Permissive']);
    prim.getIndices().setArray(o);
    compactPrimitive(prim);
  }
  after += prim.getIndices().getCount() / 3;
}
for (const t of doc.getRoot().listTextures()) t.setURI('');
await doc.transform(
  textureCompress({ encoder: sharp, targetFormat: 'png', resize: [1024, 1024] }),
  prune(),
);
await io.write(out, doc);
console.log(`${inp.split(/[\/]/).at(-2)}: ${before} -> ${after} tris`);
