// Chat templates and the seed corpus (DESIGN-BIBLE §11 "Chat sources"). Simulated players type modern MMO shorthand.
// Template tokens (filled by the client from data, so every reference resolves):
//   {dng} dungeon fitting the speaker's level   {raid} a raid   {zone} speaker's home zone   {ozone} any other zone
//   {item} [item link] near the speaker's level  {price} a price   {role} tank/heals/dps   {guild} speaker's guild
//   {mob} a mob of the speaker's zone   {quest} a quest title of the speaker's zone   {lvl} speaker level   {class} class name
//   {n} small number   {boss} a boss of {dng}   {player} the human's name   {target} context name
// "min"/"max" restrict by speaker level; "ch" is the channel.
const T = (ch, s, text, o = {}) => ({ ch, s, text, ...o });
export const templates = [
  T('lfg', 'lfm', 'LFM {dng} need {role}'), T('lfg', 'lfm', 'lfm {dng}, need {role} and 1 dps then g2g'), T('lfg', 'lfm', 'LF{n}M {dng} pst'),
  T('lfg', 'lfm', '{dng} run need {role}, have summons lol jk no summons walk'), T('lfg', 'lfm', 'need 1 more for {dng} last spot {role}'),
  T('lfg', 'lfg', 'lvl {lvl} {class} lfg {dng}'), T('lfg', 'lfg', '{class} LFG {dng} or anything really'), T('lfg', 'lfg', 'lfg {dng} can {role}'),
  T('lfg', 'lfg', '{lvl} {class} looking for {dng} group, know fights'), T('lfg', 'raid', 'LFM {raid} tonight 8 server, need {role}, pst gear', { min: 58 }),
  T('lfg', 'raid', '{raid} pug forming at the bank need 2 heals 1 tank', { min: 58 }), T('lfg', 'raid', '<{guild}> {raid} has 2 open spots tonight, {role} pref', { min: 58, guild: true }),
  T('lfg', 'lfm', 'anyone for the {quest} quest in {zone}? its a group one', { min: 8 }), T('lfg', 'lfm', 'LF2M ballroom brute at the hollywood hotel, elite quest', { max: 13 }),
  T('lfg', 'lfm', 'LFM egyptian need tank and heals, lvl 8+', { max: 14 }), T('lfg', 'lfg', 'lvl {lvl} {class} lfg egyptian', { min: 8, max: 14 }),
  T('trade', 'wts', 'WTS {item} {price}'), T('trade', 'wts', 'wts {item} {price} obo, at the taft'), T('trade', 'wts', 'WTS {item} and {item}, pst'),
  T('trade', 'wts', 'selling {item} cheaper than AH {price}'), T('trade', 'wtb', 'WTB {item} paying well'), T('trade', 'wtb', 'wtb {item} pst with price'),
  T('trade', 'wtb', 'WTB chitin shards any amount'), T('trade', 'wts', 'WTS scrap metal stacks 30c each'), T('trade', 'craft', 'gunsmith LFW, your mats, tips appreciated', { min: 15 }),
  T('trade', 'craft', 'tailor lfw can make 12 slot suitcases', { min: 20 }), T('trade', 'wtb', 'wtb .38 rounds x200'), T('trade', 'wts', 'WTS {item} {price}, or trade for invader glands'),
  T('trade', 'craft', 'any pharmacist on who can make burn salve? have mats'), T('trade', 'wts', 'WTS [Bootleg Gin] x10 dont tell the wctu', { min: 5 }),
  T('zone', 'guild', '<{guild}> recruiting all levels, dungeon runs nightly, pst', { guild: true }), T('zone', 'guild', '<{guild}> LF more for {raid}, need heals mostly', { guild: true, min: 55 }),
  T('zone', 'guild', '{guild} is recruiting. social, helpful, no drama (some drama)', { guild: true }),
  T('zone', 'question', 'where is the {quest} quest giver in {zone}?', { min: 8 }), T('zone', 'question', 'anyone done {quest}? cant find the last one', { min: 8 }),
  T('zone', 'question', 'is {dng} worth it at {lvl}?'), T('zone', 'question', 'how do i get to {ozone} from here'), T('zone', 'answer', 'red car from highland, or walk if you like dying'),
  T('zone', 'question', 'does {boss} drop anything for {class}?'), T('zone', 'brag', 'finally got {item} from {boss}!!'), T('zone', 'brag', '{boss} down, {n} wipes but down'),
  T('zone', 'complain', '{mob}s in {zone} hit way too hard', { min: 8 }), T('zone', 'complain', 'someone tagged my {mob} again'), T('zone', 'complain', 'wiped on {boss} twice, healer went afk'),
  T('zone', 'complain', 'who keeps training {mob}s into the hub lol'), T('zone', 'banter', 'the guards at the east wall one shot me. lvl 22 elites. why'),
  T('zone', 'banter', 'dont go past the hive wall under 20. just dont'), T('zone', 'banter', 'saw a lvl 60 on a duesenberg. how much is that mount again'),
  T('zone', 'answer', '900 bucks for the fast ones. start saving'), T('zone', 'banter', 'grauman gives the egyptian quest at 8 btw'), T('zone', 'banter', 'old scar is up, east end of franklin'),
  T('zone', 'banter', 'selma burrower just ate me. rare spawn by the alleys'), T('zone', 'banter', 'picture night at the chinese later, free buff'),
  T('zone', 'question', 'where do i learn salvaging'), T('zone', 'answer', 'ike brandt at hollywood and vine'), T('zone', 'question', 'where is the bank'), T('zone', 'answer', 'first national, hollywood and highland, big grey one'),
  T('zone', 'question', 'any {role} want to run egyptian?', { max: 14 }), T('zone', 'banter', 'is the dome the edge of the map or can you get out'), T('zone', 'answer', 'nothing gets out. devs said so'),
  T('zone', 'banter', 'this game needs flying mounts'), T('zone', 'answer', 'its 1930'), T('zone', 'banter', 'lvl {lvl} and still wearing the starter hat'),
];

// Corpus: tagged by situation (s) and optional temperament (t). Fixed lines, no tokens except {player}/{target}.
const L = (s, lines, t) => lines.map((text) => ({ s, t: t || 'any', text }));
export const corpus = [
  ...L('greet', ['hey', 'hi', 'yo', 'sup', 'hello', 'o/', 'heya', 'hey {player}', 'evening all', 'morning']),
  ...L('bye', ['night all', 'gn', 'gtg', 'off for the night, gn', 'logging, cya', 'bed. gn all', 'bbl dinner', 'ok im out', 'cya tomorrow']),
  ...L('afk', ['afk', 'brb', 'afk 2 min', 'brb dog', 'brb door', 'sec', 'afk bio']),
  ...L('back', ['back', 'b', 'ok back', 're']),
  ...L('thanks', ['ty', 'thanks', 'ty!', 'thx', 'tyvm', 'cheers', 'appreciate it']),
  ...L('welcome', ['np', 'yw', 'no worries', 'anytime', 'np!']),
  ...L('ding', ['ding', 'ding!', 'DING', 'ding {lvl}', 'finally {lvl}', 'ding. only {left} to go']),
  ...L('gz', ['gz', 'grats', 'gratz', 'gz!', 'grats!', 'nice', 'congrats']),
  ...L('death', ['lol', 'well that went badly', 'ow', 'oops', 'rip me', 'welp', 'too many', 'overpulled', 'that was a lot of drones', 'running back', 'ffs']),
  ...L('train', ['sorry sorry sorry', 'TRAIN', 'help', 'too many help', 'my bad', 'run', 'sorry about the drones', 'oops brought friends']),
  ...L('combat_help', ['help', 'adds', 'more inc', 'on me', 'get it off me', 'heals', 'need heals']),
  ...L('oom', ['oom', 'oom sec', 'need to drink', 'mana break', 'drinking']),
  ...L('ready', ['r', 'rdy', 'ready', 'go', 'pull', 'ok go']),
  ...L('wipe', ['wipe', 'well', 'run back', 'my bad', 'that was the adds', 'kill the adds first next time', 'again?', 'ok that didnt work', 'eggs. we forgot the eggs', 'one more try']),
  ...L('boss_kill', ['nice', 'gg', 'ez', 'clean', 'good job all', 'that was close', 'finally']),
  ...L('loot_win', ['nice', 'ooh', 'finally an upgrade', 'ty rng', 'mine', ':)']),
  ...L('loot_lose', ['gz', 'grats', 'ugh', 'of course', 'i never win rolls', 'gz i guess']),
  ...L('accept_invite', ['sure', 'ok', 'omw', 'inv', 'yeah ok', 'why not', 'sure sec', 'ok where']),
  ...L('decline_busy', ['cant rn sorry', 'in a group already', 'busy sry', 'about to log', 'no thanks', 'maybe later', 'questing rn', 'full sorry']),
  ...L('decline_low', ['ur {plvl} lol', 'too low sorry', 'come back at {need}', 'not at lvl {plvl} mate', 'need to be higher for that', 'id carry but no time']),
  ...L('decline_saved', ['already saved this week', 'saved sorry', 'on lockout']),
  ...L('party_join', ['hey', 'hi all', 'yo', 'ty for inv', 'sup', 'hello']),
  ...L('party_leave', ['gtg ty for group', 'thanks all', 'gotta go, gg', 'ty, cya', 'need to log, ty']),
  ...L('mob_tag', ['that was mine', 'really', 'ok', 'tagged it first but ok', 'lol take it']),
  ...L('heal_passby', ['there you go', 'np', 'careful', 'you looked low']),
  ...L('whisper_fallback', ['sec', 'lol', 'busy rn', 'hm?', 'one sec', 'idk', 'ask in zone', 'afk sorry', 'what', 'in a fight sec', 'yeah', 'no idea sorry']),
  ...L('who_ai', ['lol what', 'beep boop. no, im just slow', 'i wish, id level faster', 'rude', 'says the guy who keeps pulling 3 drones', 'haha ok', 'bots dont die this much', 'only on mondays']),
  ...L('idle', ['anyone know when picture night starts', 'this zone is packed tonight', 'lag?', 'is it just me or are the drones tougher today', 'need {left} more for {lvl}', 'so many workers so little time',
    'the chinese looks nice with the lights on', 'why is everyone at the bank', 'dance party on the bank steps', 'someone duel me outside the roosevelt', 'bored', 'what should i roll next',
    'my bags are full again', 'who took all the scrap nodes', 'need a bigger suitcase', 'vendor prices are robbery', 'repair bill hurts', 'i love this game', 'one more quest then bed',
    'is hynes a real guy', 'max factor is a real guy fyi', 'grauman was real too. look it up', 'the hotel brute is no joke', 'broodlings give bad xp dont bother', 'spitters hurt. bring bandages',
    'soldiers have so much armor', 'looters drop gin sometimes', 'coyotes come in packs watch out', 'dogs on franklin are easy xp at 2', 'do the lamp quest its free xp',
    'anyone else stuck on the call box keys', 'keys are on sunset, the blue boxes', 'film cans are by the wrecked truck on cahuenga', 'eggs are in the school gym', 'red car is just for show until you find more stops']),
  ...L('idle', ['LOL', 'WHO PULLED THAT', 'GUYS', 'HELLO BOULEVARD', 'anyone awake', 'hey hey hey'], 'loud'),
  ...L('idle', ['why did the drone cross the boulevard', 'i main worker. pacifist run', 'selling my soul 5c', 'legally distinct insects', 'i came here to act and kill bugs and im all out of acting'], 'joker'),
  ...L('idle', ['first to 12 today wins', 'race you to 10', 'top dps or go home', 'that rare is mine'], 'competitive'),
  ...L('idle', ['if anyone needs help with the brute pst', 'free bandages at the forecourt if anyone needs', 'happy to heal egyptian runs', 'new players: train your abilities every 2 levels'], 'helpful'),
  ...L('idle', ['paying 20c for someone to kill old scar for me', 'will tank for tips', 'wts my time'], 'mercenary'),
];
