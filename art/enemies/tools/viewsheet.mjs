// Stitch <name>__top.png / __side.png pairs into one labelled sheet. node viewsheet.mjs <viewsDir> <out.png> [filter]
import sharp from 'sharp'; import fs from 'node:fs'; import path from 'node:path';
const [dir, out, filt] = process.argv.slice(2), T = 256;
const names = [...new Set(fs.readdirSync(dir).filter((f) => f.endsWith('__top.png') && (!filt || new RegExp(filt).test(f))).map((f) => f.replace('__top.png', '')))].sort();
const cols = 4, rows = Math.ceil(names.length / cols), tiles = [];
for (const [i, n] of names.entries()) {
  const x = (i % cols) * T * 2, y = Math.floor(i / cols) * T;
  for (const [k, v] of ['top', 'side'].entries()) {
    const f = path.join(dir, `${n}__${v}.png`);
    if (fs.existsSync(f)) tiles.push({ input: await sharp(f).resize(T, T).png().toBuffer(), left: x + k * T, top: y });
  }
  tiles.push({ input: Buffer.from(`<svg width="${T * 2}" height="20"><rect width="100%" height="100%" fill="#000" opacity=".7"/><text x="4" y="15" font-size="14" font-family="Arial" fill="#fff">${n}  (top | side)  +X = right</text></svg>`), left: x, top: y });
}
await sharp({ create: { width: cols * T * 2, height: rows * T, channels: 3, background: '#444' } }).composite(tiles).png().toFile(out);
console.log(names.length, 'models ->', out);
