// Upload a packaged UE Wasm stage folder to R2 under <prefix>/ and point the client Worker's DEPLOY var at it.
//   node deploy-stage.mjs <stage dir (.../Wasm)> <prefix>
// Uses `wrangler r2 object put --remote` per file (no S3 keys needed), 6 uploads in parallel. Skips symbols.
import { readdirSync, statSync, readFileSync, writeFileSync } from 'node:fs';
import { join, relative, resolve as absPath, sep } from 'node:path';
import { execFile, execFileSync } from 'node:child_process';

const [stage, prefix] = process.argv.slice(2);
if (!stage || !prefix) { console.error('usage: node deploy-stage.mjs <stage dir> <prefix>'); process.exit(2); }
const BUCKET = 'hollywoodland-stage';
const SKIP = [/\.js\.symbols$/, /\.pdb$/, /^Manifest_.*\.txt$/, /\.bat$/, /\.sh$/, /^build-stamp\.json$/];

function walk(dir) {
  return readdirSync(dir).flatMap((n) => {
    const p = join(dir, n);
    return statSync(p).isDirectory() ? walk(p) : [p];
  });
}
const files = walk(stage).filter((p) => !SKIP.some((r) => r.test(p.split(/[\\/]/).pop())));
const wrangler = process.platform === 'win32' ? 'npx.cmd' : 'npx';
let total = 0, done = 0;
function put(f) {
  const key = `${prefix}/${relative(stage, f).split(sep).join('/')}`;
  const size = statSync(f).size;
  total += size;
  return new Promise((resolve, reject) => {
    const attempt = (n) => execFile(wrangler, ['wrangler', 'r2', 'object', 'put', `${BUCKET}/${key}`, '--file', `"${absPath(f)}"`, '--remote'],
      { shell: true, cwd: new URL('./client/', import.meta.url) }, (err, _o, stderr) => {
        if (err && n < 3) return attempt(n + 1);
        if (err) return reject(new Error(`${key}: ${stderr}`));
        console.log(`[${++done}/${files.length}] ${key} (${(size / 1048576).toFixed(2)} MiB)`);
        resolve();
      });
    attempt(1);
  });
}
const queue = [...files];
await Promise.all(Array.from({ length: 6 }, async () => { while (queue.length) await put(queue.shift()); }));
console.log(`uploaded ${files.length} files, ${(total / 1048576).toFixed(1)} MiB`);
// switch the live prefix
const cfgPath = new URL('./client/wrangler.jsonc', import.meta.url);
writeFileSync(cfgPath, readFileSync(cfgPath, 'utf8').replace(/"DEPLOY": "[^"]*"/, `"DEPLOY": "${prefix}"`));
execFileSync(wrangler, ['wrangler', 'deploy'], { cwd: new URL('./client/', import.meta.url), stdio: 'inherit', shell: true });
