// Prop placement helpers for zone.mjs. The table (Content/Data/props.json) comes from tools/propprep.py + Scripts/ImportProps.ps1:
// per prop id the static mesh path and its imported bounds in metres (front = +X, Z up; pivot floor / wall (back at x 0) / ceiling).
// A placement is { m: mesh path, x, y, z, yaw (degrees, 0 = front along +x, 90 = front along +y), s: [sx, sy, sz] }.
import { readFileSync } from 'node:fs';

export const PROPS = JSON.parse(readFileSync(new URL('../../hollywoodland-ue/Content/Data/props.json', import.meta.url), 'utf8')).props;

const r2 = (v) => Math.round(v * 100) / 100;
const vec = (s) => (Array.isArray(s) ? s : [s, s, s]);

export function placement(id, x, y, yaw = 0, s = 1, z = 0) {
  const p = PROPS[id];
  if (!p) throw new Error(`unknown prop ${id}`);
  return { m: p.mesh, x: r2(x), y: r2(y), z: r2(z), yaw: Math.round(yaw), s: vec(s).map(r2) };
}

// footprint (x0, y0, x1, y1) of a prop placed at (x, y) with an axis-aligned yaw
export function footprint(id, x, y, yaw = 0, s = 1) {
  const p = PROPS[id], [sx, sy] = vec(s);
  const a = ((Math.round(yaw / 90) % 4) + 4) % 4;
  const lx = [p.min[0] * sx, p.max[0] * sx], ly = [p.min[1] * sy, p.max[1] * sy];
  // rotate the local rectangle by a quarter turns: (u, v) -> a=1: (-v, u), a=2: (-u, -v), a=3: (v, -u)
  const xs = a === 0 ? lx : a === 1 ? ly.map((v) => -v) : a === 2 ? lx.map((u) => -u) : ly;
  const ys = a === 0 ? ly : a === 1 ? lx : a === 2 ? ly.map((v) => -v) : lx.map((u) => -u);
  return [x + Math.min(...xs), y + Math.min(...ys), x + Math.max(...xs), y + Math.max(...ys)];
}

// a prop stretched to fill an axis-aligned box (the box keeps its collision; the renderer draws the prop instead of a cube)
export function fitBox(id, x0, y0, x1, y1, h, yaw) {
  const p = PROPS[id], w = x1 - x0, d = y1 - y0;
  if (yaw === undefined) yaw = w >= d ? 0 : 90;
  const along = Math.round(yaw / 90) % 2 === 0;
  const len = p.max[0] - p.min[0], wid = p.max[1] - p.min[1], ht = p.max[2] - p.min[2];
  const s = [(along ? w : d) / len, (along ? d : w) / wid, h / ht];
  // centre of the stretched bounds lands on the box centre
  const cx = -(p.min[0] + p.max[0]) / 2 * s[0], cy = -(p.min[1] + p.max[1]) / 2 * s[1];
  const rad = (yaw * Math.PI) / 180;
  return placement(id, (x0 + x1) / 2 + cx * Math.cos(rad) - cy * Math.sin(rad), (y0 + y1) / 2 + cx * Math.sin(rad) + cy * Math.cos(rad), yaw, s);
}

// a seeded picker so the dressing is stable between data builds
export function rng(seed) {
  let t = seed >>> 0;
  return () => { t = (t + 0x6d2b79f5) >>> 0; let r = Math.imul(t ^ (t >>> 15), 1 | t); r ^= r + Math.imul(r ^ (r >>> 7), 61 | r); return ((r ^ (r >>> 14)) >>> 0) / 4294967296; };
}
