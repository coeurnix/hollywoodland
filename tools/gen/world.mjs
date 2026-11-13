// World facade data (DESIGN-BIBLE §4, §10): zones, dungeons, raids, factions, red-car lines, guilds, and the names that chat,
// the map, /who and tooltips may quote. Only zone_hollywood_blvd and dng_egyptian are built.
const Z = (id, name, lo, hi, blurb, hubs, neighbours, mobs, quests) => ({ id, name, level_min: lo, level_max: hi, blurb, hubs, neighbours, mobs, quest_titles: quests, built: id === 'zone_hollywood_blvd' });

export const zones = [
  Z('zone_hollywood_blvd', 'Hollywood Boulevard', 1, 12, 'The capital. Theatres, the bank, the auction house and every trainer, from La Brea to Vine.',
    ["Grauman's Chinese forecourt", 'Hollywood & Vine', 'Hollywood police station'], ['zone_gower_gulch', 'zone_melrose', 'zone_cahuenga_pass', 'zone_west_hollywood'],
    ['Invader Worker', 'Invader Drone', 'Invader Spitter', 'Invader Broodling', 'Invader Soldier', 'Feral Dog', 'Coyote', 'Looter'], []),
  Z('zone_gower_gulch', 'Gower Gulch', 8, 16, 'Poverty Row at Sunset and Gower, where the cowboy extras still wear their gun belts.',
    ['Columbia gate', 'Legion boxing stadium', 'Christie studio'], ['zone_hollywood_blvd', 'zone_melrose', 'zone_east_hollywood'],
    ['Invader Drone', 'Invader Spitter', 'Invader Burrower', 'Looter', 'Feral Dog', 'Stampeding Horse'],
    ['Six Reels for Columbia', 'The Carpenter Walks to Christie', 'Rope Tricks', 'Fight Night Undercard', 'Burrowers Under Poverty Row', 'A Horse for the Stables', 'The Extras Want Their Pay', 'Spitter Glands, Again']),
  Z('zone_echo_park', 'Echo Park & Edendale', 10, 18, 'The lake, the old Edendale studios and the five thousand seats of Angelus Temple.',
    ['Angelus Temple', 'Echo Park boathouse', 'Edendale red-car stop'], ['zone_east_hollywood', 'zone_griffith_park'],
    ['Invader Flier', 'Invader Drone', 'Looter', 'Rattlesnake', 'Feral Dog'],
    ['Sister Aimee\'s Radio Tower', 'Something in the Lake', 'Boats for the Boathouse', 'The Edendale Line Is Down', 'Hymnals and Shotguns', 'Fliers Over the Lotus Beds']),
  Z('zone_melrose', 'Melrose', 10, 20, 'Paramount and RKO behind their walls, and the cemetery between them and the boulevard.',
    ['Hollywood Cemetery gate', 'Van Ness & Santa Monica'], ['zone_hollywood_blvd', 'zone_gower_gulch', 'zone_west_hollywood'],
    ['Invader Soldier', 'Invader Spitter', 'Invader Burrower', 'Looter', 'Coyote'],
    ['The Crypt Keys', 'Flowers for the Cemetery Gate', 'Stembridge Wants His Thompsons', 'Marathon Street Patrol', 'RKO Is Too Quiet', 'Van Ness Barricade', 'The Night Watchman\'s Lamp']),
  Z('zone_east_hollywood', 'East Hollywood', 12, 22, 'Warner on Sunset, the old Fox lot on Western and the hall where the Unemployed Council meets.',
    ['Warner Bros. gate', 'Fox lot gate', 'Vitagraph lot'], ['zone_gower_gulch', 'zone_echo_park', 'zone_griffith_park'],
    ['Invader Soldier', 'Invader Flier', 'Invader Drone', 'Looter', 'Feral Dog'],
    ['A Dog at the Warner Gate', 'Sound Stage Seven', 'The Council Needs Bread', 'Red Squad Business', 'Vitaphone Discs', 'Western Avenue Sweep']),
  Z('zone_griffith_park', 'Griffith Park & Los Feliz', 15, 25, 'Wild country inside the dome: the zoo, the new Greek Theatre and Disney\'s little studio on Hyperion.',
    ['Greek Theatre', 'Disney Hyperion studio', 'Vermont Canyon camp'], ['zone_east_hollywood', 'zone_echo_park', 'zone_hollywoodland', 'zone_burbank'],
    ['Coyote', 'Mountain Lion', 'Black Bear', 'Mule Deer', 'Escaped Zoo Lion', 'Invader Burrower', 'Rattlesnake'],
    ['The Zoo Gates Are Open', 'Ink and Paint', 'White Sage for the Camp', 'Bear in the Greek', 'Old Zoo Keys', 'Vermont Canyon Water']),
  Z('zone_west_hollywood', 'West Hollywood & the Strip', 20, 30, 'County land past La Brea: Chaplin\'s studio, the Pickford-Fairbanks lot and Crawford\'s tunnels.',
    ['La Brea & Sunset', 'Chateau Marmont', 'Pickford-Fairbanks studio'], ['zone_hollywood_blvd', 'zone_melrose'],
    ['Dragna Gunman', 'Dragna Enforcer', 'Invader Soldier', 'Invader Spitter', 'Feral Dog'],
    ['Cornero\'s Shipment', 'The Tunnels Under the Strip', 'A Favor for Mr. Crawford', 'Dragna\'s Men on Sunset', 'The Marmont Cellar', 'Fairbanks\' Fencing Sabres']),
  Z('zone_hollywoodland', 'Hollywoodland', 25, 35, 'Beachwood Canyon, the Krotona buildings and the reservoir behind Mulholland Dam.',
    ['Hollywoodland gates', 'Krotona buildings', "Dam keeper's house"], ['zone_griffith_park', 'zone_cahuenga_pass', 'zone_mount_lee'],
    ['Hive-ridden Coyote', 'Mountain Lion', 'Invader Flier', 'Invader Soldier', 'Rattlesnake'],
    ['The Dam Keeper\'s Ledger', 'Krotona Lights', 'Beachwood Mail Run', 'Lake Hollywood Soundings', 'Canyon Road Burrowers']),
  Z('zone_cahuenga_pass', 'Cahuenga Pass', 30, 40, 'The road north out of Hollywood, the Bowl, and the Legion clubhouse with its captured field gun.',
    ['Post 43 clubhouse', 'Hollywood Bowl', 'Pass summit'], ['zone_hollywood_blvd', 'zone_hollywoodland', 'zone_universal_city'],
    ['Hardened Invader Soldier', 'Hardened Invader Spitter', 'Hardened Invader Flier', 'Hive-ridden Coyote'],
    ['Shells for the Field Gun', 'The Bowl at Night', 'Post 43 Roll Call', 'Summit Watch', 'Red Car Repair Crew']),
  Z('zone_universal_city', 'Universal City', 35, 45, 'Laemmle\'s lot and its backlot streets, and the Phantom\'s stage nobody will strike.',
    ['Universal front gate', 'Backlot European street'], ['zone_cahuenga_pass', 'zone_studio_city'],
    ['Hardened Invader Soldier', 'Hardened Invader Brute', 'Hardened Invader Burrower', 'Looter'],
    ['Uncle Carl\'s Guest List', 'The Man of a Thousand Faces', 'European Street Sweep', 'Stage 28 Rigging', 'Lugosi\'s Cape']),
  Z('zone_studio_city', 'Studio City & the River', 40, 50, 'Ventura Boulevard, Sennett\'s new lot and the camps along the Los Angeles River.',
    ['Laurel Canyon crossing', 'River camp'], ['zone_universal_city', 'zone_burbank'],
    ['Dragna Gunman', 'Dragna Enforcer', 'Hardened Invader Flier', 'Elder Invader Drone', 'Hive-ridden Black Bear'],
    ['River Camp Rations', 'Custard Pies and Dynamite', 'Laurel Canyon Crossing', 'Dragna\'s Collectors']),
  Z('zone_burbank', 'Burbank', 45, 55, 'First National\'s lot, the Lockheed plant and the hangars of United Airport.',
    ['Burbank town centre', 'Airport terminal'], ['zone_studio_city', 'zone_griffith_park'],
    ['Elder Invader Soldier', 'Elder Invader Flier', 'Elder Invader Brute', 'Hive-ridden Coyote'],
    ['Vega Parts', 'Beery\'s Flying Lesson', 'Terminal Under Siege', 'First National Vault', 'Hangar Keys']),
  Z('zone_mount_lee', 'Mount Lee', 55, 60, 'The ridge behind the HOLLYWOODLAND sign, where the hive is thickest.',
    ['Forward camp below the sign'], ['zone_hollywoodland'],
    ['Elder Invader Soldier', 'Elder Invader Brute', 'Elder Invader Spitter', 'Elder Invader Queen-guard'],
    ['Thirteen Letters', 'Forward Camp Supply', 'The Ridge Road', 'Into the Tunnels']),
];

const D = (id, name, zone, lo, hi, size, bosses) => ({ id, name, zone, level_min: lo, level_max: hi, size, bosses, built: id === 'dng_egyptian',
  blurb: `${name}: a ${size}-player ${id.startsWith('raid') ? 'raid' : 'dungeon'} for levels ${lo}${hi !== lo ? '-' + hi : ''} in ${zones.find((z) => z.id === zone).name}.` });
export const dungeons = [
  D('dng_egyptian', 'The Egyptian Theatre', 'zone_hollywood_blvd', 8, 12, 5, ['Brood Keeper', 'Hive Warden', 'Spitter Matriarch', 'The Egyptian Hive Mother']),
  D('dng_cemetery_crypts', 'Hollywood Cemetery Crypts', 'zone_melrose', 13, 18, 5, ['The Sexton', 'Crypt Burrower', 'Mausoleum Matron', 'The Pale Drone']),
  D('dng_warner_stages', 'Warner Bros. Sunset Stages', 'zone_east_hollywood', 18, 24, 5, ['Stage Manager Voss', 'The Boom Operator', 'Vitaphone Horror', 'Sound Stage Brute', 'The Understudy']),
  D('dng_griffith_zoo', 'Griffith Park Zoo', 'zone_griffith_park', 21, 27, 5, ['Sultan the Lion', 'The Keeper', 'Big Otto', 'Aviary Flier', 'Kong, zoo gorilla']),
  D('dng_chaplin_studios', 'Chaplin Studios', 'zone_west_hollywood', 26, 32, 5, ['The Gag Man', 'Prop Room Soldier', 'The Cutter', 'La Brea Burrower', 'The Director']),
  D('dng_mulholland_dam', 'Mulholland Dam', 'zone_hollywoodland', 29, 35, 5, ['Spillway Spitter', 'The Dam Keeper', 'Turbine Brute', 'Reservoir Flier', 'Gallery Warden', 'The Flood Mother']),
  D('dng_paramount', 'Paramount Studios', 'zone_melrose', 34, 40, 5, ['Gate Captain', 'The Wardrobe Mistress', 'Stage 4 Brute', 'Dragna\'s Lieutenant', 'The Projectionist', 'Bronson Gate Warden', 'The Marathon Street Queen']),
  D('dng_stage_28', 'Universal Stage 28', 'zone_universal_city', 40, 46, 5, ['The Rigger', 'Opera Box Spitter', 'The Chandelier', 'Understage Burrower', 'The Organist', 'The Phantom Brood']),
  D('dng_sennett_studio', 'Mack Sennett Studio', 'zone_studio_city', 45, 50, 5, ['The Bathing Brute', 'Pie Wagon', 'Kop Captain', 'The Tank', 'River Flier', 'The Gag Queen']),
  D('dng_lockheed', 'Lockheed Plant', 'zone_burbank', 50, 56, 5, ['Foreman Drone', 'The Riveter', 'Vega Flier', 'Wind Tunnel Warden', 'Paint Shop Spitter', 'The Test Pilot', 'Assembly Line Queen']),
  D('dng_hive_tunnels', 'Hive Tunnels', 'zone_mount_lee', 56, 60, 5, ['Tunnel Warden', 'The Nursery', 'Elder Burrower', 'Royal Guard', 'The Twins', 'Honey Vat', 'The Consort', 'The Tunnel Queen']),
  D('raid_united_airport', 'United Airport Hangars', 'zone_burbank', 60, 60, 20, ['Hangar One Swarm', 'The Tower', 'Trimotor', 'Fuel Depot Brute', 'The Mechanics', 'Runway Queen', 'The Aviatrix', 'The Hangar Mother']),
  D('raid_mount_lee', 'Mount Lee Hive', 'zone_mount_lee', 60, 60, 40, ['The Gatekeeper', 'Royal Spitters', 'The Architect', 'Brood Council', 'The Letter H', 'Elder Brute Pair', 'The Chorus', 'The King Consort', 'The Heart Chamber', 'The Hive Queen']),
];

export const factions = [
  { id: 'fac_lapd', name: 'Los Angeles Police Department', hub: 'Hollywood police station' },
  { id: 'fac_post43', name: 'American Legion, Hollywood Post 43', hub: 'Post 43 clubhouse' },
  { id: 'fac_unions', name: 'Studio Unions', hub: 'Union hall' },
  { id: 'fac_temple', name: 'Angelus Temple', hub: 'Angelus Temple' },
  { id: 'fac_wctu', name: "Woman's Christian Temperance Union", hub: 'Hollywood chapter' },
  { id: 'fac_bootleggers', name: "Crawford's Syndicate", hub: 'Sunset Blvd office' },
  { id: 'fac_communists', name: 'Unemployed Council', hub: 'East Hollywood hall' },
  { id: 'fac_extras', name: 'Gower Gulch Extras', hub: 'Gower Gulch' },
];

export const redcar = [
  { id: 'line_hollywood', name: 'Hollywood Blvd Line', stops: ['West Hollywood', 'Hollywood Blvd (Highland)', 'East Hollywood'] },
  { id: 'line_santa_monica', name: 'Santa Monica Blvd Line', stops: ['West Hollywood', 'Melrose'] },
  { id: 'line_glendale', name: 'Glendale-Burbank Line via Edendale', stops: ['Echo Park', 'Griffith Park', 'Burbank'] },
  { id: 'line_valley', name: 'San Fernando Valley Line via Cahuenga Pass', stops: ['Hollywood Blvd (Highland)', 'Cahuenga Pass', 'Universal City', 'Studio City'] },
];

export const guilds = [
  { id: 'guild_gower_irregulars', name: 'Gower Irregulars', kind: 'raid', blurb: 'Raids Mount Lee Tue/Thu 8pm server.', motd: 'Raids Tue/Thu 8pm server. Repair before you come. No pulling for the tank.' },
  { id: 'guild_post_43', name: 'Post Forty Three', kind: 'raid', blurb: 'Old-school raiding guild, clears United Airport weekly.', motd: 'Airport on reset. Consumables mandatory. Be nice in General, you represent us.' },
  { id: 'guild_red_car_regulars', name: 'Red Car Regulars', kind: 'social', blurb: 'Casual, all levels, dungeon runs most nights.', motd: 'Welcome aboard. Egyptian runs most nights, ask in here. Nobody minds questions.' },
  { id: 'guild_brown_derby', name: 'Brown Derby Lunch Club', kind: 'social', blurb: 'Social guild. Mostly talks about food.', motd: 'Lunch is at noon. Every day. Also we level sometimes.' },
  { id: 'guild_night_shift', name: 'Night Shift', kind: 'raid', blurb: 'Late-night raiders, Mount Lee progression.', motd: 'Mount Lee Fri/Sat 11pm server. Sign up in here. Sleep is optional.' },
  { id: 'guild_stembridge', name: 'Stembridge Gun Club', kind: 'crafting', blurb: 'Gunsmiths and salvagers. Sells ammo cheap to members.', motd: 'Members get ammo at cost from Skeets. Bring brass. Do not sell guns under 2c.' },
  { id: 'guild_extras', name: 'Central Casting', kind: 'levelling', blurb: 'New players welcome. Levelling groups.', motd: 'New? Good. Ask anything. Levelling groups form in here first.' },
  { id: 'guild_dome_patrol', name: 'Dome Patrol', kind: 'levelling', blurb: 'Questing and dungeon guild, levels 10-45.', motd: 'Quests and dungeons, 10-45. Help the new folks. Hive spreads get priority.' },
];

// World map layout (normalised 0..1, north up) for the 13 zone outlines.
export const mapLayout = {
  zone_hollywood_blvd: [0.36, 0.56, 0.22, 0.10], zone_gower_gulch: [0.58, 0.62, 0.10, 0.10], zone_melrose: [0.50, 0.72, 0.18, 0.10],
  zone_east_hollywood: [0.68, 0.58, 0.14, 0.14], zone_echo_park: [0.82, 0.66, 0.14, 0.16], zone_griffith_park: [0.66, 0.34, 0.24, 0.24],
  zone_west_hollywood: [0.16, 0.60, 0.20, 0.16], zone_hollywoodland: [0.46, 0.38, 0.20, 0.18], zone_cahuenga_pass: [0.34, 0.40, 0.12, 0.16],
  zone_universal_city: [0.30, 0.24, 0.16, 0.16], zone_studio_city: [0.10, 0.26, 0.20, 0.16], zone_burbank: [0.46, 0.08, 0.26, 0.20],
  zone_mount_lee: [0.50, 0.28, 0.14, 0.10],
};
