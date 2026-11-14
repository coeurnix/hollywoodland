// Picture night flipbook: cuts a short excerpt of a public-domain film into one JPEG atlas the game steps through
// (AHLWorldActor::TickFilm draws it on Grauman's facade while the film event runs).
//   node tools/film/make-flipbook.mjs <video> <start seconds> [--fps 6] [--frames 64] [--cols 8] [--cell 256]
// Writes hollywoodland-ue/Content/Data/Film/film.jpg and film.json. Needs ffmpeg on PATH.
// Source used: Hell's Angels (1930, Howard Hughes; US public domain since 2026), the Wikimedia Commons 240p transcode of
// "File:Hell's Angels (1930).webm" (the Universal 4K restoration). The video itself is not kept in the repo.
import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync, statSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const OUT = join(ROOT, 'hollywoodland-ue', 'Content', 'Data', 'Film');
const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? Number(args[i + 1]) : d; };
const [src, start] = args;
if (!src || start === undefined) { console.error('usage: make-flipbook.mjs <video> <start seconds> [--fps 6] [--frames 64] [--cols 8] [--cell 256]'); process.exit(1); }
const fps = opt('--fps', 6), frames = opt('--frames', 64), cols = opt('--cols', 8), cw = opt('--cell', 256);
const rows = Math.ceil(frames / cols);
// cells keep the source's picture shape (even height); the 1930 print is close to 1.37:1 inside a 1.54 frame
const probe = execFileSync('ffprobe', ['-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', src]).toString().trim().split(',').map(Number);
const ch = Math.round(cw * probe[1] / probe[0] / 2) * 2;
mkdirSync(OUT, { recursive: true });
const jpg = join(OUT, 'film.jpg');
// aged-print look: slight warm tone, a touch of contrast; tile the frames into the atlas
const vf = `fps=${fps},scale=${cw}:${ch}:flags=lanczos,eq=contrast=1.08:saturation=0.5,colorbalance=rs=0.04:bs=-0.04,tile=${cols}x${rows}`;
execFileSync('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', '-ss', String(start), '-i', src, '-t', String(frames / fps + 0.5), '-vf', vf, '-frames:v', '1', '-q:v', '4', jpg]);
// the screen: centred over Grauman's marquee, facing the forecourt (metres; z is the bottom edge above the ground)
const w = 9.2, h = Math.round(w * ch / cw * 100) / 100;
writeFileSync(join(OUT, 'film.json'), JSON.stringify({ note: 'tools/film/make-flipbook.mjs: Hell\'s Angels (1930), public domain', file: 'film.jpg', cols, rows, frames, fps, start: Number(start), x: 93, y: 166.4, z: 6.0, w, h }, null, 1) + '\n');
console.log(`film.jpg ${cols * cw}x${rows * ch}, ${frames} frames at ${fps} fps, ${(statSync(jpg).size / 1024).toFixed(0)} KB`);
