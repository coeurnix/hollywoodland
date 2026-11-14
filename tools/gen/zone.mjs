// Built slice (DESIGN-BIBLE §14): Hollywood Boulevard layout, the Egyptian Theatre, mob templates, NPCs, camps, objects.
// Units: metres. x = east, y = south. The client turns this into gray-box geometry and a 2 m nav grid.
// Each area has its own grid; `offset` places it in the single UE level.
// Props (art/props, imported by tools/propprep.py): a box may carry `prop` (drawn instead of its cube, the box stays the collision),
// an object may carry `prop` (drawn instead of its primitive), and `props` lists free-standing dressing (no collision).
import { PROPS as PROPS_TABLE, placement, footprint, fitBox, rng } from './props.mjs';
import { kitFront, roofProps, landmark, landmarkDeco, signCard } from './facades.mjs';
import { propCols } from './collide.mjs';
const C = { road: [0.13, 0.13, 0.14], walk: [0.3, 0.29, 0.27], lot: [0.34, 0.31, 0.24], hill: [0.36, 0.34, 0.22], shop: [0.48, 0.44, 0.38], shop2: [0.42, 0.4, 0.42],
  landmark: [0.72, 0.62, 0.42], hive: [0.22, 0.12, 0.26], civic: [0.6, 0.6, 0.62], wreck: [0.2, 0.22, 0.25], interior: [0.25, 0.2, 0.18], seat: [0.4, 0.1, 0.1] };

// ---------- mob templates ----------
const M = (id, name, family, lmin, lmax, o = {}) => ({ id, name, family, level_min: lmin, level_max: lmax, rank: 'normal', hp: 1, dmg: 1, armor: 1, aggressive: true,
  flees: false, ranged: false, social: 0, speed: 6.5, swing: 2.0, abilities: [], shape: 'bug', color: [0.35, 0.5, 0.2], scale: 1, junk: [], trade: [], money: true, respawn: 90, built: true, ...o });
export const mobs = [
  M('mob_invader_worker', 'Invader Worker', 'invader_worker', 1, 2, { hp: 0.6, dmg: 0.5, armor: 0.5, aggressive: false, flees: true, color: [0.55, 0.6, 0.3], scale: 0.8, junk: ['Bent Antenna', 'Dull Chitin Flake'], trade: [['Chitin Shard', 0.15]] }),
  M('mob_feral_dog', 'Feral Dog', 'feral_dog', 2, 3, { hp: 0.9, dmg: 0.9, armor: 0.7, social: 10, shape: 'quad', color: [0.45, 0.33, 0.2], scale: 0.8, speed: 7.5, junk: ['Matted Fur', 'Chewed Bone'], money: false }),
  M('mob_coyote', 'Coyote', 'coyote', 3, 4, { hp: 0.9, dmg: 0.9, armor: 0.8, social: 10, shape: 'quad', color: [0.62, 0.52, 0.36], scale: 0.85, speed: 7.5, junk: ['Matted Fur', 'Chewed Bone'], trade: [['Coyote Pelt', 0.25]], money: false }),
  M('mob_invader_drone', 'Invader Drone', 'invader_drone', 4, 6, { social: 12, color: [0.3, 0.45, 0.2], junk: ['Cracked Mandible', 'Dull Chitin Flake'], trade: [['Chitin Shard', 0.2]] }),
  M('mob_invader_drone_hs', 'Invader Drone', 'invader_drone', 7, 8, { social: 12, color: [0.28, 0.4, 0.18], junk: ['Cracked Mandible', 'Dull Chitin Flake'], trade: [['Chitin Shard', 0.25]] }),
  M('mob_invader_spitter', 'Invader Spitter', 'invader_spitter', 5, 7, { hp: 0.8, dmg: 0.9, armor: 0.8, ranged: true, abilities: ['ab_mob_acid_spit'], color: [0.55, 0.65, 0.15], scale: 0.9, junk: ['Acid-pitted Scale', 'Bent Antenna'], trade: [['Invader Gland', 0.15]] }),
  M('mob_looter', 'Looter', 'looter', 5, 6, { shape: 'human', color: [0.35, 0.3, 0.28], social: 8, junk: ['Empty Hip Flask', 'Pawn Ticket', 'Torn Movie Ticket'], trade: [['Brass Casings', 0.3]] }),
  M('mob_invader_broodling', 'Invader Broodling', 'invader_broodling', 7, 8, { hp: 0.25, dmg: 0.35, armor: 0.5, social: 8, color: [0.6, 0.7, 0.4], scale: 0.45, speed: 7, junk: ['Broodling Husk'], respawn: 60 }),
  M('mob_invader_soldier', 'Invader Soldier', 'invader_soldier', 9, 10, { hp: 1.3, dmg: 1.1, armor: 2, color: [0.2, 0.3, 0.15], scale: 1.25, junk: ['Cracked Mandible', 'Acid-pitted Scale'], trade: [['Chitin Shard', 0.35]] }),
  M('mob_old_scar', 'Old Scar', 'coyote', 5, 5, { rank: 'rare', hp: 0.9, dmg: 0.9, armor: 0.8, shape: 'quad', color: [0.75, 0.7, 0.6], scale: 1.15, speed: 7.5, title: 'coyote', junk: ['Matted Fur'], money: false, respawn: 600 }),
  M('mob_selma_burrower', 'The Selma Avenue Burrower', 'invader_burrower', 7, 7, { rank: 'rare', script: 'burrower', hp: 0.9, dmg: 1.2, color: [0.5, 0.3, 0.2], scale: 1.3, title: 'invader burrower', junk: ['Cracked Mandible'], respawn: 900 }),
  M('mob_looter_ringleader', 'Knuckles Malone', 'looter', 8, 8, { rank: 'elite', hp: 0.7, dmg: 0.8, shape: 'human', color: [0.5, 0.15, 0.15], scale: 1.1, title: 'Looter ringleader', junk: ['Pawn Ticket'], respawn: 240 }),
  M('mob_ballroom_brute', 'Ballroom Brute', 'invader_brute', 11, 11, { rank: 'elite', hp: 1.2, dmg: 1.2, armor: 1.5, abilities: ['ab_mob_knockback'], color: [0.25, 0.2, 0.3], scale: 1.9, title: 'Invader Brute', junk: ['Cracked Mandible'], respawn: 480 }),
  // Exit guards: far above any player who can reach them.
  // They outrun a player (speed 8.5 against 7), daze on every hit (client rule: 5+ levels up) and see a level-6 player from 30 m.
  M('mob_guard_cahuenga', 'Hardened Invader Soldier', 'invader_soldier', 34, 34, { rank: 'elite', hp: 1.3, dmg: 1.1, armor: 2, color: [0.15, 0.2, 0.1], scale: 1.6, guard: true, speed: 8.5, social: 30, respawn: 60 }),
  M('mob_guard_gower', 'Invader Brute', 'invader_brute', 22, 22, { rank: 'elite', hp: 1.2, dmg: 1.2, armor: 1.5, color: [0.25, 0.2, 0.3], scale: 1.9, guard: true, speed: 8.5, social: 30, respawn: 60 }),
  M('mob_guard_melrose', 'Invader Brute', 'invader_brute', 24, 24, { rank: 'elite', hp: 1.2, dmg: 1.2, armor: 1.5, color: [0.25, 0.2, 0.3], scale: 1.9, guard: true, speed: 8.5, social: 30, respawn: 60 }),
  M('mob_guard_la_brea', 'Dragna Enforcer', 'dragna_enforcer', 30, 30, { rank: 'elite', hp: 1.2, dmg: 1.2, armor: 1.2, shape: 'human', color: [0.12, 0.12, 0.14], scale: 1.15, guard: true, speed: 8.5, social: 30, respawn: 60 }),
  // Beyond the wall: roaming packs from the neighbouring zones, far above the slice. A taste of what is out there.
  M('mob_wall_stalker', 'Hive Wall Stalker', 'invader_soldier', 25, 27, { rank: 'elite', hp: 1.2, dmg: 1.1, armor: 1.6, color: [0.18, 0.12, 0.22], scale: 1.45, speed: 9, social: 24, respawn: 120 }),
  M('mob_pass_soldier', 'Hardened Invader Soldier', 'invader_soldier', 32, 35, { rank: 'elite', hp: 1.3, dmg: 1.1, armor: 2, color: [0.15, 0.2, 0.1], scale: 1.6, speed: 8.5, social: 24, respawn: 120 }),
  M('mob_pass_flier', 'Hardened Invader Flier', 'invader_flier', 33, 36, { rank: 'elite', hp: 0.8, dmg: 1.0, armor: 0.7, script: 'flier', color: [0.35, 0.3, 0.12], scale: 1.2, speed: 10, social: 24, respawn: 120 }),
  M('mob_wh_gunman', 'Dragna Gunman', 'dragna_enforcer', 26, 28, { rank: 'elite', hp: 0.9, dmg: 1.1, armor: 0.9, shape: 'human', ranged: true, color: [0.14, 0.13, 0.12], scale: 1.05, speed: 8.5, social: 24, respawn: 120 }),
  M('mob_melrose_brute', 'Invader Brute', 'invader_brute', 22, 24, { rank: 'elite', hp: 1.2, dmg: 1.2, armor: 1.5, color: [0.25, 0.2, 0.3], scale: 1.9, speed: 8.5, social: 24, respawn: 120 }),
  M('mob_gulch_coyote', 'Hive-ridden Coyote', 'coyote', 18, 20, { hp: 0.9, dmg: 1.1, armor: 0.8, shape: 'quad', color: [0.4, 0.3, 0.35], scale: 0.95, speed: 9, social: 20, respawn: 90 }),
  // The Egyptian Theatre
  M('mob_egy_drone', 'Theatre Drone', 'invader_drone', 9, 9, { rank: 'elite', hp: 0.95, dmg: 0.8, social: 10, color: [0.3, 0.45, 0.2], junk: ['Cracked Mandible'], respawn: 0 }),
  M('mob_egy_spitter', 'Balcony Spitter', 'invader_spitter', 10, 10, { rank: 'elite', hp: 0.8, dmg: 0.75, armor: 0.8, ranged: true, social: 10, abilities: ['ab_mob_acid_spit'], color: [0.55, 0.65, 0.15], scale: 0.9, junk: ['Acid-pitted Scale'], respawn: 0 }),
  M('mob_egy_broodling', 'Aisle Broodling', 'invader_broodling', 9, 9, { hp: 0.25, dmg: 0.35, armor: 0.5, social: 8, color: [0.6, 0.7, 0.4], scale: 0.45, speed: 7, junk: ['Broodling Husk'], respawn: 0 }),
  M('mob_egy_soldier', 'Stage Soldier', 'invader_soldier', 11, 11, { rank: 'elite', hp: 1.0, dmg: 0.8, armor: 2, social: 10, color: [0.2, 0.3, 0.15], scale: 1.25, junk: ['Cracked Mandible'], respawn: 0 }),
  M('mob_boss_brood_keeper', 'Brood Keeper', 'invader_brute', 9, 9, { rank: 'boss', hp: 1.3, dmg: 0.7, armor: 1.5, color: [0.4, 0.25, 0.35], scale: 2.0, script: 'brood_keeper', abilities: ['ab_mob_knockback'], respawn: 0 }),
  M('mob_boss_hive_warden', 'Hive Warden', 'invader_soldier', 10, 10, { rank: 'boss', hp: 1.3, dmg: 0.75, armor: 2, color: [0.15, 0.3, 0.3], scale: 1.9, script: 'hive_warden', abilities: ['ab_mob_charge'], respawn: 0 }),
  M('mob_boss_matriarch', 'Spitter Matriarch', 'invader_spitter', 11, 11, { rank: 'boss', hp: 1.3, dmg: 0.75, armor: 0.8, ranged: true, color: [0.7, 0.75, 0.1], scale: 1.8, script: 'matriarch', abilities: ['ab_mob_acid_spit', 'ab_mob_acid_pool'], respawn: 0 }),
  M('mob_boss_hive_mother', 'The Egyptian Hive Mother', 'invader_queen', 12, 12, { rank: 'boss', hp: 1.7, dmg: 0.9, armor: 1.5, color: [0.55, 0.1, 0.45], scale: 2.6, script: 'hive_mother', abilities: ['ab_mob_knockback'], respawn: 0 }),
];

// ---------- geometry helpers ----------
const boxes = [], ground = [], labels = [];
const box = (x0, y0, x1, y1, h, color, label, o = {}) => { const b = { x: x0, y: y0, w: x1 - x0, d: y1 - y0, h, color, label: label || '', ...o }; boxes.push(b); return b; };
// ground strips: kind picks the painted ground tile (T_GND_<Kind>, art/textures); colourless kinds stay flat colour
const KIND_OF = new Map();
const strip = (x0, y0, x1, y1, color, kind = KIND_OF.get(color)) => ground.push({ x: x0, y: y0, w: x1 - x0, d: y1 - y0, color, ...(kind ? { kind } : {}) });
function shops(x0, x1, y0, y1, seed, names = []) {
  // a row of storefronts of varying width/height along x
  let x = x0, i = 0;
  const row = [];
  while (x < x1 - 6) {
    const w = Math.min(x1 - x, 14 + ((seed * 7 + i * 13) % 12));
    const h = 6 + ((seed * 5 + i * 11) % 4) * 3;
    row.push(box(x, y0, x + w - 0.5, y1, h, i % 2 ? C.shop : C.shop2, names[i] || ''));
    x += w; i++;
  }
  return row;
}
// dressing: free props, and props that are their own collision box (axis-aligned yaw)
const props = [];
const deco = (id, x, y, yaw = 0, s = 1, z = 0) => props.push(placement(id, x, y, yaw, s, z));
const propBox = (id, x, y, yaw, s, color) => { const [x0, y0, x1, y1] = footprint(id, x, y, yaw, s); const p = placement(id, x, y, yaw, s); return box(x0, y0, x1, y1, (PROP_H(id) * (Array.isArray(s) ? s[2] : s)), color, '', { prop: p }); };
const PROP_H = (id) => PROPS_TABLE[id].max[2] - PROPS_TABLE[id].min[2];
// shop signs: named shops get their own; a seeded share of the rest get a generic one. North fronts face +y, south fronts -y.
const GENERIC_SIGNS = ['sign_fascia', 'sign_neon', 'sign_blade_tailor', 'sign_blade_photo', 'sign_cafe'];
const NAMED_SIGNS = { 'Max Factor Salon': 'sign_max_factor', 'Hollywood Gun Shop': 'sign_gun_shop', Haberdashery: 'sign_haberdashery', Drugstore: 'sign_drugstore' };
const signRand = rng(7);
// building-kit fronts (tools/gen/facades.mjs): every shop gets a dressed BRK street face; the Drugstore is its own landmark
let facadeSeed = 0;
const KIT_SIGNS = { Haberdashery: 'SIGN_Haberdashery' };
function facades(row, north) {
  for (const b of row) {
    facadeSeed++;
    if (b.label === 'Drugstore') {
      const { body, extra } = landmark(b, north, 'LM_Drugstore');
      b.prop = body; b.noSign = true; props.push(...extra);
      continue;
    }
    props.push(...kitFront(b, north, facadeSeed));
    props.push(...roofProps(b, north, facadeSeed));
    if (KIT_SIGNS[b.label]) { props.push(...signCard(b, north, KIT_SIGNS[b.label], b.w / 2, 3.55)); b.noSign = true; }
  }
}
function signs(row, north) {
  for (const b of row) {
    if (b.noSign) continue;
    const id = NAMED_SIGNS[b.label] || (signRand() < 0.6 ? GENERIC_SIGNS[Math.floor(signRand() * GENERIC_SIGNS.length)] : null);
    if (!id) continue;
    const blade = PROPS_TABLE[id].max[0] > 0.5;  // projects from the wall: hang it near one end of the shop
    const x = blade ? b.x + 1.2 : b.x + b.w / 2, y = north ? b.y + b.d : b.y;
    deco(id, x, y, north ? 90 : -90, 1, Math.min(b.h - 1.2, blade ? 4.6 : 4.2));
  }
}
// fire escapes on the alley side of the taller shops
function escapes(row, north, every = 2) {
  row.filter((b) => b.h >= 9).forEach((b, i) => {
    if (i % every) return;
    const id = b.h >= 12 ? 'fire_escape' : 'fire_escape_wall';
    deco(id, b.x + b.w / 2, north ? b.y - 1.4 : b.y + b.d + 1.4, north ? 180 : 0);
  });
}

// Streets
const X0 = -140, X1 = 940, Y0 = -140, Y1 = 590;
// The band beyond the hive wall (out to the second ring at OX0..OY1): blighted ground, roads running on into the next zones
const OX0 = -320, OX1 = 1120, OY0 = -320, OY1 = 770;
const blight = [0.2, 0.15, 0.2];
KIND_OF.set(C.road, 'Asphalt'); KIND_OF.set(C.walk, 'Sidewalk'); KIND_OF.set(blight, 'Blight');
strip(OX0, OY0, OX1, -106, blight); strip(OX0, 556, OX1, OY1, blight); strip(OX0, -106, -106, 556, blight); strip(906, -106, OX1, 556, blight);
strip(OX0, 190, X0, 210, C.road); strip(X1, 190, OX1, 210, C.road);   // Hollywood Blvd runs on west to La Brea, east to Gower
strip(290, OY0, 306, Y0, C.road); strip(290, Y1, 306, OY1, C.road);   // Highland north (Cahuenga Pass) and south (Melrose)
strip(X0, 190, X1, 210, C.road);                                   // Hollywood Blvd
strip(X0, 20, X1, 32, C.road); strip(X0, 300, X1, 312, C.road); strip(X0, 420, X1, 436, C.road); // Franklin, Selma, Sunset
for (const [a, b] of [[10, 26], [140, 150], [290, 306], [420, 430], [520, 530], [610, 622], [750, 766]]) strip(a, Y0, b, Y1, C.road);
strip(X0, 186, X1, 190, C.walk); strip(X0, 210, X1, 214, C.walk);
// red-car rails down the middle of the boulevard
strip(X0, 199.2, X1, 199.5, [0.5, 0.5, 0.52]); strip(X0, 200.5, X1, 200.8, [0.5, 0.5, 0.52]);

// North side of the boulevard
{ const r = shops(30, 48, 150, 186, 1); facades(r, true); signs(r, true); escapes(r, true); }
// the hub landmarks (art/buildings LM_*): the mesh replaces the box and carries its sign cards. face: the SPEC's front
// letter (S = +y, the north side of the boulevard; N = -y; W = -x). Mirrored pairs flip the right-hand one.
const LM = (b, face, id, opts) => { const { body, extra } = landmark(b, face, id, opts); b.prop = body; b.noSign = true; props.push(...extra); return b; };
LM(box(52, 108, 134, 166, 16, C.landmark, "Grauman's Chinese Theatre"), 'S', 'LM_Graumans');
LM(box(52, 166, 58, 186, 5, C.landmark), 'S', 'LM_Graumans_ForecourtWall'); LM(box(128, 166, 134, 186, 5, C.landmark), 'S', 'LM_Graumans_ForecourtWall', { mirror: true }); // forecourt walls
{ const r = shops(154, 168, 150, 186, 2, ['Max Factor Salon']); facades(r, true); signs(r, true); }
LM(box(170, 160, 286, 186, 14, C.landmark, 'Hollywood Hotel'), 'S', 'LM_HollywoodHotel');
LM(box(170, 108, 194, 160, 12, C.landmark), 'S', 'LM_HollywoodHotel_Wing'); LM(box(262, 108, 286, 160, 12, C.landmark), 'S', 'LM_HollywoodHotel_Wing', { mirror: true });
LM(box(194, 104, 220, 110, 6, C.landmark), 'S', 'LM_HollywoodHotel_CourtWall'); LM(box(236, 104, 262, 110, 6, C.landmark), 'S', 'LM_HollywoodHotel_CourtWall', { mirror: true }); // ballroom north wall with a 16 m gap
LM(box(310, 148, 352, 186, 22, C.civic, 'First National Bank'), 'S', 'LM_FirstNational');
// the Pacific Electric stop on the north kerb by the red car (non-colliding)
props.push(...landmarkDeco('LM_RedCarStop', 268, 187.5, 'S'));
{ const r = shops(356, 416, 150, 186, 3, ['Hollywood Gun Shop', 'Haberdashery']); facades(r, true); signs(r, true); escapes(r, true); }
{ const r = shops(434, 516, 150, 186, 4, ['Drugstore']); facades(r, true); signs(r, true); escapes(r, true); }
{ const r = shops(534, 606, 150, 186, 5); facades(r, true); signs(r, true); escapes(r, true); }
{ const r = shops(626, 688, 150, 186, 6); facades(r, true); signs(r, true); escapes(r, true); }
LM(box(690, 128, 746, 186, 18, C.landmark, 'Pantages Theatre'), 'S', 'LM_Pantages');
{ const r = shops(770, 840, 150, 186, 7); facades(r, true); signs(r, true); escapes(r, true); }
// South side
{ const r = shops(30, 56, 214, 250, 8); facades(r, false); signs(r, false); escapes(r, false); }
LM(box(60, 214, 134, 272, 24, C.landmark, 'Roosevelt Hotel'), 'N', 'LM_Roosevelt');
{ const r = shops(154, 286, 214, 250, 9); facades(r, false); signs(r, false); escapes(r, false); }
{ const r = shops(310, 416, 214, 250, 10); facades(r, false); signs(r, false); escapes(r, false); }
LM(box(434, 214, 460, 242, 7, C.landmark), 'N', 'LM_Egyptian_Wing'); LM(box(490, 214, 516, 242, 7, C.landmark), 'N', 'LM_Egyptian_Wing', { mirror: true }); // Egyptian courtyard wings
LM(box(434, 242, 516, 292, 15, C.landmark, 'The Egyptian Theatre'), 'N', 'LM_Egyptian');
props.push(...landmarkDeco('LM_Egyptian_Gateway', 475, 215, 'N'));                         // the pylon gate at the courtyard mouth
{ const r = shops(534, 606, 214, 252, 11); facades(r, false); signs(r, false); escapes(r, false); }
LM(box(538, 262, 584, 296, 10, C.civic, 'Hollywood Police Station'), 'N', 'LM_PoliceStation');
{ const r = shops(626, 746, 214, 250, 12); facades(r, false); signs(r, false); escapes(r, false); }
LM(box(770, 214, 812, 262, 36, C.landmark, 'Taft Building'), 'N', 'LM_Taft');
props.push(...landmarkDeco('LM_Kiosk_Auction', 785.5, 211.8, 'N'));                       // the auction booth by Miss Reed, clear of the gathering point (776, 208)
LM(box(770, 268, 794, 298, 8, C.landmark, 'Brown Derby'), 'W', 'LM_BrownDerby');           // faces Vine: 30 m along it, 24 deep
box(470, 248, 482, 250, 5, C.hive);                                                           // (decor) hive growth at the Egyptian door
// Hollywood High (Sunset & Highland)
box(196, 344, 262, 352, 9, C.civic, 'Hollywood High School'); box(196, 352, 204, 404, 9, C.civic); box(254, 352, 262, 404, 9, C.civic); // gym: open to the south
// Aid stations and misc landmarks
box(488, 300, 512, 318, 8, C.civic, 'Hollywood Receiving Hospital'); box(626, 256, 646, 280, 9, C.civic, 'Fire Station No. 27'); box(112, 440, 138, 470, 12, C.civic, 'Blessed Sacrament Church');
// Wrecks and cover in the back lots
// wrecks: each prop is its own cover box (the scrap objects sit just south of them)
const WRECKS = ['wreck_rusted', 'wreck_charred', 'truck_wrecked', 'wreck_van', 'wreck_newsreel', 'wreck_overturned', 'wreck_burned', 'wreck_hive_truck', 'truck_starlight_wreck'];
[[650, 276], [676, 290], [702, 272], [352, 340], [388, 372], [330, 396], [672, 352], [718, 384], [820, 352]].forEach(([x, y], i) => propBox(WRECKS[i], x + 3.5, y + 1.5, i % 2 ? 180 : 0, 1, C.wreck));
[[90, 60], [250, 70], [470, 40], [560, 90], [660, 20], [760, 80], [380, 100], [820, -20], [180, -30]].forEach(([x, y], i) => propBox('rock_mossy', x + 2.5, y + 2.5, (i % 4) * 90, [2.6, 3.4, 2 + (x % 3)].map((v, k) => k === 2 ? v / 0.65 : v), C.hill));

// Hive walls: the perimeter, with one gap per road out. Gap guards are elites far above the zone.
const WALL_H = 18;
function wallX(x, gaps) { let y = Y0; for (const [a, b] of gaps) { box(x - 4, y, x + 4, a, WALL_H, C.hive); y = b; } box(x - 4, y, x + 4, Y1, WALL_H, C.hive); }
function wallY(y, gaps) { let x = X0; for (const [a, b] of gaps) { box(x, y - 4, a, y + 4, WALL_H, C.hive); x = b; } box(x, y - 4, X1, y + 4, WALL_H, C.hive); }
wallX(-110, [[192, 208]]); wallX(910, [[192, 208]]); wallY(-110, [[290, 306]]); wallY(560, [[290, 306]]);
// the hive's second ring seals the band (the edge of the playable map)
box(OX0 - 8, OY0 - 8, OX1 + 8, OY0, 26, C.hive); box(OX0 - 8, OY1, OX1 + 8, OY1 + 8, 26, C.hive);
box(OX0 - 8, OY0, OX0, OY1, 26, C.hive); box(OX1, OY0, OX1 + 8, OY1, 26, C.hive);
// north: Cahuenga Pass (30-40), the Bowl up Highland, broken hills, hive spires
box(250, -300, 350, -268, 14, C.landmark, 'Hollywood Bowl'); box(230, -290, 248, -262, 9, C.landmark); box(352, -290, 370, -262, 9, C.landmark);
for (const [x, y, w, h] of [[-260, -230, 60, 8], [-120, -260, 50, 12], [40, -200, 70, 10], [150, -280, 60, 14], [430, -240, 80, 12], [600, -280, 70, 16], [760, -210, 60, 10], [900, -270, 90, 14], [1010, -200, 60, 8]]) box(x, y, x + w, y + w * 0.6, h, C.hill);
for (const [x, y] of [[120, -150], [470, -160], [700, -150], [-60, -170], [880, -150], [330, -200]]) box(x, y, x + 5, y + 5, 22, C.hive, '', { prop: fitBox('hive_spire_tall', x, y, x + 5, y + 5, 22) });
// east: Gower Gulch (8-16): Columbia at Sunset & Gower, Poverty Row shacks
box(1010, 120, 1090, 184, 16, C.landmark, 'Columbia Pictures'); box(1010, 216, 1040, 260, 7, C.shop, 'Poverty Row'); box(1050, 216, 1074, 250, 6, C.shop2); box(1080, 220, 1104, 262, 7, C.shop);
box(940, 60, 980, 90, 6, C.shop2); box(950, 300, 990, 330, 6, C.shop); box(1040, 380, 1080, 420, 9, C.civic, 'Christie Studio');
for (const [x, y] of [[960, 150], [960, 250], [1060, 330], [1000, 20], [990, 480]]) box(x, y, x + 5, y + 5, 22, C.hive, '', { prop: fitBox('hive_spire_tall', x, y, x + 5, y + 5, 22) });
// south: Melrose (10-20): the cemetery gate, Paramount and RKO beyond
box(330, 640, 400, 652, 8, C.civic, 'Hollywood Cemetery'); box(330, 652, 342, 700, 4, C.civic); box(400, 640, 470, 652, 4, C.civic);
box(150, 690, 280, 750, 18, C.landmark, 'Paramount Pictures'); box(500, 680, 600, 750, 14, C.landmark, 'RKO Radio Pictures');
for (const [x, y] of [[120, 600], [220, 610], [420, 600], [640, 620], [760, 600], [40, 680]]) box(x, y, x + 5, y + 5, 22, C.hive, '', { prop: fitBox('hive_spire_tall', x, y, x + 5, y + 5, 22) });
// west: West Hollywood (20-30): La Brea & Sunset, Chaplin's studio
box(-290, 230, -200, 300, 14, C.landmark, 'Chaplin Studios'); box(-300, 120, -240, 180, 9, C.shop, 'La Brea Avenue'); box(-230, 120, -180, 180, 7, C.shop2);
box(-300, 400, -220, 450, 10, C.shop, 'Sunset & La Brea'); box(-200, 400, -150, 440, 7, C.shop2);
for (const [x, y] of [[-150, 150], [-160, 260], [-260, 40], [-250, 350], [-170, 500]]) box(x, y, x + 5, y + 5, 22, C.hive, '', { prop: fitBox('hive_spire_tall', x, y, x + 5, y + 5, 22) });

const subzones = [
  { name: "Grauman's Chinese Forecourt", x: 30, y: 150, w: 120, d: 70 }, { name: 'Hollywood & Highland', x: 150, y: 150, w: 270, d: 70 },
  { name: 'Hollywood & Vine', x: 622, y: 150, w: 230, d: 120 }, { name: 'Hollywood Police Station', x: 516, y: 250, w: 80, d: 54 },
  { name: 'Franklin Slopes', x: X0, y: -100, w: X1 - X0, d: 250 }, { name: 'Selma Avenue Alleys', x: 306, y: 262, w: 200, d: 156 },
  { name: 'Cahuenga Rooftops', x: 600, y: 300, w: 150, d: 120 }, { name: 'Taft Back Lots', x: 766, y: 300, w: 120, d: 120 },
  { name: 'Hollywood High School', x: 30, y: 320, w: 260, d: 110 }, { name: 'Hollywood Hotel Ballroom', x: 194, y: 104, w: 68, d: 56 },
  { name: 'The Egyptian Theatre', x: 434, y: 214, w: 82, d: 80 }, { name: 'Sunset Boulevard', x: X0, y: 420, w: X1 - X0, d: 60 },
  { name: 'The Hive Wall', x: X0, y: Y0, w: 34, d: Y1 - Y0 }, { name: 'The Hive Wall', x: 906, y: Y0, w: 34, d: Y1 - Y0 },
  { name: 'The Hive Wall', x: X0, y: Y0, w: X1 - X0, d: 34 }, { name: 'The Hive Wall', x: X0, y: 556, w: X1 - X0, d: 34 },
  // beyond the wall: the edges of the neighbouring zones (seen, deadly, not built)
  { name: 'Cahuenga Pass', x: OX0, y: OY0, w: OX1 - OX0, d: Y0 - OY0, levels: '30-40', hostile: true },
  { name: 'Melrose', x: OX0, y: Y1, w: OX1 - OX0, d: OY1 - Y1, levels: '10-20', hostile: true },
  { name: 'West Hollywood', x: OX0, y: Y0, w: X0 - OX0, d: Y1 - Y0, levels: '20-30', hostile: true },
  { name: 'Gower Gulch', x: X1, y: Y0, w: OX1 - X1, d: Y1 - Y0, levels: '8-16', hostile: true },
];

// ---------- NPCs ----------
const npcs = [];
const N = (id, name, title, x, y, yaw, roles, o = {}) => npcs.push({ id, name, title, x, y, yaw, roles, color: [0.8, 0.7, 0.5], ...o });
N('npc_sid_grauman', 'Sid Grauman', 'Showman', 92, 172, 90, ['quest'], { real_person: true, died: 1950, hub: 'hub_chinese_forecourt' });
N('npc_max_factor', 'Max Factor', 'Pharmacy Trainer', 158, 188, 90, ['quest', 'vendor'], { real_person: true, died: 1938, stock: 'pharmacy', hub: 'hub_chinese_forecourt' });
[['veteran', 'Sgt. Walt Brenner', 64], ['stuntman', 'Buck Tolliver', 70], ['gunman', 'Annie Pruitt', 76], ['electrician', 'Lou Kessler', 82],
  ['powderman', 'Dutch Vanderhoek', 102], ['nurse', 'Edith Collier, R.N.', 108], ['preacher', 'Rev. Amos Tully', 114], ['mechanic', 'Hank Mulroney', 120]]
  .forEach(([cls, name, x]) => N(`npc_trainer_${cls}`, name, `${cls[0].toUpperCase() + cls.slice(1).replace('powderman', 'owder Man').replace('Powder Man', 'Powder Man')} Trainer`, x, 181, 90, ['trainer', 'quest'], { trains: cls, hub: 'hub_chinese_forecourt' }));
npcs.find((n) => n.id === 'npc_trainer_powderman').title = 'Powder Man Trainer';
N('npc_newsie', 'Mickey Doyle', 'General Goods', 124, 187, 90, ['vendor'], { stock: 'general', hub: 'hub_chinese_forecourt' });
N('npc_roosevelt_clerk', 'Mr. Pennington', 'Roosevelt Hotel Front Desk', 96, 212.5, -90, ['quest', 'vendor', 'innkeeper'], { stock: 'food', hub: 'hub_chinese_forecourt' });
N('npc_teller', 'Miss Abigail Frost', 'Teller, First National Bank', 330, 188, 90, ['banker'], { hub: 'hub_highland' });
N('npc_conductor', 'Conductor Ruiz', 'Pacific Electric', 284, 196, 0, ['redcar'], { hub: 'hub_highland' });
N('npc_gunsmith', 'Earl Tanner', 'Gunsmithing Trainer', 366, 188, 90, ['quest', 'vendor', 'repair', 'trainer_prof'], { stock: 'weapons', prof: 'guns', hub: 'hub_highland' });
N('npc_haberdasher', 'Saul Hirsch', 'Haberdasher', 384, 188, 90, ['vendor', 'repair'], { stock: 'armor', hub: 'hub_highland' });
N('npc_auctioneer', 'Cornelius Vane', 'Auctioneer', 772, 211, 180, ['quest', 'auctioneer'], { hub: 'hub_vine' });
N('npc_auctioneer2', 'Miss Opal Reed', 'Auctioneer', 790, 211.5, -90, ['auctioneer'], { hub: 'hub_vine' });
N('npc_salvage_trainer', 'Ike Brandt', 'Salvaging Trainer', 742, 214, -90, ['quest', 'trainer_prof'], { prof: 'salvage', hub: 'hub_vine' });
N('npc_derby_cook', 'Gus Malloy', 'Brown Derby Kitchen', 768, 286, 180, ['quest', 'vendor'], { stock: 'food', hub: 'hub_vine' });
N('npc_druggist', 'Mrs. Ida Walsh', 'Druggist, WCTU', 730, 212.5, -90, ['quest'], { hub: 'hub_vine' });
N('npc_pantages_manager', 'Lloyd Carver', 'Pantages House Manager', 718, 188, 90, ['quest'], { hub: 'hub_vine' });
N('npc_taft_doorman', 'Oscar Bell', 'Taft Building Doorman', 768, 240, 180, ['quest'], { hub: 'hub_vine' });
N('npc_stagehand', 'Whitey Nolan', 'Pantages Stagehand', 664, 372, 0, ['quest'], { hub: 'hub_vine' });
N('npc_chief_davis', 'Chief James E. Davis', 'Los Angeles Police Department', 534, 268, 180, ['quest'], { real_person: true, died: 1949, hub: 'hub_police' });
N('npc_capt_hynes', 'Capt. William "Red" Hynes', 'LAPD Intelligence Squad', 534, 280, 180, ['quest'], { real_person: true, died: 1952, hub: 'hub_police' });
N('npc_kearney', 'Patrolman Joe Kearney', 'Quartermaster', 534, 292, 180, ['vendor', 'repair'], { stock: 'general', hub: 'hub_police' });
// Guards (flavour, stand at hubs)
[[58, 189], [136, 189], [300, 186], [300, 214], [748, 188], [748, 212], [528, 262]].forEach(([x, y], i) => N(`npc_guard_${i}`, 'Boulevard Patrolman', 'LAPD', x, y, 90, ['guard'], { color: [0.15, 0.2, 0.45] }));

// ---------- camps ----------
const camps = [];
let campN = 0;
const camp = (mob, count, x, y, r, o = {}) => camps.push({ id: `camp_${String(++campN).padStart(2, '0')}`, mob, count, x, y, r, ...o });
// Workers around the Chinese (non-aggressive), dogs and coyotes on Franklin
camp('mob_invader_worker', 5, 40, 120, 14); camp('mob_invader_worker', 5, 92, 88, 16); camp('mob_invader_worker', 4, 150, 110, 12); camp('mob_invader_worker', 4, 40, 60, 14);
camp('mob_feral_dog', 4, 210, 60, 18); camp('mob_feral_dog', 4, 300, 80, 16); camp('mob_feral_dog', 4, 350, 50, 16); camp('mob_feral_dog', 3, 120, 40, 14); camp('mob_feral_dog', 3, 260, -10, 16);
camp('mob_coyote', 4, 470, 80, 18); camp('mob_coyote', 4, 560, 50, 18); camp('mob_coyote', 4, 660, 80, 18); camp('mob_coyote', 4, 760, 40, 18); camp('mob_coyote', 3, 520, -30, 18); camp('mob_coyote', 3, 640, -40, 18);
camp('mob_old_scar', 1, 720, -50, 6);
camp('mob_invader_worker', 4, 420, 110, 12); camp('mob_invader_drone', 3, 800, 110, 12);
// Selma alleys: drones; Cahuenga: spitters; Taft back lots: looters
camp('mob_invader_drone', 4, 340, 280, 14); camp('mob_invader_drone', 4, 390, 340, 16); camp('mob_invader_drone', 4, 340, 380, 14); camp('mob_invader_drone', 3, 455, 340, 12); camp('mob_invader_drone', 3, 400, 280, 10);
camp('mob_selma_burrower', 1, 470, 380, 6);
camp('mob_invader_spitter', 3, 660, 330, 14); camp('mob_invader_spitter', 3, 710, 360, 14); camp('mob_invader_spitter', 3, 650, 395, 12); camp('mob_invader_spitter', 2, 570, 350, 12); camp('mob_invader_drone', 3, 580, 395, 12);
camp('mob_looter', 4, 800, 330, 14); camp('mob_looter', 4, 850, 370, 14); camp('mob_looter', 3, 800, 395, 12);
camp('mob_looter_ringleader', 1, 870, 340, 3);
// Hollywood High: drones at the gates, broodlings in the yard and gym, soldiers on the west field
camp('mob_invader_drone_hs', 4, 276, 380, 10); camp('mob_invader_drone_hs', 3, 276, 330, 10);
camp('mob_invader_broodling', 8, 228, 380, 14); camp('mob_invader_broodling', 7, 170, 360, 14); camp('mob_invader_broodling', 6, 228, 325, 10);
camp('mob_invader_soldier', 3, 100, 350, 14); camp('mob_invader_soldier', 3, 60, 395, 12); camp('mob_invader_soldier', 2, 140, 400, 10);
camp('mob_ballroom_brute', 1, 228, 136, 4);
// Exit guards (leash keeps them at the gap)
camp('mob_guard_cahuenga', 2, 298, -106, 4, { leash: 70 }); camp('mob_guard_gower', 2, 906, 200, 4, { leash: 70 });
camp('mob_guard_melrose', 2, 298, 556, 4, { leash: 70 }); camp('mob_guard_la_brea', 2, -106, 200, 4, { leash: 70 });
// Beyond the wall: patrols that sweep the roads (generous leash: they chase), and a few standing packs
const patrol = (mob, count, pts, o = {}) => camp(mob, count, pts[0][0], pts[0][1], 4, { leash: 140, patrol: pts, ...o });
patrol('mob_pass_soldier', 2, [[298, -150], [298, -250], [150, -230], [298, -250], [450, -230]]);
patrol('mob_pass_flier', 2, [[-200, -160], [100, -260], [500, -180], [900, -260], [1060, -150], [500, -280]]);
patrol('mob_wall_stalker', 3, [[960, 200], [1080, 200], [1080, 80], [960, 60], [960, 340], [1080, 450]]);
patrol('mob_gulch_coyote', 3, [[1000, 500], [1090, 300], [940, 420]]);
patrol('mob_melrose_brute', 2, [[298, 600], [298, 720], [480, 680], [298, 720], [120, 650]]);
patrol('mob_wall_stalker', 3, [[-40, 600], [300, 590], [700, 600], [1000, 640]]);
patrol('mob_wh_gunman', 3, [[-150, 200], [-280, 200], [-280, 360], [-150, 470], [-150, 60]]);
patrol('mob_wall_stalker', 2, [[-160, -60], [-280, 40], [-160, 520], [-280, 700]]);
camp('mob_pass_soldier', 3, 120, -290, 10); camp('mob_melrose_brute', 2, 420, 700, 8); camp('mob_wh_gunman', 3, -240, 330, 10); camp('mob_wall_stalker', 3, 1060, 280, 10);

// ---------- interactable objects ----------
const objects = [];
let objN = 0;
const obj = (kind, name, x, y, o = {}) => objects.push({ id: `obj_${String(++objN).padStart(3, '0')}`, kind, name, x, y, respawn: 60, ...o });
[[648, 274], [660, 281], [674, 288], [686, 294], [700, 270], [712, 277], [690, 262], [640, 290]].forEach(([x, y]) => obj('film_can', 'Film Can', x, y, { gives: 'Newsreel Film Can', quest: 'q_hb_06' }));
[[210, 188], [400, 188], [470, 212], [596, 188]].forEach(([x, y]) => obj('arc_lamp', 'Marquee Arc Lamp', x, y, { quest: 'q_hb_07', use: true, respawn: 45 }));
[[784, 300], [790, 302], [796, 300], [802, 296], [804, 288], [804, 280], [778, 302], [772, 300], [806, 272], [786, 306], [798, 306], [808, 300]].forEach(([x, y]) => obj('coffee_tin', 'Coffee Tin', x, y, { gives: 'Brown Derby Coffee Tin', quest: 'q_hb_13' }));
[[330, 400], [372, 392], [408, 404], [440, 396]].forEach(([x, y]) => obj('spire', 'Hive Spire', x, y, { quest: 'q_hb_16', use: true, respawn: 90, tall: true }));
[[180, 440], [300, 444], [420, 440], [540, 444], [660, 440], [60, 444], [760, 444]].forEach(([x, y]) => obj('callbox', 'Police Call Box', x, y, { gives: 'Call-box Key', quest: 'q_hb_20' }));
[[[300, 360], 1], [[140, 382], 2], [[330, 302], 3]].forEach(([[x, y], s]) => [[0, 0], [6, 4], [-4, 6]].forEach(([dx, dy]) => obj('spread_egg', 'Spreading Egg Cluster', x + dx, y + dy, { use: true, respawn: 0, spread: s })));
[[212, 360], [222, 372], [236, 364], [246, 378], [214, 388], [230, 394], [244, 392], [226, 356], [240, 356], [210, 398]].forEach(([x, y]) => obj('egg', 'Egg Cluster', x, y, { quest: 'q_hb_22', use: true, respawn: 90 }));
[[354, 338], [390, 370], [332, 394], [674, 350], [720, 382], [822, 350], [652, 274], [704, 270], [560, 330], [480, 330], [600, 280], [860, 300], [420, 300], [300, 330]].forEach(([x, y]) => obj('scrap', 'Wreckage', x + 1, y + 4, { gives: 'Scrap Metal', salvage: true, respawn: 120 }));
obj('mailbox', 'Mailbox', 354, 188, { respawn: 0 });
obj('redcar', 'Pacific Electric Red Car', 276, 200, { respawn: 0, big: true });

// ---------- props for the interactables (the renderer falls back to primitives without them) ----------
// kind -> (index, object) => [prop id, yaw, scale]
const objRand = rng(11);
const OBJ_PROPS = {
  // the art/world interactables (int_*) mix in with the older Rodin props
  film_can: (i) => (i % 3 === 2 ? ['int_film_cans', objRand() * 360, 1.3] : [i % 2 ? 'film_can_b' : 'film_can_a', objRand() * 360, 1.4]),
  reel: () => ['int_film_reel', objRand() * 360, 1.6],
  coffee_tin: (i) => (i % 3 === 0 ? ['int_coffee_tin', objRand() * 360, 1.5] : ['coffee_tin_a', objRand() * 360, 2.5]),
  arc_lamp: (i, o) => (i % 3 === 2 ? ['int_arc_lamp', o.y < 200 ? 90 : -90, 1.1] : [i % 2 ? 'arc_lamp_b' : 'arc_lamp_a', o.y < 200 ? 90 : -90, 1]),
  spire: () => ['hive_spire', objRand() * 360, [1.8, 1.8, 3.7]],
  egg: () => ['hive_eggs', objRand() * 360, 0.75],
  spread_egg: () => ['hive_eggs_pale', objRand() * 360, 0.7],
  boss_egg: () => ['hive_eggs', objRand() * 360, 0.9],
  egg_sac: () => ['hive_eggs_pale', 0, [1.6, 1.6, 1.9]],
  callbox: (i) => [['callbox', 'callbox_blue', 'int_callbox'][i % 3], -90, 1],
  scrap: (i) => (i % 2 ? ['int_scrap', objRand() * 360, 1.1] : ['scrap_pile', objRand() * 360, 0.8]),
  mailbox: () => ['int_mailbox', 90, 1],
  redcar: () => ['redcar_pe', 0, 1],
  usher: () => ['int_cocoon', objRand() * 360, 1.1],
};
function dressObjects(list) {
  const seen = {};
  for (const o of list) {
    const f = OBJ_PROPS[o.kind];
    if (!f) continue;
    const [id, yaw, s] = f((seen[o.kind] = (seen[o.kind] ?? -1) + 1), o);
    o.prop = placement(id, o.x, o.y, yaw, s);
  }
}
dressObjects(objects);

// ---------- dressing (no collision) ----------
const CROSS = [[10, 26], [140, 150], [290, 306], [420, 430], [520, 530], [610, 622], [750, 766]];
const onCross = (x, pad = 3) => CROSS.some(([a, b]) => x > a - pad && x < b + pad);
const nearNpc = (x, y, r = 4) => npcs.some((n) => Math.hypot(n.x - x, n.y - y) < r) || objects.some((o) => Math.hypot(o.x - x, o.y - y) < r);
// street lamps on both kerbs of the boulevard, trolley poles on the north kerb holding the red-car wire over the rails
for (let x = -96; x < 900; x += 36) {
  for (const y of [189.4, 210.6]) if (!onCross(x) && !nearNpc(x, y)) deco('street_lamp', x, y, 0);
  const px = x + 18;
  // the pole stands at the -x end of the mesh (bounds centred): shift it so the base is on the kerb and the arm reaches over the street
  if (!onCross(px) && !nearNpc(px, 189.4) && (px < 250 || px > 310)) deco('trolley_pole', px, 189.4 + 2.6, 90);
}
// fan palms: the boulevard's south sidewalk (between the lamps) and both sides of Highland
// (a canary palm, scaled down to fit the sidewalk, every third)
const palm = (x, y, i) => (i % 3 === 2 ? deco('palm_canary', x, y, objRand() * 360, 0.62 + objRand() * 0.12) : deco('palm_fan', x, y, objRand() * 360, 0.85 + objRand() * 0.3));
let palmN = 0;
for (let x = -96; x < 900; x += 36) if (!onCross(x + 18, 5) && !nearNpc(x + 18, 212.4, 6)) palm(x + 18, 212.4, palmN++);
for (let y = -80; y < 540; y += 30) for (const x of [288.6, 307.4]) if ((y < 180 || y > 222) && !nearNpc(x, y, 6)) palm(x, y, palmN++);
// the HOLLYWOODLAND sign on the north ridge (past the hive ring, on the hills HLTerrain raises there), facing the city
{
  const S = 2, GAP = 4, letters = 'HOLLYWOODLAND'.split('');
  const w = (c) => (PROPS_TABLE[`sign_${c.toLowerCase()}`].max[1] - PROPS_TABLE[`sign_${c.toLowerCase()}`].min[1]) * S;
  const total = letters.reduce((a, c) => a + w(c) + GAP, -GAP);
  let x = 300 - total / 2;
  letters.forEach((c, i) => { deco(`sign_${c.toLowerCase()}`, x + w(c) / 2, -700 + [0, 5, 2][i % 3], 90, S, -2); x += w(c) + GAP; });
}
// theatre marquees (hung from the front wall) and a theatre front on Vine
// (the Pantages, Grauman's and the Egyptian carry their own marquees: art/buildings sign cards)
deco('theatre_facade', 662, 214 + 1.2, -90);
// cars abandoned along the kerbs (solid: they are cover), the newsreel truck and a delivery van
[['car_tudor', 180, 192.4, 180], ['car_sedan_burgundy', 430, 207.6, 0], ['car_chevrolet', 560, 192.4, 180], ['car_chevrolet_blue', 650, 207.6, 0],
  ['car_ford_a', 860, 192.4, 180], ['car_model_t', -60, 207.6, 0], ['truck_newsreel', 600, 207.4, 0], ['van_delivery', 520, 192.6, 180], ['car_chevrolet', 30, 192.4, 180]]
  .forEach(([id, x, y, yaw]) => propBox(id, x, y, yaw, 1, C.wreck));
// barricades at the gaps in the hive wall
for (const [x, y, yaw] of [[284, -98, 90], [312, -98, 90], [284, 548, -90], [312, 548, -90], [-98, 186, 0], [-98, 214, 0], [898, 186, 180], [898, 214, 180]]) deco('sandbags', x, y, yaw);
// aid stations: triage tents
deco('tent_triage', 522, 326, 180); deco('tent_triage', 656, 290, 180); deco('tent_triage', 100, 450, 0);
// the ballroom: a fallen chandelier and wrecked furniture around the brute
deco('chandelier_rubble', 222, 130, 30);
[[204, 118, 20], [252, 120, 160], [208, 150, 75], [250, 150, 300], [238, 146, 200], [214, 134, 110]].forEach(([x, y, yaw]) => deco('ballroom_furniture', x, y, yaw));
deco('brick_rubble', 228, 112, 0, 1.4); deco('brick_rubble', 198, 140, 90, 1.2);
// the Brown Derby kitchen yard and the back lots
deco('coffee_sacks', 814, 284, 0); deco('coffee_sacks', 766, 306, 90); deco('crates', 812, 296, 0);
[[592, 300, 0], [700, 360, 90], [828, 330, 0], [372, 300, 90], [456, 360, 0], [612, 408, 90]].forEach(([x, y, yaw]) => deco('crates', x, y, yaw));
deco('shed_collapsed', 470, 400, 0); deco('wall_ruined', 600, 340, 90); deco('wall_ruined', 760, 400, 0); deco('pole_fallen', 452, 316, 20);
deco('brick_rubble', 642, 316, 40, 1.4); deco('brick_rubble', 846, 316, 0, 1.4); deco('brick_rubble', 316, 352, 120, 1.4);
deco('stair_wooden', 590, 146, 180); deco('stair_landing', 398, 254, 0);
// hive growth spreading from the walls and around the egg fields
for (const [x, y, id] of [[226, 372, 'hive_goo'], [300, 364, 'hive_goo'], [140, 388, 'hive_goo'], [334, 306, 'hive_goo'], [476, 254, 'hive_goo'],
  [-96, 120, 'hive_arch'], [-96, 290, 'hive_arch'], [896, 100, 'hive_arch'], [896, 330, 'hive_arch'], [120, -96, 'hive_spire'], [640, -96, 'hive_spire'], [480, 546, 'hive_spire'], [120, 546, 'hive_spire']])
  deco(id, x, y, objRand() * 360, id === 'hive_goo' ? 1.6 : id === 'hive_arch' ? 2.4 : [2, 2, 3]);

const zone = {
  id: 'zone_hollywood_blvd', name: 'Hollywood Boulevard', min: [OX0 - 10, OY0 - 10], size: [OX1 - OX0 + 20, OY1 - OY0 + 20], cell: 2, offset: [0, 0], sky: [0.42, 0.32, 0.38], floor: C.lot, floor_kind: 'Lot',
  boxes: boxes.splice(0), ground: ground.splice(0), cols: propCols(props), props: props.splice(0), subzones, npcs: npcs.splice(0), camps: camps.splice(0), objects: objects.splice(0),
  spawn: [92, 184], aid: [{ name: 'Hollywood Receiving Hospital', x: 500, y: 322 }, { name: 'Fire Station No. 27', x: 636, y: 284 }, { name: 'Blessed Sacrament Church', x: 125, y: 436 }],
  portals: [{ id: 'portal_egyptian', x: 475, y: 238, r: 3, to_area: 'dng_egyptian', to: [60, 8], label: 'The Egyptian Theatre (Dungeon, 8-12)' }],
  pois: { bank: [330, 191], auction: [776, 208], mailbox: [354, 190], redcar: [284, 198], trainers: [92, 184], inn: [96, 210], vendor: [372, 190], police: [530, 276], forecourt: [92, 188], vine: [756, 200] },
  // sound spots [sfx id, x, y, z]: radios playing 1920s records (tools/gen-sfx.mjs loop_radio) at the inn desk, the drugstore and the Derby kitchen
  sounds: [['loop_radio', 97, 216, 1.2], ['loop_radio', 731, 215, 1.2], ['loop_radio', 769, 289, 1.2]],
  exits: [{ id: 'exit_cahuenga', name: 'Highland north', to_zone: 'zone_cahuenga_pass', x: 298, y: OY0 + 6 }, { id: 'exit_gower', name: 'Hollywood Blvd east', to_zone: 'zone_gower_gulch', x: OX1 - 6, y: 200 },
    { id: 'exit_melrose', name: 'Highland south', to_zone: 'zone_melrose', x: 298, y: OY1 - 6 }, { id: 'exit_west', name: 'La Brea west', to_zone: 'zone_west_hollywood', x: OX0 + 6, y: 200 },
    { id: 'exit_redcar', name: 'Red car', to_zone: 'zone_east_hollywood', x: 276, y: 200, redcar: true }],
  idle_spots: [[92, 186], [80, 176], [108, 178], [330, 192], [338, 190], [322, 194], [776, 206], [784, 208], [760, 200], [96, 208], [120, 190], [300, 200], [372, 190], [354, 192], [70, 186], [745, 196]],
};

// ---------- The Egyptian Theatre (dungeon) ----------
const W = (x0, y0, x1, y1, h = 9) => box(x0, y0, x1, y1, h, C.interior);
W(16, -4, 20, 260); W(100, -4, 104, 260); W(16, -4, 104, 0); W(16, 256, 104, 260);          // outer walls
for (const y of [42, 86, 170, 204]) { W(20, y, 54, y + 4); W(66, y, 100, y + 4); }           // dividers with a 12 m door at x 54-66
// seat rows (line-of-sight cover): each box draws as six art/world seat rows facing the stage
const SEAT_S = [1.3, 1.08, 1.25];
for (let r = 0; r < 5; r++) for (const x0 of [26, 68]) {
  const y = 101.5 + r * 13, xs = [0, 1, 2, 3, 4, 5].map((k) => x0 + 2.17 + k * 4.33);
  box(x0, 100 + r * 13, x0 + 26, 103 + r * 13, 1.4, C.seat, '', { prop: placement('egy_seats', xs[0], y, 90, SEAT_S) });
  for (const x of xs.slice(1)) deco('egy_seats', x, y, 90, SEAT_S);
}
box(30, 236, 90, 252, 0.3, [0.3, 0.25, 0.2], 'Stage', { solid: false });
box(24, 4, 28, 36, 6, C.landmark); box(92, 4, 96, 36, 6, C.landmark);
const dcamp = (mob, count, x, y, r, o = {}) => camps.push({ id: `dcamp_${String(++campN).padStart(2, '0')}`, mob, count, x, y, r, ...o });
dcamp('mob_egy_drone', 3, 60, 24, 6); dcamp('mob_egy_broodling', 5, 40, 30, 5);
dcamp('mob_egy_drone', 2, 36, 56, 4); dcamp('mob_egy_spitter', 2, 84, 56, 4);
dcamp('mob_boss_brood_keeper', 1, 60, 74, 1);
dcamp('mob_egy_drone', 3, 60, 96, 4); dcamp('mob_egy_spitter', 2, 30, 130, 4); dcamp('mob_egy_soldier', 2, 88, 140, 4); dcamp('mob_egy_broodling', 6, 60, 128, 6);
dcamp('mob_boss_hive_warden', 1, 60, 158, 1);
dcamp('mob_egy_spitter', 2, 36, 182, 3); dcamp('mob_egy_soldier', 1, 80, 182, 2);
dcamp('mob_boss_matriarch', 1, 60, 194, 1);
dcamp('mob_egy_soldier', 2, 40, 216, 3); dcamp('mob_egy_drone', 2, 82, 216, 3);
dcamp('mob_boss_hive_mother', 1, 60, 242, 1);
[[30, 60], [90, 120], [28, 150], [90, 190]].forEach(([x, y]) => obj('usher', 'Captured Usher', x, y, { quest: 'q_hb_27', use: true, respawn: 0 }));
[[26, 176], [94, 176], [26, 198], [94, 198], [44, 198]].forEach(([x, y]) => obj('reel', 'Film Reel', x, y, { gives: 'Reel of Robin Hood', quest: 'q_hb_26', respawn: 0 }));
[[44, 66], [76, 66], [44, 80], [76, 80]].forEach(([x, y]) => obj('boss_egg', 'Egg Cluster', x, y, { use: true, respawn: 0, boss: 'mob_boss_brood_keeper' }));
obj('egg_sac', 'Egg Sac', 60, 250, { use: true, respawn: 0, boss: 'mob_boss_hive_mother' });
dressObjects(objects);
// the hive has taken the theatre: membranes hanging off the divider walls, goo, egg clutches and spires in the corners
for (const y of [42, 86, 170, 204]) for (const x of [30, 44, 76, 90]) deco('hive_membrane', x, y - 0.2, 90, [1, 2, 2], 9);
for (const [x, y] of [[36, 20], [84, 28], [30, 70], [88, 76], [60, 116], [34, 160], [86, 150], [60, 188], [36, 226], [84, 230]]) deco('hive_goo', x, y, objRand() * 360, 1.8);
for (const [x, y] of [[24, 40], [96, 40], [24, 84], [96, 84], [24, 168], [96, 168], [24, 254], [96, 254]]) deco('hive_spire', x, y, objRand() * 360, [1.4, 1.4, 2.4]);
for (const [x, y] of [[88, 12], [32, 52], [92, 196], [30, 214], [90, 246]]) deco('hive_eggs_pale', x, y, objRand() * 360, 0.8);
deco('crates', 26, 190, 0); deco('crates', 94, 186, 90); deco('film_canister', 30, 186, 0, 1.2); deco('chandelier_rubble', 36, 121, 0, 1.2); // between seat rows, clear of the centre aisle (its exact col snagged units walking the aisle)
deco('hive_tendrils', 50, 246, 0, 2); deco('hive_tendrils', 70, 246, 180, 2);
// the theatre itself (art/world Egyptian kit): sphinxes along the forecourt path, pharaohs at the pylons, the box office,
// lotus columns down the lobby, hieroglyph panels on the auditorium walls, the projector, the organ and the proscenium
deco('egy_sphinx', 47, 16, 0); deco('egy_sphinx', 73, 16, 180); deco('egy_sphinx', 47, 30, 0); deco('egy_sphinx', 73, 30, 180);
deco('egy_pharaoh', 32, 36, -90); deco('egy_pharaoh', 88, 36, -90);
deco('egy_booth', 78, 36, -90);
for (const y of [52, 64, 76]) { deco('egy_column', 23, y, 0); deco('egy_column', 97, y, 0); }
for (const y of [94, 112, 138, 162, 214, 228]) { deco('egy_wall', 20.7, y, 0); deco('egy_wall', 99.3, y, 180); }
deco('egy_projector', 46, 190, 90, 1.2);
deco('egy_organ', 84, 230, 90);
deco('egy_stage', 60, 252.6, -90, [1.25, 1.6, 1.2]);

const dungeon = {
  id: 'dng_egyptian', name: 'The Egyptian Theatre', min: [0, -10], size: [120, 280], cell: 2, offset: [3000, 0], sky: [0.05, 0.04, 0.06], floor: [0.2, 0.17, 0.15], floor_kind: 'Theatre', dungeon: true,
  boxes: boxes.splice(0), ground: [{ x: 54, y: 0, w: 12, d: 256, color: [0.45, 0.12, 0.12], kind: 'Theatre' }], subzones: [
    { name: 'Forecourt', x: 20, y: 0, w: 80, d: 42 }, { name: 'Lobby', x: 20, y: 46, w: 80, d: 40 }, { name: 'Auditorium', x: 20, y: 90, w: 80, d: 80 },
    { name: 'Projection Booth', x: 20, y: 174, w: 80, d: 30 }, { name: 'Stage', x: 20, y: 208, w: 80, d: 48 }],
  cols: propCols(props), props: props.splice(0), npcs: [], camps: camps.splice(0), objects: objects.splice(0), spawn: [60, 8], aid: [],
  portals: [{ id: 'portal_egyptian_exit', x: 60, y: 2.5, r: 2.5, to_area: 'zone_hollywood_blvd', to: [475, 226], label: 'Exit to Hollywood Boulevard' }],
  pois: {}, exits: [], idle_spots: [],
};

export const areas = [zone, dungeon];
