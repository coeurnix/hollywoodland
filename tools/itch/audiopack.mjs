// Bundle sfx/*.mp3 (SFX + 1,300+ voice lines) into audio/*.bin so the itch.io package stays under its 1,000-file limit.
//   import { packAudio } from './audiopack.mjs'; packAudio(ROOT, outDir)  -> { files, bytes }
// audio/index.json maps "sfx/<file>" to [bundle path, offset, length]; HLBridge's sfx loader fetches a bundle once, the first
// time one of its sounds plays, and slices the sound out. Files are taken in name order and a bundle closes at about 1 MB on a
// sound-group boundary (vo_npc_guard_hello_0..11 stay together), so a speaker's lines usually come in one fetch.
import { readdirSync, readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';

const SOFT = 1_000_000, HARD = 2_000_000;
const group = (n) => n.replace(/_?\d*\.mp3$/, '');

export function packAudio(root, out) {
  const src = join(root, 'sfx');
  const names = readdirSync(src).filter((n) => n.endsWith('.mp3')).sort();
  mkdirSync(join(out, 'audio'), { recursive: true });
  const index = {};
  let parts = [], size = 0, count = 0, bytes = 0, prev = null;
  const flush = () => {
    if (!parts.length) return;
    const path = `audio/${String(count++).padStart(3, '0')}.bin`;
    let off = 0;
    for (const [n, buf] of parts) { index[`sfx/${n}`] = [path, off, buf.length]; off += buf.length; }
    writeFileSync(join(out, path), Buffer.concat(parts.map((p) => p[1])));
    bytes += off; parts = []; size = 0;
  };
  for (const n of names) {
    const buf = readFileSync(join(src, n));
    if (size >= HARD || (size >= SOFT && group(n) !== prev)) flush();
    parts.push([n, buf]); size += buf.length; prev = group(n);
  }
  flush();
  const ix = JSON.stringify(index);
  writeFileSync(join(out, 'audio', 'index.json'), ix);
  return { sounds: names.length, files: count + 1, bytes: bytes + ix.length };
}
