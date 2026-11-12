// Classes and abilities to level 12 (DESIGN-BIBLE §5). Effects are data; the client executes them generically.
// Effect types: damage{weapon,base,coef} dot{base,coef,ticks,period} heal{base,coef} hot{base,coef,ticks,period}
// shield{base,coef,dur} stun{dur} slow{pct,dur} taunt{dur} buff{stat,value,dur} (stat: dmg_pct|armor_pct|dodge|crit|taken_pct)
// coef scales with: melee/ranged attack power /14 for physical classes, 0.5*Int for casters (ability bonus).
// target: enemy | self | friend (self if no friendly target) | party. radius>0 = area around the target (or self).
export const classes = [
  { id: 'veteran', name: 'Veteran', resource: 'adrenaline', hp_per_level: 22, primary: 'str', secondary: 'sta', armour: 'steel', role: 'tank', ranged: false, caster: false,
    color: [0.55, 0.42, 0.25], weapons: ['knife', 'club', 'tool', 'sabre', 'heavy', 'shotgun', 'shield'], specs: ['Sergeant', 'Infantry', 'Brawler'], start: ['base_trench_shovel'] },
  { id: 'stuntman', name: 'Stuntman', resource: 'energy', hp_per_level: 17, primary: 'agi', secondary: 'str', armour: 'leather', role: 'melee', ranged: false, caster: false,
    color: [0.85, 0.75, 0.2], weapons: ['knife', 'club', 'fist', 'pistol'], specs: ['Fighter', 'Knife Work', 'Falls'], start: ['base_pocketknife'] },
  { id: 'gunman', name: 'Gunman', resource: 'focus', hp_per_level: 17, primary: 'agi', secondary: 'sta', armour: 'padded', role: 'ranged', ranged: true, caster: false,
    color: [0.35, 0.6, 0.3], weapons: ['knife', 'rifle', 'shotgun', 'revolver', 'pistol', 'smg'], specs: ['Marksman', 'Trick Shot', 'Hunter'], start: ['base_pocketknife', 'base_colt_police_positive'] },
  { id: 'electrician', name: 'Electrician', resource: 'power', hp_per_level: 12, primary: 'int', secondary: 'spi', armour: 'cloth', role: 'caster', ranged: false, caster: true,
    color: [0.3, 0.6, 0.95], weapons: ['tool', 'lamp'], specs: ['Arc', 'Floodlight', 'Generator'], start: ['base_pipe_wrench'] },
  { id: 'powderman', name: 'Powder Man', resource: 'power', hp_per_level: 12, primary: 'int', secondary: 'sta', armour: 'cloth', role: 'caster', ranged: false, caster: true,
    color: [0.9, 0.4, 0.15], weapons: ['tool', 'lamp'], specs: ['Fire', 'Smoke', 'Demolition'], start: ['base_pipe_wrench'] },
  { id: 'nurse', name: 'Nurse', resource: 'power', hp_per_level: 13, primary: 'int', secondary: 'spi', armour: 'cloth', role: 'healer', ranged: false, caster: true,
    color: [0.95, 0.95, 0.95], weapons: ['knife', 'club', 'bag'], specs: ['Triage', 'Medicine', 'Chemistry'], start: ['base_pocketknife'] },
  { id: 'preacher', name: 'Preacher', resource: 'power', hp_per_level: 14, primary: 'spi', secondary: 'int', armour: 'leather', role: 'healer', ranged: false, caster: true,
    color: [0.25, 0.2, 0.3], weapons: ['club', 'bible'], specs: ['Gospel', 'Temperance', 'Hellfire'], start: ['base_cane'] },
  { id: 'mechanic', name: 'Mechanic', resource: 'power', hp_per_level: 19, primary: 'str', secondary: 'int', armour: 'padded', role: 'melee', ranged: false, caster: false,
    color: [0.45, 0.5, 0.6], weapons: ['tool', 'heavy', 'shotgun'], specs: ['Gadgets', 'Heavy Tools', 'Repair'], start: ['base_pipe_wrench'] },
];

const A = (cls, level, id, name, o) => ({ id: `ab_${id}`, name, class: cls, level, cost: 0, range: 5, cast: 0, cd: 0, gcd: true, target: 'enemy', radius: 0, school: 'physical', effects: [], desc: '', ...o });
const dmg = (weapon, base, coef = 1) => ({ type: 'damage', weapon, base, coef });
const dot = (base, ticks, period, coef = 0.2) => ({ type: 'dot', base, coef, ticks, period });
const heal = (base, coef = 1) => ({ type: 'heal', base, coef });
const hot = (base, ticks, period, coef = 0.2) => ({ type: 'hot', base, coef, ticks, period });
const shield = (base, dur, coef = 1) => ({ type: 'shield', base, coef, dur });
const stun = (dur) => ({ type: 'stun', dur });
const slow = (pct, dur) => ({ type: 'slow', pct, dur });
const buff = (stat, value, dur) => ({ type: 'buff', stat, value, dur });

export const abilities = [
  // Veteran: adrenaline 0-100, built by damage dealt and taken.
  A('veteran', 1, 'vet_thrust', 'Bayonet Thrust', { cost: 15, effects: [dmg(1, 6)], desc: 'A hard thrust: weapon damage plus 6.' }),
  A('veteran', 2, 'vet_shout', 'Sergeant\'s Bark', { cost: 0, cd: 30, target: 'self', effects: [buff('dmg_pct', 0.1, 20), { type: 'resource', value: 20 }], desc: 'Gain 20 Adrenaline and deal 10% more damage for 20 s.' }),
  A('veteran', 4, 'vet_hold', 'Hold the Line', { cost: 10, cd: 10, effects: [{ type: 'taunt', dur: 4 }, { type: 'threat', value: 120 }, dmg(0.5, 4)], desc: 'Force the target to attack you for 4 s. High threat.' }),
  A('veteran', 6, 'vet_sweep', 'Trench Sweep', { cost: 25, cd: 6, radius: 6, effects: [dmg(0.8, 5)], desc: 'Strike every enemy within 6 m of your target.' }),
  A('veteran', 8, 'vet_butt', 'Rifle Butt', { cost: 10, cd: 20, effects: [dmg(0.4, 3), stun(3)], desc: 'Stun the target for 3 s.' }),
  A('veteran', 10, 'vet_dig_in', 'Dig In', { cost: 0, cd: 60, target: 'self', effects: [buff('taken_pct', -0.4, 10)], desc: 'Take 40% less damage for 10 s.' }),
  A('veteran', 12, 'vet_over_top', 'Over the Top', { cost: 35, cd: 15, effects: [dmg(1.6, 14)], desc: 'A finishing blow: 160% weapon damage plus 14.' }),
  // Stuntman: energy 100, +10/s.
  A('stuntman', 1, 'stn_haymaker', 'Haymaker', { cost: 40, effects: [dmg(1, 6)], desc: 'A wild swing: weapon damage plus 6.' }),
  A('stuntman', 2, 'stn_tumble', 'Tumble', { cost: 20, cd: 20, target: 'self', effects: [buff('dodge', 0.5, 6)], desc: '+50% dodge for 6 s.' }),
  A('stuntman', 4, 'stn_low_blow', 'Low Blow', { cost: 25, cd: 25, effects: [dmg(0.4, 3), stun(3)], desc: 'Stun the target for 3 s.' }),
  A('stuntman', 6, 'stn_flurry', 'Knife Flurry', { cost: 50, cd: 8, effects: [dmg(1.7, 8)], desc: 'Three fast cuts: 170% weapon damage plus 8.' }),
  A('stuntman', 8, 'stn_bottle', 'Breakaway Bottle', { cost: 30, cd: 12, range: 20, effects: [dmg(0.6, 10), slow(0.5, 6)], desc: 'Throw a bottle: damage and a 50% slow for 6 s.' }),
  A('stuntman', 10, 'stn_leap', 'Death-Defying Leap', { cost: 45, cd: 20, target: 'self', radius: 6, effects: [dmg(0.9, 10)], desc: 'Hit every enemy within 6 m of you.' }),
  A('stuntman', 12, 'stn_showstopper', 'Showstopper', { cost: 60, cd: 30, effects: [dmg(2.2, 18)], desc: '220% weapon damage plus 18.' }),
  // Gunman: focus 100, +5/s. Ranged 30 m.
  A('gunman', 1, 'gun_aimed', 'Aimed Shot', { cost: 30, range: 30, cast: 1.5, effects: [dmg(1.5, 8)], desc: 'A careful shot: 150% weapon damage plus 8.' }),
  A('gunman', 2, 'gun_hip', 'Hip Fire', { cost: 20, range: 30, cd: 6, effects: [dmg(1, 4)], desc: 'An instant shot.' }),
  A('gunman', 4, 'gun_knee', 'Knee Shot', { cost: 15, range: 30, cd: 10, effects: [dmg(0.5, 3), slow(0.5, 6)], desc: 'Slow the target by 50% for 6 s.' }),
  A('gunman', 6, 'gun_fan', 'Fan the Hammer', { cost: 45, range: 20, cd: 12, effects: [dmg(2, 10)], desc: 'Empty the cylinder: 200% weapon damage plus 10.' }),
  A('gunman', 8, 'gun_buckshot', 'Buckshot', { cost: 35, range: 15, cd: 8, radius: 6, effects: [dmg(0.9, 8)], desc: 'Hit every enemy within 6 m of the target.' }),
  A('gunman', 10, 'gun_nerves', 'Steady Nerves', { cost: 0, cd: 60, target: 'self', effects: [buff('crit', 0.25, 12)], desc: '+25% critical chance for 12 s.' }),
  A('gunman', 12, 'gun_rights', 'Dead to Rights', { cost: 50, range: 30, cast: 2, cd: 30, effects: [dmg(2.6, 20)], desc: '260% weapon damage plus 20.' }),
  // Electrician
  A('electrician', 1, 'elc_arc', 'Arc', { cost: 14, range: 30, cast: 1.5, school: 'electric', effects: [dmg(0, 14)], desc: 'A jolt of current.' }),
  A('electrician', 2, 'elc_insulate', 'Insulate', { cost: 20, cd: 20, target: 'friend', school: 'electric', effects: [shield(30, 20)], desc: 'A shield that absorbs damage for 20 s.' }),
  A('electrician', 4, 'elc_live_wire', 'Live Wire', { cost: 18, range: 30, school: 'electric', effects: [dot(7, 4, 3)], desc: 'Shocks the target every 3 s for 12 s.' }),
  A('electrician', 6, 'elc_floodlight', 'Floodlight', { cost: 30, range: 30, cast: 2, radius: 8, school: 'electric', effects: [dmg(0, 16, 0.6)], desc: 'Burn every enemy within 8 m of the target.' }),
  A('electrician', 8, 'elc_short', 'Short Circuit', { cost: 22, range: 30, cd: 30, school: 'electric', effects: [dmg(0, 8, 0.3), stun(3)], desc: 'Stun the target for 3 s.' }),
  A('electrician', 10, 'elc_overload', 'Overload', { cost: 40, range: 30, cast: 2.5, school: 'electric', effects: [dmg(0, 46, 1.4)], desc: 'A slow, heavy discharge.' }),
  A('electrician', 12, 'elc_chain', 'Chain Lightning', { cost: 45, range: 30, cast: 2, cd: 10, radius: 10, school: 'electric', effects: [dmg(0, 30, 0.8)], desc: 'Lightning jumps between enemies within 10 m of the target.' }),
  // Powder Man
  A('powderman', 1, 'pow_flash', 'Flash Powder', { cost: 14, range: 30, cast: 1.5, school: 'fire', effects: [dmg(0, 13)], desc: 'A burst of flash powder.' }),
  A('powderman', 2, 'pow_fuse', 'Slow Fuse', { cost: 16, range: 30, school: 'fire', effects: [dot(6, 5, 3)], desc: 'The target burns every 3 s for 15 s.' }),
  A('powderman', 4, 'pow_smoke', 'Smoke Pot', { cost: 20, range: 30, cd: 15, radius: 8, school: 'fire', effects: [slow(0.5, 8)], desc: 'Slow every enemy within 8 m of the target by 50% for 8 s.' }),
  A('powderman', 6, 'pow_squib', 'Squib', { cost: 18, range: 30, cd: 8, school: 'fire', effects: [dmg(0, 20, 0.7)], desc: 'An instant charge.' }),
  A('powderman', 8, 'pow_keg', 'Powder Keg', { cost: 36, range: 30, cast: 2, radius: 8, school: 'fire', effects: [dmg(0, 22, 0.7)], desc: 'Blast every enemy within 8 m of the target.' }),
  A('powderman', 10, 'pow_screen', 'Smokescreen', { cost: 20, cd: 45, target: 'self', effects: [buff('dodge', 0.5, 8)], desc: '+50% dodge for 8 s.' }),
  A('powderman', 12, 'pow_demo', 'Demolition Charge', { cost: 50, range: 30, cast: 3, cd: 15, school: 'fire', effects: [dmg(0, 70, 1.8)], desc: 'A very large charge.' }),
  // Nurse
  A('nurse', 1, 'nur_ether', 'Ether Rag', { cost: 12, range: 30, cast: 1.5, school: 'acid', effects: [dmg(0, 11)], desc: 'Chemical damage.' }),
  A('nurse', 1, 'nur_dress', 'Dress Wound', { cost: 18, range: 40, cast: 2, target: 'friend', school: 'heal', effects: [heal(24)], desc: 'Heal a friendly target.' }),
  A('nurse', 4, 'nur_tonic', 'Tonic', { cost: 16, range: 40, target: 'friend', school: 'heal', effects: [hot(8, 5, 3)], desc: 'Heals every 3 s for 15 s.' }),
  A('nurse', 6, 'nur_salts', 'Smelling Salts', { cost: 20, range: 40, cd: 15, target: 'friend', school: 'heal', effects: [heal(34, 0.8)], desc: 'An instant heal.' }),
  A('nurse', 8, 'nur_iodine', 'Iodine Splash', { cost: 16, range: 30, school: 'acid', effects: [dot(8, 5, 3)], desc: 'Burns every 3 s for 15 s.' }),
  A('nurse', 10, 'nur_surgery', 'Field Surgery', { cost: 42, range: 40, cast: 2.5, target: 'friend', school: 'heal', effects: [heal(80, 1.6)], desc: 'A large heal.' }),
  A('nurse', 12, 'nur_sedative', 'Sedative', { cost: 25, range: 30, cd: 30, school: 'acid', effects: [stun(5)], desc: 'Stun the target for 5 s.' }),
  // Preacher
  A('preacher', 1, 'pre_rebuke', 'Rebuke', { cost: 12, range: 30, cast: 1.5, school: 'holy', effects: [dmg(0, 12)], desc: 'Holy damage.' }),
  A('preacher', 1, 'pre_hands', 'Lay On Hands', { cost: 18, range: 40, cast: 2, target: 'friend', school: 'heal', effects: [heal(22)], desc: 'Heal a friendly target.' }),
  A('preacher', 4, 'pre_blessing', 'Blessing', { cost: 20, range: 40, cd: 12, target: 'friend', school: 'heal', effects: [shield(36, 20)], desc: 'A shield that absorbs damage for 20 s.' }),
  A('preacher', 6, 'pre_sermon', 'Hellfire Sermon', { cost: 18, range: 30, school: 'holy', effects: [dot(8, 5, 3)], desc: 'Burns every 3 s for 15 s.' }),
  A('preacher', 8, 'pre_hymn', 'Hymn', { cost: 40, cast: 2.5, target: 'party', school: 'heal', effects: [heal(30, 0.7)], desc: 'Heal every party member within 30 m.' }),
  A('preacher', 10, 'pre_fear', 'Fear of God', { cost: 22, range: 30, cd: 30, school: 'holy', effects: [stun(4)], desc: 'Stun the target for 4 s.' }),
  A('preacher', 12, 'pre_revival', 'Revival', { cost: 45, range: 40, cast: 2.5, target: 'friend', school: 'heal', effects: [heal(85, 1.6)], desc: 'A large heal.' }),
  // Mechanic
  A('mechanic', 1, 'mec_swing', 'Wrench Swing', { cost: 8, effects: [dmg(1, 6)], desc: 'Weapon damage plus 6.' }),
  A('mechanic', 2, 'mec_patch', 'Patch Kit', { cost: 16, range: 40, cd: 8, target: 'friend', school: 'heal', effects: [hot(7, 5, 3, 0.4)], desc: 'Heals every 3 s for 15 s.' }),
  A('mechanic', 4, 'mec_caltrops', 'Caltrops', { cost: 14, cd: 15, target: 'self', radius: 8, effects: [dmg(0.2, 3), slow(0.5, 8)], desc: 'Slow every enemy within 8 m by 50% for 8 s.' }),
  A('mechanic', 6, 'mec_monkey', 'Monkey Wrench', { cost: 14, cd: 20, effects: [dmg(0.5, 4), stun(2.5)], desc: 'Stun the target for 2.5 s.' }),
  A('mechanic', 8, 'mec_welder', 'Arc Welder', { cost: 24, cd: 8, radius: 5, school: 'electric', effects: [dmg(0.8, 12)], desc: 'Burn every enemy within 5 m of the target.' }),
  A('mechanic', 10, 'mec_overhaul', 'Overhaul', { cost: 30, cd: 45, target: 'self', school: 'heal', effects: [heal(40, 1), buff('armor_pct', 0.5, 15)], desc: 'Heal yourself and gain 50% armour for 15 s.' }),
  A('mechanic', 12, 'mec_pile', 'Pile Driver', { cost: 30, cd: 15, effects: [dmg(1.8, 16)], desc: '180% weapon damage plus 16.' }),
  // Mob abilities (class "mob")
  A('mob', 1, 'mob_acid_spit', 'Acid Spit', { range: 30, cast: 1.5, school: 'acid', effects: [dmg(1, 0), { type: 'dot', base: 0, coef: 0, weapon: 0.2, ticks: 3, period: 2 }], desc: 'Acid: a hit, then 3 x 20% of a hit over 6 s.' }),
  A('mob', 1, 'mob_knockback', 'Knockback', { cd: 10, effects: [dmg(1.5, 0), stun(1.5)], desc: 'A heavy blow that staggers.' }),
  A('mob', 1, 'mob_charge', 'Charge', { cd: 15, range: 30, effects: [dmg(1.8, 0), stun(2)], desc: 'Charges a random party member.' }),
  A('mob', 1, 'mob_acid_pool', 'Acid Pool', { cd: 12, range: 40, radius: 5, school: 'acid', effects: [dmg(1.2, 0)], desc: 'Acid splashes everyone near the target.' }),
  A('mob', 1, 'mob_pistol', 'Pistol Shot', { range: 25, cast: 1.5, effects: [dmg(1, 0)], desc: 'A pistol shot.' }),
];
