// Upload <repo>/music/*.mp3 to R2 under music/ (shared by every stage; the client Worker serves /music/<file>).
// With --sfx, uploads <repo>/sfx/*.mp3 under sfx/ instead (tools/gen-sfx.mjs).
//   node deploy-music.mjs [--sfx] [--force]   (skips files already in R2 with the same size unless --force)
import { readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { execFile } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const BUCKET = 'hollywoodland-stage';
const KIND = process.argv.includes('--sfx') ? 'sfx' : 'music';
const dir = new URL(`../${KIND}/`, import.meta.url);
const force = process.argv.includes('--force');
const wrangler = process.platform === 'win32' ? 'npx.cmd' : 'npx';
const files = readdirSync(dir).filter((n) => n.endsWith('.mp3'));
const base = `https://hollywoodland.coeurnix.workers.dev/${KIND}/`;

async function remoteSize(name) {
  try { const r = await fetch(base + encodeURIComponent(name), { method: 'HEAD' }); return r.ok ? Number(r.headers.get('content-length')) : -1; }
  catch { return -1; }
}
function put(name) {
  const path = join(fileURLToPath(dir), name);
  return new Promise((resolve, reject) => {
    const attempt = (n) => execFile(wrangler, ['wrangler', 'r2', 'object', 'put', `${BUCKET}/${KIND}/${name}`, '--file', `"${path}"`,
      '--content-type', 'audio/mpeg', '--remote'], { shell: true, cwd: new URL('./client/', import.meta.url) }, (err, _o, stderr) => {
      if (err && n < 3) return attempt(n + 1);
      if (err) return reject(new Error(`${name}: ${stderr}`));
      console.log(`uploaded ${KIND}/${name}`);
      resolve();
    });
    attempt(1);
  });
}
const queue = [];
for (const n of files) {
  const size = statSync(new URL(n, dir)).size;
  if (!force && (await remoteSize(n)) === size) { console.log(`same size, skipped: ${n}`); continue; }
  queue.push(n);
}
await Promise.all(Array.from({ length: 6 }, async () => { while (queue.length) await put(queue.shift()); }));
console.log(`${KIND} upload done`);
