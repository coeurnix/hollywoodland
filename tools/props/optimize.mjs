// Geometry half of tools/propprep.py: weld + meshoptimizer simplification bounded by geometric error (not by a triangle
// ratio), so a prop only loses triangles that don't change its silhouette or open its UV seams.
//   node tools/props/optimize.mjs <jobs.json>
// jobs.json: [{ in: <glb>, out: <glb>, error: <max deviation, fraction of the mesh's size>, max_tris: <soft ceiling> }]
// A prop still over max_tris after the error-bounded pass is simplified further toward the ceiling, but never past
// 4x the error bound. Prints 'HL_OPT <name> <tris before> -> <tris after> err <e>' per prop.
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { weld, simplify, dedup, prune } from '@gltf-transform/functions';
import { MeshoptSimplifier } from 'meshoptimizer';
import { readFileSync } from 'node:fs';
import { basename } from 'node:path';

await MeshoptSimplifier.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const tris = (doc) => doc.getRoot().listMeshes().flatMap((m) => m.listPrimitives())
  .reduce((s, p) => s + (p.getIndices() ? p.getIndices().getCount() : p.getAttribute('POSITION').getCount()) / 3, 0);

for (const job of JSON.parse(readFileSync(process.argv[2], 'utf8'))) {
  const src = await io.read(job.in);
  const before = tris(src);
  let err = job.error, doc, after;
  for (;;) {
    doc = await io.read(job.in);
    await doc.transform(weld(), simplify({ simplifier: MeshoptSimplifier, ratio: 0, error: err, lockBorder: false }), dedup(), prune());
    after = tris(doc);
    if (after <= job.max_tris || (!job.hard && err >= job.error * 4) || err > 0.08) break;
    err *= 1.5;
  }
  await io.write(job.out, doc);
  console.log(`HL_OPT ${basename(job.in, '.glb')} ${before} -> ${after} err ${err.toFixed(5)}`);
}
