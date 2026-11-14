// Colour variants of base-colour textures. node variants.mjs <outRoot>
import sharp from 'sharp'; import fs from 'node:fs'; import path from 'node:path';
const root = process.argv[2];
const tint = async (name, variant, mul) => {
  const src = path.join(root, name, 'textures', `T_${name}_BaseColor.png`);
  const dst = path.join(root, name, 'textures', `T_${name}_${variant}_BaseColor.png`);
  await sharp(src).linear(mul, [0, 0, 0]).toFile(dst); console.log('variant', path.basename(dst));
};
// mobs.json colours relative to the family base colour
await tint('InvaderSoldier', 'Hardened', [0.15 / 0.2, 0.2 / 0.3, 0.1 / 0.15]);          // mob_guard_cahuenga, mob_pass_soldier
await tint('InvaderSoldier', 'WallStalker', [0.18 / 0.2 * 0.8, 0.12 / 0.3 * 0.8, 0.22 / 0.15 * 0.8]); // mob_wall_stalker
