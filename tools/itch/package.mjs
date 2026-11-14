// Package the web client for itch.io (https://coeurnix.itch.io/hollywoodland). The server stays on Cloudflare.
//   node tools/itch/package.mjs [stage dir]        (default hollywoodland-ue/web-dist-Shipping/Wasm, from Scripts/BuildWeb.ps1)
//   butler push hollywoodland-ue/web-dist-Shipping/itch coeurnix/hollywoodland:html5 --userversion <v>
//
// itch serves the files as uploaded: no .br siblings (its CDN does no Brotli negotiation), no _headers. The game's service
// worker (sw.js) fetches each data file whole once and answers the engine's range reads from Cache Storage, so the CDN needs
// no Range support. Cross-origin isolation (SharedArrayBuffer for the game's threads) comes from the itch page setting
// Edit game > Embed options > "SharedArrayBuffer support". Audio ships in the package: music/*.mp3 as files (streamed by
// <audio>), sfx/*.mp3 (1,800+ files) bundled into audio/*.bin by audiopack.mjs to stay under the file limit.
// itch limits checked here: the whole package under 500 MB, under 1,000 files, no single file over 200 MB (the pak's IoStore
// container is split into 128 MB partitions by MaxIoStorePartitionSizeMB in Config/DefaultGame.ini), index.html at the top.
import { readdirSync, statSync, mkdirSync, rmSync, copyFileSync, existsSync } from 'node:fs';
import { join, relative, dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { packAudio } from './audiopack.mjs';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const stage = resolve(process.argv[2] || join(ROOT, 'hollywoodland-ue', 'web-dist-Shipping', 'Wasm'));
const out = join(dirname(stage), 'itch');
const SKIP = [/\.br$/, /^_headers$/, /\.js\.symbols$/, /\.pdb$/, /^Manifest_.*\.txt$/, /\.bat$/, /\.sh$/, /^build-stamp\.json$/];
const LIMIT_BYTES = 500e6, LIMIT_FILES = 1000, LIMIT_FILE = 200 * 1024 * 1024;

function walk(dir) {
  return readdirSync(dir).flatMap((n) => {
    const p = join(dir, n);
    return statSync(p).isDirectory() ? walk(p) : SKIP.some((r) => r.test(n)) ? [] : [p];
  });
}

if (!existsSync(join(stage, 'index.html'))) { console.error(`no index.html in ${stage}: run Scripts/BuildWeb.ps1 first`); process.exit(2); }
rmSync(out, { recursive: true, force: true });
const copies = walk(stage).map((f) => [f, join(out, relative(stage, f))]);
for (const n of readdirSync(join(ROOT, 'music')).filter((n) => n.endsWith('.mp3'))) copies.push([join(ROOT, 'music', n), join(out, 'music', n)]);
for (const [f, dst] of copies) {
  mkdirSync(dirname(dst), { recursive: true });
  copyFileSync(f, dst);
}
const audio = packAudio(ROOT, out);
const files = walk(out);
const total = files.reduce((s, f) => s + statSync(f).size, 0);
const mb = (b) => (b / 1e6).toFixed(1);
console.log(`itch package: ${out}`);
console.log(`  audio: ${audio.sounds} sounds in ${audio.files} bundle files (${mb(audio.bytes)} MB), music ${mb(walk(join(out, 'music')).reduce((s, f) => s + statSync(f).size, 0))} MB`);
console.log(`  ${files.length} files, ${mb(total)} MB (limits: ${LIMIT_FILES} files, ${mb(LIMIT_BYTES)} MB)`);
for (const f of files.sort((a, b) => statSync(b).size - statSync(a).size).slice(0, 4)) console.log(`  ${mb(statSync(f).size).padStart(7)} MB  ${relative(out, f)}`);
const big = files.filter((f) => statSync(f).size > LIMIT_FILE);
for (const f of big) console.error(`  over the 200 MB per-file limit: ${relative(out, f)}`);
if (total >= LIMIT_BYTES || files.length >= LIMIT_FILES || big.length) { console.error('OVER THE ITCH.IO LIMIT'); process.exit(1); }
console.log(`next: butler push "${out}" coeurnix/hollywoodland:html5 --userversion <version>`);
