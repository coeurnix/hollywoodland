// Collision for free-standing dressing: every prop on the ground that a person would walk around gets an axis-aligned
// rectangle (x0, y0, x1, y1, h) the client slides units along (FHLNav exact rects; not on the A* grid, so paths still
// thread between lamp posts). Posts collide only at their foot; walk-through dressing (slime, arches, wall decals,
// fire escapes, stairs) and anything off the ground does not collide. Posts are 0.9 m "tall" for line of sight and the
// camera, so they never block a shot or push the camera in.
import { PROPS } from './props.mjs';
import { KIT } from './facades.mjs';

const BY_MESH = new Map();
for (const p of Object.values(PROPS)) BY_MESH.set(p.mesh, p);
for (const [id, k] of Object.entries(KIT)) if (k.min && k.max && /^(DR_Rubble|DR_Sandbag|DR_FallenAwning)/.test(id)) BY_MESH.set(k.mesh, k);
const ID_OF = new Map();
for (const [id, p] of Object.entries(PROPS)) ID_OF.set(p.mesh, id);

const POSTS = /^(street_lamp|trolley_pole|lamp|post|pole_standing|hydrant|mailbox|clock)/;
const SKIP = /^(fire_escape|theatre_facade|hive_goo|hive_arch|film_canister|stair_|sign_|rug|puddle|decal)/;
const r2 = (v) => Math.round(v * 100) / 100;

export function propCols(list) {
  const out = [];
  for (const p of list) {
    const b = BY_MESH.get(p.m);
    if (!b || (p.z || 0) > 0.3) continue;
    const id = ID_OF.get(p.m) || '';
    if (SKIP.test(id)) continue;
    const [sx, sy, sz] = p.s || [1, 1, 1];
    const h = (b.max[2] - b.min[2]) * sz;
    if (h < 0.45) continue;
    if (POSTS.test(id)) { out.push([r2(p.x - 0.28), r2(p.y - 0.28), r2(p.x + 0.28), r2(p.y + 0.28), 0.9]); continue; }
    // rotate the local rectangle (shrunk a little so units brush the dressing rather than stop short of it)
    const k = 0.85, rad = ((p.yaw || 0) * Math.PI) / 180, c = Math.cos(rad), s = Math.sin(rad);
    const xs = [], ys = [];
    for (const u of [b.min[0] * sx * k, b.max[0] * sx * k]) for (const v of [b.min[1] * sy * k, b.max[1] * sy * k]) { xs.push(p.x + u * c - v * s); ys.push(p.y + u * s + v * c); }
    out.push([r2(Math.min(...xs)), r2(Math.min(...ys)), r2(Math.max(...xs)), r2(Math.max(...ys)), r2(h)]);
  }
  return out;
}
