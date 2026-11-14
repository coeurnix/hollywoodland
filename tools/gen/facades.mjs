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

// The four storefront families (brick, Spanish revival, Art Deco, masonry); a shop's family comes from its seed.
export const FAMILIES = Object.keys(R).filter((k) => R[k].ground);
const WEIGHTS = { DECO: 0.3, SPN: 0.25, BRK: 0.25, MSN: 0.2 };
export function pickFamily(seed) {
  const fams = FAMILIES.filter((k) => WEIGHTS[k]);
  let x = rng(seed * 104729 + 3)() * fams.reduce((s, k) => s + WEIGHTS[k], 0);
  for (const k of fams) { x -= WEIGHTS[k]; if (x <= 0) return k; }
  return fams[0];
}
const has = (id) => !!KIT[id];
// a ground-centre piece (dressing, roof props) at distance t along the face, `out` metres in front of it (negative: on the roof)
function putC(arr, f, id, t, out, z, yaw = 0, s = [1, 1, 1]) { if (has(id)) put(arr, id, at(f, t, out), f.yaw + yaw, s, z); }

// Lay a storefront on box b; returns placements. seed varies family, variants, awnings, crests, damage and dressing per shop.
export function kitFront(b, north, seed, opts = {}) {
  const fam = opts.family || pickFamily(seed);
  const rec = R[fam], out = [], rnd = rng(seed * 7919 + 17), f = frame(b, north);
  const W = f.W, h = b.h;
  const rows = Math.max(0, Math.min(3, Math.round((h - rec.ground_h - 0.5) / rec.upper_h)));
  const ph = Math.max(0, h - rec.ground_h - rows * rec.upper_h);
  const L = W - 2 * rec.corner_front_m;
  const n = Math.max(1, Math.round((L + 1) / 7));
  const bw = (L - (n - 1)) / n;
  const varG = rnd() < 0.5 ? 'A' : 'B', varU = rnd() < 0.5 ? 'A' : 'B';
  const bay = (ids, v) => ids.filter((id) => id.endsWith(v)).map((id) => ({ w: width(id), id }));
  const ground = bay(rec.ground, varG), upper = bay(rec.upper, varU);
  const awnings = rnd() < (opts.awning ?? 0.45);
  const damaged = rnd() < (opts.damaged ?? 0.15);
  const [dmgG, dmgU] = rec.damaged;
  const [aw4, aw6] = rec.awning, [pp2, pp4] = rec.parapet;
  const rowZ = (r) => rec.ground_h + (r - 1) * rec.upper_h; // r = 0 ground, 1.. upper rows

  for (let r = 0; r <= rows; r++) {
    const z = r === 0 ? 0 : rowZ(r);
    const sz = 1;
    // corners: the right one as modelled (return wraps the right side); the left one turned a quarter so its return is the front
    // (pushed OUT past the box's side face and 4 mm further forward than the neighbour's left corner: no coplanar faces to flicker)
    put(out, r ? rec.corner_upper : rec.corner, at(f, W - rec.corner_front_m + OUT, OUT + 0.004), f.yaw, [1, 1, sz], z);
    const lc = [f.S[0] - f.F[0] * (rec.corner_return_m - OUT) - f.u[0] * OUT, f.S[1] - f.F[1] * (rec.corner_return_m - OUT) - f.u[1] * OUT];
    put(out, r ? rec.corner_upper : rec.corner, lc, f.yaw + 90, [1, 1, sz], z);
    for (let i = 0; i < n; i++) {
      const t0 = rec.corner_front_m + i * (bw + 1);
      if (r === 0) {
        let m = nearest(bw, ground);
        if (damaged && m.w === 6 && i === n - 1) m = { w: 6, id: dmgG };
        put(out, m.id, at(f, t0), f.yaw, [1, bw / m.w, 1], 0);
        if (awnings && !(damaged && i === n - 1)) {
          const a = bw <= 5 ? aw4 : aw6;
          put(out, a, at(f, t0), f.yaw, [1, bw / width(a), 1], 0);
        }
      } else {
        const k = Math.max(1, Math.round(bw / 3.5));
        const uw = bw / k;
        for (let j = 0; j < k; j++) {
          let m = nearest(uw, upper);
          if (damaged && r === rows && m.w === 3 && i === 0 && j === 0) m = { w: 3, id: dmgU };
          put(out, m.id, at(f, t0 + j * uw), f.yaw, [1, uw / m.w, 1], z);
          // a boarded-up upper window on a hard-hit shop
          if (damaged && r === 1 && j === k - 1 && i === Math.floor(n / 2) && has('DR_BoardedWindow15')) put(out, 'DR_BoardedWindow15', at(f, t0 + j * uw + (uw - 1.5) / 2, OUT + 0.04), f.yaw, [1, 1, 1], z + 1.0);
        }
      }
      if (i < n - 1) put(out, r ? rec.pilaster_upper : rec.pilaster, at(f, t0 + bw), f.yaw, [1, 1, 1], z);
    }
  }
  // parapet across the whole front, then sometimes a crest over the middle
  const top = rec.ground_h + rows * rec.upper_h;
  if (ph >= 0.4) {
    const np = Math.max(1, Math.round(W / 4)), pw = W / np;
    for (let i = 0; i < np; i++) put(out, pw < 3 ? pp2 : pp4, at(f, i * pw), f.yaw, [1, pw / (pw < 3 ? 2 : 4), (ph + 0.03) / rec.parapet_h], top); // 3 cm above the roof: its top is not coplanar with the box's
    if (ph >= 1 && rnd() < (opts.crest ?? 0.35)) {
      const c = rec.crest[rnd() < 0.5 ? 0 : 1];
      put(out, c, at(f, W / 2 - width(c) / 2), f.yaw, [1, 1, 1], h - 0.6);
    }
  }
  // the occupation on the street: soot, cracks, broken glass, rubble and a fallen awning on damaged shops; sandbagged
  // doorways and the mayor's notices here and there
  if (damaged) {
    const tD = rec.corner_front_m + (n - 1) * (bw + 1); // the damaged bay
    if (rnd() < 0.7) put(out, rnd() < 0.5 ? 'DR_SootA' : 'DR_SootB', at(f, tD + 0.5 + rnd() * Math.max(0, bw - 2.5), OUT + 0.03), f.yaw, [1, 1, 1], rec.ground_h - 0.4 + rnd() * 1.5);
    if (rows > 0 && rnd() < 0.6) put(out, 'DR_Crack', at(f, rec.corner_front_m + rnd() * Math.max(0, L - 2), OUT + 0.03), f.yaw, [1, 1, 1], rec.ground_h + 0.6 + rnd() * 1.2);
    if (rnd() < 0.5) putC(out, f, 'DR_Rubble2', tD + bw / 2, 1.3, 0, Math.round(rnd() * 40 - 20));
    else putC(out, f, 'DR_FallenAwning', tD + bw / 2, 1.0, 0, Math.round(rnd() * 16 - 8));
  } else if (n > 1 && rnd() < (opts.glass ?? 0.08)) {
    const i = Math.floor(rnd() * n);
    put(out, 'DR_BrokenGlass', at(f, rec.corner_front_m + i * (bw + 1) + (bw - 3) / 2, OUT + 0.05), f.yaw, [1, 1, 1], 0.9);
  }
  if (rnd() < (opts.sandbags ?? 0.06) && bw >= 4) putC(out, f, 'DR_SandbagDoorway4', rec.corner_front_m + bw / 2, 0.75, 0);
  else if (rnd() < 0.06) putC(out, f, 'DR_SandbagStack3', rec.corner_front_m + 1.5 + rnd() * Math.max(0, L - 3), 0.6, 0);
  if (n > 1 && rnd() < (opts.notice ?? 0.18)) put(out, 'DR_MayorNotice', at(f, rec.corner_front_m + bw + 0.5 - 0.375, OUT + 0.25), f.yaw, [1, 1, 1], 1.3);
  return out;
}
export const brkFront = (b, north, seed, opts = {}) => kitFront(b, north, seed, { ...opts, family: 'BRK' });

// Roof clutter on a shop box: water tanks, chimneys, vents, skylights and stair huts, kept back from the parapet.
export function roofProps(b, north, seed) {
  const out = [], rnd = rng(seed * 6151 + 29), f = frame(b, north);
  if (b.d < 6 || b.w < 5 || rnd() < 0.3) return out;
  const spots = [];
  const tryPut = (id, r, prob) => {
    if (!has(id) || rnd() >= prob) return;
    for (let k = 0; k < 6; k++) {
      const t = r + 0.6 + rnd() * (b.w - 2 * r - 1.2), d = -(Math.max(2.5, r + 1.5) + rnd() * Math.max(0, b.d - 2 * Math.max(2.5, r + 1.5)));
      if (t < r || t > b.w - r || -d > b.d - r) continue;
      if (spots.some((s) => Math.hypot(s[0] - t, s[1] - d) < s[2] + r + 0.5)) continue;
      spots.push([t, d, r]);
      putC(out, f, id, t, d, b.h, Math.round(rnd() * 4) * 90);
      return;
    }
  };
  tryPut('RF_WaterTank', 1.8, b.h >= 9 ? 0.45 : 0.15);
  tryPut('RF_Skylight', 2.2, b.w >= 10 && b.d >= 10 ? 0.35 : 0);
  tryPut('RF_StairBulkhead', 2.4, b.w >= 9 ? 0.3 : 0);
  tryPut('RF_ChimneyBrick', 0.7, 0.55);
  tryPut('RF_VentMetal', 0.6, 0.5);
  tryPut('RF_VentMetal', 0.6, 0.3);
  return out;
}

// A sign card (front-plane-bottom-left) centred at t along a box face, bottom at z.
export function signCard(b, north, id, t, z, proud = 0.3) {
  const f = frame(b, north), out = [];
  put(out, id, at(f, t - KIT[id].nominal[0] / 2, proud), f.yaw, [1, 1, 1], z);
  return out;
}

// A landmark that replaces its box: footprint-centre origin, front +X. Returns { body, extra } (body becomes the box's prop).
//   face: true / 'S' = front faces +y (a building north of its street; the SPEC's "S"), false / 'N' = -y, 'W' = -x, 'E' = +x.
//   opts.mirror: flip it across (the right-hand one of a mirrored pair: wings, forecourt and courtyard walls).
// The mesh is stretched to the box (a warning past 10%, the box should match the nominal size). Sign attachments come
// from the manifest (bottom-left origin, Blender frame: x across left to right from the street, -y forward, z up).
const FACES = {
  S: { F: [0, 1], u: [1, 0], yaw: 90 }, N: { F: [0, -1], u: [-1, 0], yaw: -90 },
  W: { F: [-1, 0], u: [0, 1], yaw: 180 }, E: { F: [1, 0], u: [0, -1], yaw: 0 },
};
export function landmark(b, face, id, opts = {}) {
  const k = KIT[id];
  if (!k) throw new Error(`unknown landmark ${id}`);
  const f = FACES[face === true ? 'S' : face === false ? 'N' : face];
  const across = f.F[0] ? b.d : b.w, deep = f.F[0] ? b.w : b.d;
  const sw = across / k.nominal[0], sd = deep / k.nominal[1];
  if (Math.abs(sw - 1) > 0.1 || Math.abs(sd - 1) > 0.1) console.warn(`landmark ${id}: box ${across}x${deep} vs nominal ${k.nominal[0]}x${k.nominal[1]}`);
  const m = opts.mirror ? -1 : 1;
  const c = [b.x + b.w / 2, b.y + b.d / 2];
  const body = { m: k.mesh, x: r2(c[0]), y: r2(c[1]), z: r2(opts.z || 0), yaw: f.yaw, s: [r2(sd), r2(sw * m), 1] };
  const extra = [];
  for (const a of k.attachments || []) {
    const [ax, ay, az] = a.location, [sx, , sz] = a.scale || [1, 1, 1];
    const cw = (KIT[a.asset].nominal[0]) * sx;
    // a mirrored parent keeps its signs readable: the card's span [ax, ax + cw] maps to [-ax - cw, -ax]
    const fwd = -ay * sd, right = (m > 0 ? ax : -ax - cw) * sw;
    put(extra, a.asset, [c[0] + f.F[0] * fwd + f.u[0] * right, c[1] + f.F[1] * fwd + f.u[1] * right], f.yaw, [1, sx, sz], az + (opts.z || 0));
  }
  return { body, extra };
}
// A non-colliding landmark (a gate, a shelter, a kiosk): its pieces only, centred on (x, y).
export function landmarkDeco(id, x, y, face, opts = {}) {
  const k = KIT[id], f = FACES[face === true ? 'S' : face === false ? 'N' : face];
  const [w, d] = f.F[0] ? [k.nominal[1], k.nominal[0]] : [k.nominal[0], k.nominal[1]];
  const { body, extra } = landmark({ x: x - w / 2, y: y - d / 2, w, d }, face, id, opts);
  return [body, ...extra];
}
