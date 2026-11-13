// Item generator (DESIGN-BIBLE §8): bases x 13 level bands x quality x suffix. Every number comes from the §8 formulas.
// Row: {id,name,kind,slot,type,q,req,ilvl,stats,armor,dmin,dmax,speed,hands,price,stack,use,blurb,built}
export const BANDS = [1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60];
export const QUALITY = ['poor', 'common', 'uncommon', 'rare', 'epic'];
const slug = (s) => s.toLowerCase().replace(/&/g, 'and').replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '');
const SLOT_FACTOR = { hat: 1, coat: 1, trousers: 1, gloves: 0.75, belt: 0.75, shoes: 0.75, wrist: 0.55, neck: 0.55, ring: 0.55, off: 0.55, main1: 0.45, main2: 1, ranged: 0.35 };
const Q_FACTOR = [0, 0, 0.5, 0.65, 0.8];
const ARMOUR_MULT = { cloth: 1, leather: 2, padded: 4, steel: 7 };
const ARMOUR_Q = [1, 1, 1.2, 1.4, 1.6];
const DPS_Q = [0.8, 0.9, 1, 1.12, 1.25];
const PRICE_Q = [0.5, 1, 2, 5, 12];

export const SUFFIXES = {
  'of Strength': { str: 1 }, 'of Agility': { agi: 1 }, 'of Stamina': { sta: 1 }, 'of Intellect': { int: 1 }, 'of Spirit': { spi: 1 },
  'of the Boxer': { str: 0.6, sta: 0.4 }, 'of the Acrobat': { agi: 0.6, sta: 0.4 }, 'of the Stevedore': { str: 0.6, agi: 0.4 },
  'of the Marksman': { agi: 0.6, int: 0.4 }, 'of the Engineer': { int: 0.6, sta: 0.4 }, 'of the Doctor': { int: 0.6, spi: 0.4 },
  'of the Deacon': { spi: 0.6, sta: 0.4 },
};
const SUFFIX_FOR = {
  cloth: ['of Intellect', 'of Spirit', 'of Stamina', 'of the Engineer', 'of the Doctor', 'of the Deacon'],
  leather: ['of Agility', 'of Stamina', 'of Spirit', 'of the Acrobat', 'of the Stevedore', 'of the Deacon'],
  padded: ['of Agility', 'of Strength', 'of Stamina', 'of the Boxer', 'of the Marksman', 'of the Acrobat', 'of the Engineer'],
  steel: ['of Strength', 'of Stamina', 'of the Boxer', 'of the Stevedore'],
  jewel: Object.keys(SUFFIXES),
  str: ['of Strength', 'of Stamina', 'of the Boxer', 'of the Stevedore'],
  agi: ['of Agility', 'of Stamina', 'of the Acrobat', 'of the Marksman', 'of the Stevedore'],
  int: ['of Intellect', 'of Spirit', 'of the Engineer', 'of the Doctor', 'of the Deacon'],
};
const CONDITION = ['Worn', 'Patched', 'Plain', 'Sturdy', 'Good', 'Fine', 'Pressed', 'Tailored', 'Heavy', 'Reinforced', 'Superior', 'Chitin-lined', 'Chitin-plated'];

// Armour bases: slot -> armour type -> names by tier (spread across the 13 bands). Real brands existed by 1930.
const ARMOUR = {
  hat: { cloth: ['Newsboy Cap', 'Felt Fedora', 'Stetson Fedora', 'Silk Top Hat'], leather: ['Leather Flying Cap', 'Stetson Boss of the Plains', 'Stunt Rider\'s Helmet'], padded: ['Canvas Work Cap', 'Football Helmet', 'Quilted Aviator Helmet'], steel: ['M1917 Helmet', 'Riveted Steel Helmet', 'Chitin-crested Helmet'] },
  coat: { cloth: ['Arrow Shirt and Vest', 'Wool Sack Suit Coat', 'Hart Schaffner & Marx Suit Coat', 'Opening Night Tailcoat'], leather: ['Work Jacket', 'Horsehide Jacket', 'Pendleton-lined Riding Coat'], padded: ['Carhartt Chore Coat', 'Quilted Vest', 'Football Shoulder Pads'], steel: ['Steel Vest', 'Plate-insert Trench Coat', 'Chitin-plated Cuirass'] },
  trousers: { cloth: ['Wool Trousers', 'Pleated Flannel Trousers', 'Striped Morning Trousers'], leather: ['Leather Chaps', 'Riding Breeches', 'Stunt Rider\'s Leggings'], padded: ['Levi Strauss Waist Overalls', 'Canvas Work Trousers', 'Quilted Leggings'], steel: ['Steel-kneed Breeches', 'Plate-insert Trousers', 'Chitin Greaves'] },
  shoes: { cloth: ['Canvas Plimsolls', 'Florsheim Oxfords', 'Patent Leather Pumps'], leather: ['Work Boots', 'Riding Boots', 'Cavalry Boots'], padded: ['Hobnail Boots', 'Lineman\'s Boots', 'Logger Boots'], steel: ['Steel-toed Boots', 'Trench Boots', 'Chitin-shod Boots'] },
  gloves: { cloth: ['Cotton Gloves', 'Kid Gloves', 'Opera Gloves'], leather: ['Leather Gloves', 'Riding Gauntlets', 'Everlast Bag Gloves'], padded: ['Canvas Work Gloves', 'Lineman\'s Gloves', 'Welder\'s Gauntlets'], steel: ['Mail-backed Gloves', 'Plated Gauntlets'] },
  belt: { cloth: ['Braces', 'Silk Sash', 'Cummerbund'], leather: ['Leather Belt', 'Gun Belt', 'Sam Browne Belt'], padded: ['Canvas Web Belt', 'Tool Belt', 'Lineman\'s Belt'], steel: ['Plated Belt', 'Cartridge Belt'] },
  wrist: { cloth: ['Shirt Cuffs', 'French Cuffs', 'Sleeve Garters'], leather: ['Leather Wrist Straps', 'Roping Cuffs'], padded: ['Canvas Wrist Wraps', 'Quilted Cuffs'], steel: ['Steel Bracers', 'Chitin Bracers'] },
};
const JEWEL = { neck: ['Brass Locket', 'Silver Chain', 'St. Christopher Medal', 'Pearl Choker', 'Studio Pass on a Chain'], ring: ['Brass Ring', 'Signet Ring', 'Lodge Ring', 'Wedding Band', 'Diamond Pinky Ring'] };

// Weapons: type -> {slot, hands, speed, stat family, names by tier}
const WEAPONS = {
  knife: { slot: 'main', hands: 1, speed: 1.7, fam: 'agi', names: ['Pocketknife', 'Straight Razor', 'Hunting Knife', 'M1905 Bayonet', 'Trench Knife'] },
  fist: { slot: 'main', hands: 1, speed: 1.5, fam: 'agi', names: ['Taped Knuckles', 'Brass Knuckles', 'Weighted Gloves'] },
  club: { slot: 'main', hands: 1, speed: 2.4, fam: 'str', names: ['Cane', 'Blackjack', 'Police Nightstick', 'Louisville Slugger'] },
  tool: { slot: 'main', hands: 1, speed: 2.6, fam: 'int', names: ['Pipe Wrench', 'Crowbar', 'Hatchet', 'Lineman\'s Wrench'] },
  sabre: { slot: 'main', hands: 1, speed: 2.4, fam: 'str', names: ['Prop Fencing Sabre', 'M1913 Cavalry Sabre'] },
  heavy: { slot: 'main', hands: 2, speed: 3.5, fam: 'str', names: ['Trench Shovel', 'Pickaxe', 'Fire Axe', 'Sledgehammer'] },
  revolver: { slot: 'ranged', hands: 1, speed: 2.2, fam: 'agi', names: ['Colt Police Positive', 'Smith & Wesson .38 Military & Police', 'Colt Single Action Army'] },
  pistol: { slot: 'ranged', hands: 1, speed: 2.0, fam: 'agi', names: ['Colt M1911'] },
  rifle: { slot: 'ranged', hands: 2, speed: 3.0, fam: 'agi', names: ['Winchester Model 1894', 'Remington Model 8', 'Springfield M1903'] },
  shotgun: { slot: 'ranged', hands: 2, speed: 3.2, fam: 'agi', names: ['Winchester Model 1897', 'Remington Model 11'] },
  smg: { slot: 'ranged', hands: 2, speed: 1.6, fam: 'agi', names: ['Thompson M1928'], minBand: 6 },
  shield: { slot: 'off', hands: 1, speed: 0, fam: 'str', names: ['Trash Can Lid', 'Riot Shield'] },
  lamp: { slot: 'off', hands: 1, speed: 0, fam: 'int', names: ['Work Lamp', 'Carbide Lamp', 'Arc Lamp'] },
  bag: { slot: 'off', hands: 1, speed: 0, fam: 'int', names: ['Medical Bag', 'Johnson & Johnson First Aid Kit'] },
  bible: { slot: 'off', hands: 1, speed: 0, fam: 'int', names: ['Pocket Testament', 'Family Bible'] },
};

function statsFor(points, split) {
  const out = {};
  let left = points;
  const keys = Object.keys(split);
  keys.forEach((k, i) => {
    const v = i === keys.length - 1 ? left : Math.round(points * split[k]);
    if (v > 0) out[k] = v;
    left -= v;
  });
  return out;
}
const price = (ilvl, q) => Math.max(1, Math.round(ilvl * ilvl / 10 * PRICE_Q[q]));
const tierName = (names, band, minBand = 0) => names[Math.min(names.length - 1, Math.floor(Math.max(0, band - minBand) / (13 - minBand) * names.length))];

function armourRow(name, slot, type, q, req, split, extra = {}) {
  const ilvl = q >= 2 ? req + 5 : req;
  const sf = SLOT_FACTOR[slot];
  const row = { id: '', name, kind: 'armor', slot, type, q, req, ilvl, stats: q >= 2 ? statsFor(Math.round(ilvl * Q_FACTOR[q] * sf), split) : {},
    armor: type === 'jewel' ? 0 : Math.round(ilvl * ARMOUR_MULT[type] * sf * ARMOUR_Q[q]), price: price(ilvl, q), built: req <= 12, ...extra };
  return row;
}
function weaponRow(name, type, q, req, split, extra = {}) {
  const w = WEAPONS[type];
  const ilvl = q >= 2 ? req + 5 : req;
  const sf = w.slot === 'ranged' ? SLOT_FACTOR.ranged : w.slot === 'off' ? SLOT_FACTOR.off : w.hands === 2 ? SLOT_FACTOR.main2 : SLOT_FACTOR.main1;
  const row = { id: '', name, kind: 'weapon', slot: w.slot, type, q, req, ilvl, hands: w.hands, stats: q >= 2 ? statsFor(Math.round(ilvl * Q_FACTOR[q] * sf), split) : {},
    armor: type === 'shield' ? Math.round(ilvl * 6 * ARMOUR_Q[q]) : 0, price: price(ilvl, q), built: req <= 12, ...extra };
  if (w.speed > 0) {
    const dps = (1.5 + 0.55 * ilvl) * DPS_Q[q] * (w.slot === 'ranged' ? 0.8 : w.hands === 2 ? 1.3 : 1);
    row.speed = w.speed;
    row.dmin = Math.max(1, Math.round(dps * w.speed * 0.8));
    row.dmax = Math.max(2, Math.round(dps * w.speed * 1.2));
  }
  return row;
}

// Named blues and epics (after real places or people dead before Nov 1956). [name, slot, type, band index, quality, split]
const NAMED = [
  // Egyptian Theatre loot (band 2 = req 10, ilvl 15) — built
  ["Grauman's Usher Coat", 'coat', 'cloth', 2, 3, { int: 0.6, spi: 0.4 }], ['Brood Keeper\'s Carapace Vest', 'coat', 'padded', 2, 3, { str: 0.5, sta: 0.5 }],
  ['Projectionist\'s Eyeshade', 'hat', 'cloth', 2, 3, { int: 0.6, sta: 0.4 }], ['Warden\'s Chitin Helm', 'hat', 'steel', 2, 3, { str: 0.5, sta: 0.5 }],
  ['Matriarch\'s Acid-etched Gloves', 'gloves', 'leather', 2, 3, { agi: 0.6, sta: 0.4 }], ['Stagehand\'s Rigging Gloves', 'gloves', 'padded', 2, 3, { agi: 0.6, str: 0.4 }],
  ['Egyptian Court Sandals', 'shoes', 'cloth', 2, 3, { spi: 0.6, int: 0.4 }], ['Hive Mother\'s Girdle', 'belt', 'leather', 2, 3, { agi: 0.5, sta: 0.5 }],
  ['Robin Hood\'s Prop Tights', 'trousers', 'leather', 2, 3, { agi: 0.6, str: 0.4 }], ['Orchestra Pit Trousers', 'trousers', 'cloth', 2, 3, { int: 0.5, sta: 0.5 }],
  ['Sid\'s Opening Night Cufflinks', 'wrist', 'cloth', 2, 3, { int: 0.5, spi: 0.5 }], ['Scarab Signet', 'ring', 'jewel', 2, 3, { sta: 0.5, str: 0.5 }],
  ['Pharaoh\'s Paste Necklace', 'neck', 'jewel', 2, 3, { int: 0.5, sta: 0.5 }], ['Usher\'s Brass Whistle Chain', 'neck', 'jewel', 2, 3, { agi: 0.5, sta: 0.5 }],
  // Elsewhere (data only)
  ['Crypt Keeper\'s Shroud', 'coat', 'cloth', 3, 3, { int: 0.6, spi: 0.4 }], ['Sexton\'s Mud Boots', 'shoes', 'leather', 3, 3, { agi: 0.5, sta: 0.5 }],
  ['Warner Grip\'s Harness', 'belt', 'padded', 4, 3, { str: 0.6, sta: 0.4 }], ['Vitaphone Engineer\'s Headset', 'hat', 'cloth', 4, 3, { int: 0.7, sta: 0.3 }],
  ['Rin Tin Tin\'s Collar', 'neck', 'jewel', 4, 3, { agi: 0.6, sta: 0.4 }], ['Zookeeper\'s Bite Sleeve', 'wrist', 'padded', 5, 3, { sta: 0.6, str: 0.4 }],
  ['Greek Theatre Chorus Robe', 'coat', 'cloth', 5, 3, { spi: 0.6, int: 0.4 }], ['The Gag Man\'s Derby', 'hat', 'leather', 6, 3, { agi: 0.6, sta: 0.4 }],
  ['Cornero\'s Rum-Runner Slicker', 'coat', 'leather', 6, 4, { agi: 0.6, sta: 0.4 }], ['Fairbanks\' Fencing Jacket', 'coat', 'leather', 6, 3, { agi: 0.6, str: 0.4 }],
  ['Dam Keeper\'s Waders', 'trousers', 'padded', 7, 3, { sta: 0.6, str: 0.4 }], ['Krotona Initiate\'s Sash', 'belt', 'cloth', 7, 3, { spi: 0.6, int: 0.4 }],
  ['Stembridge Armourer\'s Apron', 'coat', 'padded', 8, 3, { agi: 0.5, sta: 0.5 }], ['Bronson Gate Guard Cap', 'hat', 'padded', 8, 3, { agi: 0.5, sta: 0.5 }],
  ['Marathon Street Queen\'s Mantle', 'coat', 'cloth', 8, 4, { int: 0.6, spi: 0.4 }], ['Dragna Lieutenant\'s Spats', 'shoes', 'leather', 8, 3, { agi: 0.6, str: 0.4 }],
  ['Chaney\'s Greasepaint Kit', 'neck', 'jewel', 9, 3, { int: 0.5, sta: 0.5 }], ['Phantom\'s Opera Cloak', 'coat', 'cloth', 9, 4, { int: 0.5, sta: 0.5 }],
  ['Laemmle\'s Lucky Watch Chain', 'neck', 'jewel', 9, 4, { spi: 0.5, int: 0.5 }], ['Keystone Kop Helmet', 'hat', 'steel', 10, 3, { str: 0.5, sta: 0.5 }],
  ['Bathing Beauty\'s Slippers', 'shoes', 'cloth', 10, 3, { spi: 0.5, int: 0.5 }], ['Lockheed Riveter\'s Gauntlets', 'gloves', 'steel', 11, 3, { str: 0.6, sta: 0.4 }],
  ['Test Pilot\'s Goggles', 'hat', 'leather', 11, 4, { agi: 0.7, sta: 0.3 }], ['Vega Mechanic\'s Coveralls', 'coat', 'padded', 11, 3, { str: 0.5, int: 0.5 }],
  // Raid epics (band 12 = 60)
  ['Hive Queen\'s Chitin Crown', 'hat', 'steel', 12, 4, { str: 0.5, sta: 0.5 }], ['Mantle of the Thirteen Letters', 'coat', 'cloth', 12, 4, { int: 0.6, spi: 0.4 }],
  ['Aviatrix\'s Flight Jacket', 'coat', 'leather', 12, 4, { agi: 0.6, sta: 0.4 }], ['Hangar Mother\'s Carapace', 'coat', 'steel', 12, 4, { sta: 0.6, str: 0.4 }],
  ['Architect\'s Drafting Gloves', 'gloves', 'cloth', 12, 4, { int: 0.6, sta: 0.4 }], ['King Consort\'s Greaves', 'trousers', 'steel', 12, 4, { str: 0.6, sta: 0.4 }],
  ['Trimotor Pilot\'s Boots', 'shoes', 'padded', 12, 4, { agi: 0.6, sta: 0.4 }], ['Heart Chamber Signet', 'ring', 'jewel', 12, 4, { int: 0.5, spi: 0.5 }],
  ['Beery\'s Barnstormer Scarf', 'neck', 'jewel', 12, 4, { agi: 0.5, sta: 0.5 }], ['Runway Queen\'s Sash', 'belt', 'leather', 12, 4, { agi: 0.6, sta: 0.4 }],
  ['Deacon\'s Revival Vestments', 'coat', 'leather', 12, 4, { spi: 0.6, int: 0.4 }], ['Chorus Conductor\'s Cuffs', 'wrist', 'cloth', 12, 4, { int: 0.5, spi: 0.5 }],
];
const NAMED_WEAPONS = [
  ['Usher\'s Flashlight Baton', 'club', 2, 3, { str: 0.5, sta: 0.5 }], ['Hive Warden\'s Mandible', 'knife', 2, 3, { agi: 0.6, sta: 0.4 }],
  ['Egyptian Prop Khopesh', 'sabre', 2, 3, { str: 0.6, sta: 0.4 }], ['Projection Booth Arc Lamp', 'lamp', 2, 3, { int: 0.6, spi: 0.4 }],
  ['Matriarch\'s Spitting Gland Sprayer', 'revolver', 2, 3, { agi: 0.6, sta: 0.4 }], ['Stage Brace', 'heavy', 2, 3, { str: 0.6, sta: 0.4 }],
  ['Lighting Rigger\'s Wrench', 'tool', 2, 3, { int: 0.6, sta: 0.4 }], ['Robin Hood\'s Prop Quarterstaff', 'heavy', 2, 3, { str: 0.5, agi: 0.5 }],
  ['Sexton\'s Spade', 'heavy', 3, 3, { str: 0.6, sta: 0.4 }], ['Stembridge Rental Thompson', 'smg', 8, 3, { agi: 0.6, sta: 0.4 }],
  ['Tom Mix\'s Peacemaker', 'revolver', 5, 4, { agi: 0.7, sta: 0.3 }], ['Chandelier Chain', 'club', 9, 3, { str: 0.6, sta: 0.4 }],
  ['Post 43 Springfield', 'rifle', 12, 4, { agi: 0.6, sta: 0.4 }], ['Crawford\'s Thompson', 'smg', 12, 4, { agi: 0.6, sta: 0.4 }],
  ['Letter H Girder', 'heavy', 12, 4, { str: 0.6, sta: 0.4 }], ['Hive Queen\'s Stinger', 'knife', 12, 4, { agi: 0.6, sta: 0.4 }],
  ['Tower Beacon', 'lamp', 12, 4, { int: 0.6, spi: 0.4 }], ['Architect\'s T-Square', 'tool', 12, 4, { int: 0.6, sta: 0.4 }],
  ['Sister Aimee\'s Pulpit Bible', 'bible', 12, 4, { spi: 0.6, int: 0.4 }], ['Kop Captain\'s Nightstick', 'club', 10, 3, { str: 0.5, sta: 0.5 }],
  // Class quest rewards (band 2 = req 10)
  ['Belleau Wood Trench Club', 'club', 2, 3, { str: 0.6, sta: 0.4 }], ['Stunt Double\'s Weighted Gloves', 'fist', 2, 3, { agi: 0.6, str: 0.4 }],
  ['Studio Armourer\'s Winchester', 'rifle', 2, 3, { agi: 0.6, sta: 0.4 }], ['Klieg Light Reflector', 'lamp', 2, 3, { int: 0.6, spi: 0.4 }],
  ['Powder Monkey\'s Tamping Bar', 'tool', 2, 3, { int: 0.6, sta: 0.4 }], ['Queen of Angels Field Satchel', 'bag', 2, 3, { int: 0.6, spi: 0.4 }],
  ['Circuit Rider\'s Saddle Bible', 'bible', 2, 3, { spi: 0.6, int: 0.4 }], ['Model T Crank Handle', 'tool', 2, 3, { str: 0.6, int: 0.4 }],
  // Gunsmithing products (green)
  ['Zip Gun', 'pistol', 1, 2, { agi: 0.6, sta: 0.4 }], ['Sawed-off Coach Gun', 'shotgun', 2, 2, { agi: 0.6, str: 0.4 }],
];

export function buildItems() {
  const items = [];
  const seen = new Set();
  const add = (row, idHint) => {
    let id = 'item_' + slug(idHint || row.name) + (row.kind === 'armor' || row.kind === 'weapon' ? `_${row.req}` : '');
    if (seen.has(id)) return null;
    seen.add(id);
    row.id = id;
    items.push(row);
    return row;
  };
  BANDS.forEach((req, band) => {
    for (const [slot, types] of Object.entries(ARMOUR)) {
      for (const [type, names] of Object.entries(types)) {
        const base = tierName(names, band);
        add(armourRow(`${CONDITION[band]} ${base}`, slot, type, 1, req, {}, { base: 'base_' + slug(base) }));
        for (const suf of SUFFIX_FOR[type]) add(armourRow(`${base} ${suf}`, slot, type, 2, req, SUFFIXES[suf], { base: 'base_' + slug(base) }));
      }
    }
    for (const [slot, names] of Object.entries(JEWEL)) {
      const base = tierName(names, band);
      for (const suf of SUFFIX_FOR.jewel) add(armourRow(`${base} ${suf}`, slot, 'jewel', 2, req, SUFFIXES[suf], { base: 'base_' + slug(base) }));
    }
    for (const [type, w] of Object.entries(WEAPONS)) {
      if (band < (w.minBand || 0)) continue;
      const base = tierName(w.names, band, w.minBand || 0);
      add(weaponRow(`${CONDITION[band]} ${base}`, type, 1, req, {}, { base: 'base_' + slug(base) }));
      for (const suf of SUFFIX_FOR[w.fam]) add(weaponRow(`${base} ${suf}`, type, 2, req, SUFFIXES[suf], { base: 'base_' + slug(base) }));
    }
  });
  for (const [name, slot, type, band, q, split] of NAMED) add(armourRow(name, slot, type, q, BANDS[band], split, { named: true }));
  for (const [name, type, band, q, split] of NAMED_WEAPONS) add(weaponRow(name, type, q, BANDS[band], split, { named: true }));

  // Consumables: food restores health, drink restores Power, over 18 s out of combat (sitting).
  const food = [['Hot Dog', 'Coffee', 1, 60], ['Ham Sandwich', 'Coca-Cola', 5, 150], ['Blue Plate Special', 'Hires Root Beer', 15, 400], ['Chili Size', 'Egg Cream', 25, 800], ['Cobb Salad', 'Orange Julep', 35, 1400], ['Musso & Frank Steak Dinner', 'Brown Derby Coffee', 45, 2200]];
  for (const [f, d, req, amount] of food) {
    add({ id: '', name: f, kind: 'consumable', slot: '', type: 'food', q: 1, req, ilvl: req, stats: {}, armor: 0, price: Math.max(1, Math.round(req * 1.2)), stack: 20, use: { type: 'food', amount }, built: req <= 12 });
    add({ id: '', name: d, kind: 'consumable', slot: '', type: 'drink', q: 1, req, ilvl: req, stats: {}, armor: 0, price: Math.max(1, Math.round(req * 1.2)), stack: 20, use: { type: 'drink', amount: Math.round(amount * 1.2) }, built: req <= 12 });
  }
  add({ id: '', name: 'Linen Bandage', kind: 'consumable', slot: '', type: 'bandage', q: 1, req: 1, ilvl: 1, stats: {}, armor: 0, price: 2, stack: 20, use: { type: 'bandage', amount: 40 }, built: true });
  add({ id: '', name: 'Johnson & Johnson Gauze Roll', kind: 'consumable', slot: '', type: 'bandage', q: 1, req: 8, ilvl: 8, stats: {}, armor: 0, price: 8, stack: 20, use: { type: 'bandage', amount: 110 }, built: true });
  add({ id: '', name: 'Bootleg Gin', kind: 'consumable', slot: '', type: 'liquor', q: 1, req: 5, ilvl: 5, stats: {}, armor: 0, price: 12, stack: 10, use: { type: 'buff', stat: 'dmg_pct', value: 0.05, dur: 600 }, built: true, blurb: 'Contraband. +5% damage for 10 minutes.' });
  // Pocket items: usable in combat, sharing one 60 s cooldown.
  add({ id: '', name: 'Mk II Fragmentation Grenade', kind: 'consumable', slot: '', type: 'pocket', q: 1, req: 6, ilvl: 6, stats: {}, armor: 0, price: 12, stack: 10, use: { type: 'grenade', amount: 90 }, built: true, blurb: 'Army surplus "pineapple". Thrown at your target: damages every invader within 6 m of it.' });
  add({ id: '', name: 'Smoke Pot', kind: 'consumable', slot: '', type: 'pocket', q: 1, req: 4, ilvl: 4, stats: {}, armor: 0, price: 8, stack: 10, use: { type: 'smoke', amount: 30 }, built: true, blurb: 'A stage effects pot. Everything fighting you within 30 m loses track of you.' });
  add({ id: '', name: 'Smelling Salts', kind: 'consumable', slot: '', type: 'pocket', q: 1, req: 1, ilvl: 1, stats: {}, armor: 0, price: 3, stack: 10, use: { type: 'cleanse', amount: 0 }, built: true, blurb: 'Clears the head: removes stuns, dazes and slows.' });
  add({ id: '', name: 'Patent Tonic', kind: 'consumable', slot: '', type: 'pocket', q: 1, req: 3, ilvl: 3, stats: {}, armor: 0, price: 10, stack: 10, use: { type: 'heal_pct', amount: 30 }, built: true, blurb: '"Cures what ails you." Restores 30% of your health at once.' });
  // Ammunition (bible §6: each auto-shot uses one round). Sold in boxes of `buy`; price is per box, sell value per round.
  const ammo = [
    ['.38 Special Lead Round-Nose', 'pistol', 1, 0.5, 1, 'Fits revolvers, automatic pistols and the Thompson.'],
    ['.45 ACP Full Metal Jacket', 'pistol', 10, 1.4, 5, 'Fits revolvers, automatic pistols and the Thompson. Hits harder.'],
    ['.30-30 Winchester Soft-Point', 'rifle', 1, 0.6, 1, 'Fits lever-action and bolt rifles.'],
    ['.30-06 Springfield Ball', 'rifle', 10, 1.6, 5, 'Fits lever-action and bolt rifles. Army surplus.'],
    ['12-Gauge Birdshot Shells', 'shotgun', 1, 0.6, 1, 'Fits pump and auto shotguns.'],
    ['12-Gauge 00 Buckshot Shells', 'shotgun', 10, 1.6, 5, 'Fits pump and auto shotguns. Stops a charging drone.'],
  ];
  for (const [n, cal, req, dps, price, blurb] of ammo) add({ id: '', name: n, kind: 'ammo', slot: '', type: 'ammo', q: 1, req, ilvl: req, stats: {}, armor: 0, price, buy: 100, stack: 200, ammo: cal, ammo_dps: dps, blurb: `${blurb} +${dps} damage per second. Used one round per shot.`, built: true });
  // Junk and trade goods
  const junk = [['Cracked Mandible', 2], ['Bent Antenna', 3], ['Dull Chitin Flake', 4], ['Matted Fur', 2], ['Chewed Bone', 2], ['Empty Hip Flask', 6], ['Pawn Ticket', 9], ['Broodling Husk', 5], ['Acid-pitted Scale', 8], ['Torn Movie Ticket', 1]];
  for (const [n, lvl] of junk) add({ id: '', name: n, kind: 'junk', slot: '', type: 'junk', q: 0, req: 0, ilvl: lvl, stats: {}, armor: 0, price: Math.max(1, Math.round(lvl * lvl / 10 * 0.5) + lvl), stack: 20, built: true });
  const trade = [['Scrap Metal', 3, 'Salvaged from wrecks. Used in Gunsmithing and Metalworking.'], ['Chitin Shard', 6, 'Salvaged from dead invaders.'], ['Coyote Pelt', 4, 'A rough hide.'], ['Invader Gland', 8, 'Salvaged from Spitters. Used in Pharmacy.'], ['Brass Casings', 5, 'Used in Gunsmithing.'], ['Wool Cloth', 3, 'Used in Tailoring.'], ['White Sage', 12, 'Foraged in Griffith Park.'], ['Hive Resin', 40, 'From the Hive Tunnels.'], ['Elder Chitin Plate', 55, 'From elder invaders.']];
  for (const [n, lvl, blurb] of trade) add({ id: '', name: n, kind: 'trade', slot: '', type: 'trade', q: 1, req: 0, ilvl: lvl, stats: {}, armor: 0, price: Math.max(1, Math.round(lvl * lvl / 10) + 2), stack: 20, blurb, built: lvl <= 12 });
  return items;
}

export function questItem(name, blurb) {
  return { id: 'item_q_' + slug(name), name, kind: 'quest', slot: '', type: 'quest', q: 1, req: 0, ilvl: 1, stats: {}, armor: 0, price: 0, stack: 20, blurb: blurb || 'Quest item.', built: true };
}
export const slugify = slug;
