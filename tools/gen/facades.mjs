// Storefront facades from the building kit (art/buildings -> tools/buildprep.py -> Scripts/ImportBuildings.ps1 -> buildings.json).
// A shop box stays the collision and the body; its street face gets a dressed front laid out from the family recipe:
//   corner | bay | pilaster | bay | ... | corner      ground row 4.5 m, upper rows 3.5 m, parapet on top (1.5 m, scaled down
//   on taller shops: 6 m -> 1.5, 9 m -> 1.0, 12 m -> 0.5, 15 m -> none, the README's proposed exception)
// Module frame after import: front +X, width along -Y from the left end (as seen from the street), Z up, front plane x = 0.
// Bays are stretched along the width (within ~25%) so pilasters fall on exact positions and the row fills the box.
import { readFileSync } from 'node:fs';
import { rng } from './props.mjs';

const TABLE = JSON.parse(readFileSync(new URL('../../hollywoodland-ue/Content/Data/buildings.json', import.meta.url), 'utf8'));
export const KIT = TABLE.props;
const R = TABLE.recipes;
const r2 = (v) => Math.round(v * 100) / 100;
const OUT = 0.02; // the facade sits just proud of the box face (no z-fighting with the cube)

function put(out, id, p, yaw, s = [1, 1, 1], z = 0) {
  const k = KIT[id];
  if (!k) throw new Error(`unknown kit piece ${id}`);
  out.push({ m: k.mesh, x: r2(p[0]), y: r2(p[1]), z: r2(z), yaw: Math.round(yaw), s: s.map(r2) });
}

const nearest = (w, opts) => opts.reduce((a, b) => (Math.abs(b.w - w) < Math.abs(a.w - w) ? b : a));
const width = (id) => KIT[id].nominal[0];

// The frame of a box's street face. north: the shop is north of the street, its front faces +y.
function frame(b, north) {
  return north
    ? { F: [0, 1], u: [1, 0], S: [b.x, b.y + b.d], yaw: 90, W: b.w }
    : { F: [0, -1], u: [-1, 0], S: [b.x + b.w, b.y], yaw: -90, W: b.w };
}
const at = (f, t, out = OUT) => [f.S[0] + f.u[0] * t + f.F[0] * out, f.S[1] + f.u[1] * t + f.F[1] * out];

// Lay a BRK front on box b; returns placements. seed varies variants, awnings, crests and damage per shop.
export function brkFront(b, north, seed, opts = {}) {
  const rec = R.BRK, out = [], rnd = rng(seed * 7919 + 17), f = frame(b, north);
  const W = f.W, h = b.h;
  const rows = Math.max(0, Math.min(3, Math.round((h - rec.ground_h - 0.5) / rec.upper_h)));
  const ph = Math.max(0, h - rec.ground_h - rows * rec.upper_h);
  const L = W - 2 * rec.corner_front_m;
  const n = Math.max(1, Math.round((L + 1) / 7));
  const bw = (L - (n - 1)) / n;
  const varG = rnd() < 0.5 ? 'A' : 'B', varU = rnd() < 0.5 ? 'A' : 'B';
  const ground = [4, 6, 8].map((w) => ({ w, id: `BRK_G_Bay${w}${varG}` }));
  const upper = [2, 3, 4].map((w) => ({ w, id: `BRK_U_Bay${w}${varU}` }));
  const awnings = rnd() < (opts.awning ?? 0.45);
  const damaged = rnd() < (opts.damaged ?? 0.15);
  const rowZ = (r) => rec.ground_h + (r - 1) * rec.upper_h; // r = 0 ground, 1.. upper rows

  for (let r = 0; r <= rows; r++) {
    const z = r === 0 ? 0 : rowZ(r);
    const sz = 1;
    // corners: the right one as modelled (return wraps the right side); the left one turned a quarter so its return is the front
    put(out, r ? rec.corner_upper : rec.corner, at(f, W - rec.corner_front_m), f.yaw, [1, 1, sz], z);
    const lc = [f.S[0] - f.F[0] * (rec.corner_return_m - OUT) - f.u[0] * OUT, f.S[1] - f.F[1] * (rec.corner_return_m - OUT) - f.u[1] * OUT];
    put(out, r ? rec.corner_upper : rec.corner, lc, f.yaw + 90, [1, 1, sz], z);
    for (let i = 0; i < n; i++) {
      const t0 = rec.corner_front_m + i * (bw + 1);
      if (r === 0) {
        let m = nearest(bw, ground);
        if (damaged && m.w === 6 && i === n - 1) m = { w: 6, id: 'BRK_Damaged_G_Bay6' };
        put(out, m.id, at(f, t0), f.yaw, [1, bw / m.w, 1], 0);
        if (awnings && !(damaged && i === n - 1)) {
          const a = bw <= 5 ? 'BRK_Awning4' : 'BRK_Awning6';
          put(out, a, at(f, t0), f.yaw, [1, bw / width(a), 1], 0);
        }
      } else {
        const k = Math.max(1, Math.round(bw / 3.5));
        const uw = bw / k;
        for (let j = 0; j < k; j++) {
          let m = nearest(uw, upper);
          if (damaged && r === rows && m.w === 3 && i === 0 && j === 0) m = { w: 3, id: 'BRK_Damaged_U_Bay3' };
          put(out, m.id, at(f, t0 + j * uw), f.yaw, [1, uw / m.w, 1], z);
        }
      }
      if (i < n - 1) put(out, r ? rec.pilaster_upper : rec.pilaster, at(f, t0 + bw), f.yaw, [1, 1, 1], z);
    }
  }
  // parapet across the whole front, then sometimes a crest over the middle
  const top = rec.ground_h + rows * rec.upper_h;
  if (ph >= 0.4) {
    const np = Math.max(1, Math.round(W / 4)), pw = W / np;
    for (let i = 0; i < np; i++) put(out, pw < 3 ? 'BRK_Parapet2' : 'BRK_Parapet4', at(f, i * pw), f.yaw, [1, pw / (pw < 3 ? 2 : 4), ph / rec.parapet_h], top);
    if (ph >= 1 && rnd() < (opts.crest ?? 0.35)) {
      const c = rnd() < 0.5 ? 'BRK_CrestA' : 'BRK_CrestB';
      put(out, c, at(f, W / 2 - 2), f.yaw, [1, 1, 1], h - 0.6);
    }
  }
  return out;
}

// A sign card (front-plane-bottom-left) centred at t along a box face, bottom at z.
export function signCard(b, north, id, t, z, proud = 0.3) {
  const f = frame(b, north), out = [];
  put(out, id, at(f, t - KIT[id].nominal[0] / 2, proud), f.yaw, [1, 1, 1], z);
  return out;
}

// A landmark that replaces its box: footprint-centre origin, front +X. Returns { body, extra } (body becomes the box's prop).
export function landmark(b, north, id) {
  const k = KIT[id], f = frame(b, north);
  const sw = b.w / k.nominal[0];
  const c = [b.x + b.w / 2, b.y + b.d / 2];
  const body = { m: k.mesh, x: r2(c[0]), y: r2(c[1]), z: 0, yaw: f.yaw, s: [1, r2(sw), 1] };
  // attachments are in the source (Blender) frame: x across (left to right from the street), -y forward
  const extra = [];
  for (const a of k.attachments || []) {
    const [ax, ay, az] = a.location;
    const fwd = -ay, right = ax * sw;
    put(extra, a.asset, [c[0] + f.F[0] * fwd + f.u[0] * right, c[1] + f.F[1] * fwd + f.u[1] * right], f.yaw, [1, 1, 1], az);
  }
  return { body, extra };
}
