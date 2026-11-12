// Built quests for zone_hollywood_blvd and dng_egyptian (DESIGN-BIBLE §14). Text: giver's first person, plain 1930 voice.
// Objective types: kill{mobs[],count} item{item,count,from[],rate} object{kind,count,(item)} use{kind,count} talk{npc}
// Rewards: xp = 8 x same-level kill XP per step (x12 at a chain end), 10c x level. choice entries are [item name, req level].
const killXp = (L) => 45 + 5 * L;
const Q = (id, name, level, giver, turnin, objectives, text, done, o = {}) => ({ id, name, level, min_level: Math.max(1, level - 2), giver, turnin: turnin || giver, type: objectives[0].type, objectives, prereq: null, next: null,
  rewards: { xp: Math.round((o.end ? 12 : 8) * killXp(level)), cents: 10 * level * (o.end ? 2 : 1), choice: o.choice || [], rep: o.rep || {} }, text, complete_text: done, built: true, hub: o.hub || 'hub_chinese_forecourt', group: !!o.group, dungeon: !!o.dungeon, ...(o.extra || {}) });
const kill = (mobs, count, label) => ({ type: 'kill', mobs: [].concat(mobs), count, label });
const item = (name, count, from, rate, label) => ({ type: 'item', item: name, count, from: [].concat(from), rate, label: label || name });
const object = (kind, count, name, label) => ({ type: 'object', kind, item: name, count, label: label || name });
const use = (kind, count, label) => ({ type: 'use', kind, count, label });
const talk = (npc, label) => ({ type: 'talk', npc, count: 1, label });
const G = 'npc_sid_grauman';
const starter = [['Newsboy Cap of Stamina', 1], ['Leather Gloves of Agility', 1], ['Canvas Work Gloves of Strength', 1]];

export const quests = [
  Q('q_hb_01', 'Pests in the Forecourt', 1, G, G, [kill('mob_invader_worker', 6, 'Invader Workers slain')],
    "Welcome to the Chinese, friend. I'd shake your hand but I've a theatre full of vermin. Those pale ones, the Workers, are carrying off my forecourt a brick at a time. They don't bite unless you start it. Start it. Six of them ought to make my point.",
    "Six fewer. I can nearly hear the box office again. You'll do, kid."),
  Q('q_hb_02', 'Plates for Mr. Factor', 1, 'npc_max_factor', null, [item('Cracked Chitin Plate', 5, 'mob_invader_worker', 0.6)],
    "You have steady hands, I can tell. The shell of these creatures grinds to a powder finer than anything I can import. Bring me five plates off the Workers, cracked is no matter, and I will show you what a chemist can do with a nuisance.",
    "Yes. Yes, this will grind beautifully. Thank you."),
  Q('q_hb_03', 'A Note for the Roosevelt', 2, G, 'npc_roosevelt_clerk', [talk('npc_roosevelt_clerk', 'Deliver the note to the Roosevelt front desk')],
    "The Roosevelt across the way is holding rooms for my ushers and Pennington at the desk wants it in writing. Everything in writing, that man. Take him this note. Mind the streetcar tracks.",
    "From Mr. Grauman? In writing, good. If you need a bed, a meal or a quiet hour, the Roosevelt is open to anyone fighting for the boulevard.", { extra: { gives_on_accept: "Grauman's Note" } }),
  Q('q_hb_04', 'Dogs on Franklin', 2, 'npc_roosevelt_clerk', null, [kill('mob_feral_dog', 8, 'Feral Dogs slain')],
    "Our guests on the north side can't sleep for the howling. The dogs on Franklin went wild the week the sky closed and they hunt in packs now. Thin them out, eight would do, and the hotel will see you're paid.",
    "A quiet night at last. The hotel is obliged to you.", { choice: starter }),
  Q('q_hb_05', 'Coyote Country', 3, 'npc_roosevelt_clerk', null, [kill('mob_coyote', 8, 'Coyotes slain'), item('Coyote Pelt', 4, 'mob_coyote', 0.5)],
    "Coyotes have come down out of the hills as far as Franklin and Wilcox. The housekeeper says a good pelt makes a better blanket than anything we can get through the dome. Eight coyotes, and four pelts fit to use.",
    "These will keep somebody warm. Well done."),
  Q('q_hb_06', 'The Newsreel Truck', 3, G, G, [object('film_can', 6, 'Newsreel Film Can', 'Newsreel film cans recovered')],
    "A Fox Movietone truck went over on Cahuenga, down by Selma, the night it all started. There is footage in those cans of the first hour and I mean to show it. Bring me six cans. The alleys there are crawling, so step lively.",
    "Six cans, and not a dent that matters. Someday people will want to see this."),
  Q('q_hb_07', 'Light the Boulevard', 4, G, G, [use('arc_lamp', 4, 'Marquee arc lamps lit')],
    "A dark boulevard belongs to them. A lit one belongs to us. There are four arc lamps between here and Wilcox that still have carbon in them. Strike them up, all four, and let the things see we're open for business.",
    "Look at that. You can read a marquee from Highland again."),
  Q('q_hb_08', 'Selma Avenue Sweep', 4, G, G, [kill('mob_invader_drone', 10, 'Invader Drones slain')],
    "The Drones are a different animal from the Workers. They come at you, and they bring their friends. The alleys off Selma, behind the Egyptian, are thick with them. Ten would give my ushers room to breathe.",
    "Ten Drones. You're earning a name on this street.", { choice: [['Work Boots of Agility', 5], ['Canvas Plimsolls of Intellect', 5], ['Hobnail Boots of Strength', 5]], end: true }),
  Q('q_hb_09', 'Old Scar', 5, 'npc_roosevelt_clerk', null, [kill('mob_old_scar', 1, 'Old Scar slain')],
    "There's one coyote the bellhops have a name for. Old Scar. Big grey brute, hunts alone up past Franklin on the east end, and he's taken two of the hotel's dogs. I'd count it a personal favour.",
    "Old Scar himself? The bellhops will want to hear it from you.", { choice: [['Brass Ring of the Acrobat', 5], ['Brass Ring of the Engineer', 5], ['Brass Ring of the Boxer', 5]], end: true }),
  Q('q_hb_10', 'An Introduction at the Taft', 4, G, 'npc_auctioneer', [talk('npc_auctioneer', 'Speak to the auctioneer at the Taft Building')],
    "You've been picking things up off those creatures. Don't sell it all to shopkeepers. Cornelius Vane runs the auction floor at the Taft Building, Hollywood and Vine, and everyone on the hill sells through him. Tell him Sid sent you.",
    "Grauman's word is good here. Everything that comes off a studio lot, a hive or a dead man's back is sold on this floor. Browse the listings, bid as you please.", { hub: 'hub_vine' }),
  Q('q_hb_11', 'Glands for the Drugstore', 5, 'npc_druggist', null, [item('Spitter Gland', 8, 'mob_invader_spitter', 0.5)],
    "I do not hold with liquor, but I hold with medicine. The yellow ones that spit carry a gland that makes a first-rate burn salve. They roost above Cahuenga, south of the boulevard. Bring me eight, and wear gloves.",
    "Eight, and intact. There will be salve at the aid stations by morning.", { hub: 'hub_vine', choice: [['Cotton Gloves of Intellect', 5], ['Leather Gloves of the Acrobat', 5], ['Canvas Work Gloves of the Boxer', 5]] }),
  Q('q_hb_12', 'Behind the Taft', 5, 'npc_taft_doorman', null, [kill('mob_looter', 8, 'Looters slain')],
    "It ain't only bugs. There's men behind this building stripping the back lots and selling to whoever. They shot at the night porter. Eight of them gone and the rest'll find another block.",
    "That's the back lots quiet. The building thanks you, and so do I.", { hub: 'hub_vine' }),
  Q('q_hb_13', 'Coffee for the Derby', 6, 'npc_derby_cook', null, [object('coffee_tin', 10, 'Brown Derby Coffee Tin', 'Coffee tins recovered')],
    "No trucks come through that dome, so what's in my storeroom is all the coffee there is. Trouble is the storeroom door's gone and the tins are scattered round the back of the Derby. Ten tins and the whole street stays awake.",
    "Ten tins. I could kiss you. Have a cup instead.", { hub: 'hub_vine' }),
  Q('q_hb_14', 'The Stagehand', 6, 'npc_pantages_manager', null, [talk('npc_stagehand', 'Find Whitey Nolan in the Cahuenga alleys')],
    "My best stagehand, Whitey Nolan, went down Cahuenga for lumber two days ago. Somebody saw him holed up past Selma with those spitting things all around. Find him and tell him the Pantages still has a job for him.",
    "Whitey's alive? Then the show goes up Friday. Thank you.", { hub: 'hub_vine' }),
  Q('q_hb_15', 'Rooftop Spitters', 7, 'npc_pantages_manager', null, [kill('mob_invader_spitter', 6, 'Invader Spitters slain')],
    "The Spitters above Cahuenga can hit my loading door from the rooftops. I have had two scenery flats burned through. Six of them, if you please, before they start on the marquee.",
    "The loading door is safe. You have a friend at the Pantages.", { hub: 'hub_vine', choice: [['Wool Trousers of the Doctor', 5], ['Leather Chaps of the Acrobat', 5], ['Levi Strauss Waist Overalls of the Boxer', 5], ['Steel-kneed Breeches of Strength', 5]], end: true }),
  Q('q_hb_16', 'Charges on the Spires', 7, 'npc_salvage_trainer', null, [use('spire', 3, 'Hive spires destroyed')],
    "See those spires going up south of Selma? Every one of them hatches a Drone on the half minute. I got charges from a powder man at Columbia. Set them on three spires and stand well back.",
    "Three spires down. That'll slow the hatching.", { hub: 'hub_vine' }),
  Q('q_hb_17', 'Report to Chief Davis', 6, G, 'npc_chief_davis', [talk('npc_chief_davis', 'Report to Chief Davis at the Hollywood police station')],
    "The Chief of Police himself has set up at the Hollywood station on Wilcox. He asked me for anyone who can handle themselves, and you can. Go and see him. Stand up straight when you do.",
    "Grauman sent you? Then you can shoot or you can run, and I need both. Hollywood High is a nest and I mean to clean it out.", { hub: 'hub_police' }),
  Q('q_hb_18', 'The School Gates', 7, 'npc_chief_davis', null, [kill('mob_invader_drone_hs', 10, 'Drones slain at Hollywood High')],
    "Hollywood High, Sunset and Highland. The Drones hold the east gates. I want ten of them dead before I send a single officer through. This department does not lose men to insects.",
    "The gates are open. Good work.", { hub: 'hub_police', rep: { fac_lapd: 250 } }),
  Q('q_hb_19', 'Broodlings', 7, 'npc_chief_davis', null, [kill('mob_invader_broodling', 15, 'Invader Broodlings slain')],
    "The small ones in the schoolyard come in swarms. Any one of them is nothing. Fifteen of them have a man off his feet in a moment. Kill fifteen, and don't let them get round behind you.",
    "Fifteen. They'll hatch more, but not today.", { hub: 'hub_police', rep: { fac_lapd: 250 } }),
  Q('q_hb_20', 'Call-box Keys', 8, 'npc_kearney', 'npc_kearney', [object('callbox', 5, 'Call-box Key', 'Call-box keys recovered')],
    "Every call box on Sunset has a key hanging in it and every key opens every box. I can't have looters ringing the station at all hours. Fetch me five keys off the Sunset boxes.",
    "Five keys. That's five boxes they can't play games with.", { hub: 'hub_police', rep: { fac_lapd: 250 }, extra: { giver_override: 'npc_capt_hynes' } }),
  Q('q_hb_21', 'The Ringleader', 8, 'npc_capt_hynes', null, [kill('mob_looter_ringleader', 1, 'Knuckles Malone slain')],
    "The looters behind the Taft take orders from one man, Knuckles Malone. He works out of the lot at the far east end. My squad has a file on him an inch thick. Close the file.",
    "Malone's finished? I'll mark the file.", { hub: 'hub_police', rep: { fac_lapd: 350 }, choice: [['Newsboy Cap of the Engineer', 5], ['Newsboy Cap of the Doctor', 5], ['M1917 Helmet of the Boxer', 5]], end: true }),
  Q('q_hb_22', 'The Gymnasium', 9, 'npc_chief_davis', null, [use('egg', 8, 'Egg clusters destroyed')],
    "They are laying in the school gymnasium. Egg clusters, wall to wall. Burn eight of them. If we leave it a week there will be no taking that school back.",
    "Eight clusters. That's a few hundred we won't have to shoot.", { hub: 'hub_police', rep: { fac_lapd: 250 } }),
  Q('q_hb_23', 'Soldiers on the Field', 9, 'npc_chief_davis', null, [kill('mob_invader_soldier', 6, 'Invader Soldiers slain')],
    "The armoured ones on the athletic field are their regulars. Bullets skip off them. Six dead Soldiers would tell me whether this town can be held. Take help if you can find it.",
    "Six Soldiers. Then it can be done.", { hub: 'hub_police', rep: { fac_lapd: 350 }, choice: [['Plain Cane', 10], ['Plain Colt Police Positive', 10], ['Plain Pipe Wrench', 10]], end: true }),
  Q('q_hb_24', 'The Ballroom Brute', 10, 'npc_chief_davis', null, [kill('mob_ballroom_brute', 1, 'Ballroom Brute slain')],
    "There is something the size of a delivery truck nesting in the ballroom of the Hollywood Hotel. It walked through the north wall. No one man is going to kill it. Raise a party and bring it down.",
    "The ballroom is ours. The hotel will want to give a dance.", { hub: 'hub_police', group: true, rep: { fac_lapd: 500 }, choice: [['Brass Ring of the Boxer', 10], ['Brass Ring of the Acrobat', 10], ['Brass Ring of the Doctor', 10]], end: true }),
  Q('q_hb_25', 'Clear the Egyptian', 8, G, G, [kill('mob_boss_hive_mother', 1, 'The Egyptian Hive Mother slain')],
    "I built the Egyptian before I built this place. Opened it with Fairbanks in Robin Hood, in 'twenty-two. Now there's a queen of some kind on my stage. You'll want four friends. Go in through the courtyard and give me back my theatre.",
    "She's dead? On my stage? Kid, the Egyptian reopens Friday and your name goes on the marquee.", { dungeon: true, group: true, choice: [["Grauman's Usher Coat", 10], ['Usher\'s Flashlight Baton', 10], ['Scarab Signet', 10]], end: true }),
  Q('q_hb_26', 'Five Reels of Robin Hood', 8, G, G, [object('reel', 5, 'Reel of Robin Hood', 'Reels of Robin Hood recovered')],
    "The print of Robin Hood is still up in the Egyptian's projection booth. Five reels I care about. Nitrate burns if you look at it wrong, so carry them like eggs.",
    "All five. We'll run it the night the Egyptian reopens.", { dungeon: true, group: true }),
  Q('q_hb_27', 'The Ushers', 9, 'npc_capt_hynes', null, [use('usher', 4, 'Captured ushers freed')],
    "Four of Grauman's ushers were taken alive into the Egyptian. The creatures keep people to work the place. Find them, cut them loose and send them out the front.",
    "All four walked out. Good.", { hub: 'hub_police', dungeon: true, group: true, rep: { fac_lapd: 350 } }),
  Q('q_hb_28', 'Scrap Drive', 5, 'npc_gunsmith', null, [object('scrap', 10, 'Scrap Metal', 'Scrap Metal salvaged')],
    "I can't cast a bullet out of good intentions. The wrecks in the back lots are full of steel and brass. Learn to salvage, Ike Brandt at Vine will show you if you ask, and bring me ten loads of scrap.",
    "That's a week of cartridges. Much obliged.", { hub: 'hub_highland' }),
  Q('q_hb_30', 'Word from Melrose', 12, G, 'npc_cemetery_gatekeeper', [talk('npc_cemetery_gatekeeper', 'Report to the Hollywood Cemetery gate in Melrose')],
    "You've outgrown my boulevard. The people at the Hollywood Cemetery gate, down in Melrose, are asking for help with the crypts. It's a hard road south through the wall. Don't go alone.", "", { extra: { breadcrumb: 'zone_melrose' } }),
  Q('q_hb_31', 'Extras Wanted', 12, 'npc_auctioneer', 'npc_columbia_gate', [talk('npc_columbia_gate', 'Report to the Columbia gate in Gower Gulch')],
    "Columbia is hiring anyone who can hold a gun, down at Sunset and Gower. The cowboys call it Gower Gulch. East through the wall, if you can get past what guards it.", "", { hub: 'hub_vine', extra: { breadcrumb: 'zone_gower_gulch' } }),
  Q('q_hb_32', 'The Legion Is Calling', 12, 'npc_chief_davis', 'npc_post43_adjutant', [talk('npc_post43_adjutant', 'Report to Post 43 in Cahuenga Pass')],
    "Post 43 holds the clubhouse up Highland with a German field gun and not much else. They want experienced hands. It is thirty levels of trouble north of here. Go when you are ready, not before.", "", { hub: 'hub_police', extra: { breadcrumb: 'zone_cahuenga_pass' } }),
];
// chains
const chain = (a, b) => { quests.find((q) => q.id === a).next = b; quests.find((q) => q.id === b).prereq = a; };
chain('q_hb_01', 'q_hb_03'); chain('q_hb_03', 'q_hb_04'); chain('q_hb_04', 'q_hb_05'); chain('q_hb_05', 'q_hb_09');
chain('q_hb_01', 'q_hb_06'); chain('q_hb_06', 'q_hb_07'); chain('q_hb_07', 'q_hb_08'); chain('q_hb_08', 'q_hb_17');
chain('q_hb_17', 'q_hb_18'); chain('q_hb_18', 'q_hb_19'); chain('q_hb_19', 'q_hb_22'); chain('q_hb_22', 'q_hb_23'); chain('q_hb_23', 'q_hb_24');
chain('q_hb_11', 'q_hb_15'); chain('q_hb_12', 'q_hb_21'); chain('q_hb_14', 'q_hb_15');
// A quest can have only one prereq; restore the ones overwritten above (first prereq wins for display, all listed here).
quests.find((q) => q.id === 'q_hb_06').prereq = 'q_hb_01'; quests.find((q) => q.id === 'q_hb_15').prereq = 'q_hb_11';
quests.find((q) => q.id === 'q_hb_20').giver = 'npc_kearney';
