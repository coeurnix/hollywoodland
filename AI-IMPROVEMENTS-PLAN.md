# AI improvements plan: simulated players (SPs)

Status: **plan for review, nothing implemented yet** (2026-11-14). Written after reading the whole current system
(`HLSim.cpp`, `HLSimLife.cpp`, `HLChat.cpp`, `HLSocial.cpp`, `workers/server`, `tools/gen/personas.mjs`, `tools/gen/chat.mjs`,
DESIGN-BIBLE §11 and §13) and measuring OpenAI against Fireworks.

Contents: [1 Where we are](#1-where-we-are) · [2 OpenAI latency findings](#2-openai-latency-findings) ·
[3 Target architecture](#3-target-architecture) · [4 Workstreams](#4-workstreams) · [5 Cost and limits](#5-cost-and-limits) ·
[6 Testing](#6-testing) · [7 Order of work](#7-order-of-work) · [8 Decisions I need from you](#8-decisions-i-need-from-you)

---

## 1. Where we are

What an SP is today, and why it reads as a bot:

| Area | Today | Problem against the brief |
| --- | --- | --- |
| Personality | One of six `temperament` words, a typing style, `reaction_s`, `mistake_rate`. Temperament only picks corpus lines and nudges invite odds. | No archetypes; personality never touches play style. |
| Goals | A state machine (`Idle → Travel → Grind → Rest`, town errand after 6-16 kills, `Leaving`). Scripted set pieces (`State = Script`) for the Director, happenings, fight night, dungeon groups. `persona.goal` is a display string. | Nobody *wants* anything. No priorities, no opportunity cost, no LLM judgment. |
| Movement | `MoveTo` follows the A* path at full speed in straight segments and stops exactly at the goal. Party members hold a fixed slot behind the player (`Yaw + 140 + Slot * 27`, 3 m), teleport at 50 m. | Perfect lockstep; no hesitation, drift, overshoot or correction. |
| Combat | `SimPickAbility` scores every ability every `reaction_s`; a "mistake" is a 1.5-3.5 s pause. Party sims read the player's target directly. | Optimal rotation with pauses. Never overwhelmed, never wrong, never inventive. |
| Typing | Lines are delayed by length, but the sim keeps running and fighting while "typing". | Types while moving and fighting. |
| Party talk | Party sims share the game's state: target, health, who is tanking. Chat is decoration. | Nothing to misunderstand, so nothing to say. |
| Knowledge | Three fixed world facts plus zone and dungeon blurbs are pasted into every prompt; the model may only mention names in `allowed.ids`. | Narrow, the same for everyone, and independent of who the SP is. |
| Observation | SPs react only to chat lines and invite/trade/duel requests. | Emote at one, dance, jump in circles: nothing. |
| Memory | `FHLRelation`: familiarity, liking, up to 20 fact strings, a bond word; saved locally and in KV. No dates, no forgetting, no conversation history. | Flat list that fills up and never fades. |
| Setting | Prompt says "regular player of this game"; templates talk raids, lockouts, guild schedules; 45% of personas are level 41-60. | An "established MMO" nobody believes for a jam entry. |
| Models | Worker → Fireworks: Rune (`/systemone`) decides reply/ignore, accept/decline, liking; DeepSeek flash writes lines. | One provider, no fallback. |

What already works and stays: the Census, the corpus/template ambient chat (cost-free), the typing-style filter, the
Worker's session/limiter/validation shell, bonds, the scripted events. The plan builds a mind *under* these, then
migrates each of them onto it.

---

## 2. OpenAI latency findings

Measured 2026-11-14 with `node tools/ai-bench.mjs 12` (same prompts the Worker builds today; 12 calls each after a warm-up).

| Call | p50 | p90 | max | Measured from |
| --- | ---: | ---: | ---: | --- |
| Fireworks Rune (decisions) | 161 ms | 188 ms | 199 ms | Cloudflare edge (Worker's own timer) |
| **OpenAI Decisions API** (`gpt-6-luna`) | 548 ms | 2,261 ms | 6,860 ms | this PC |
| Fireworks DeepSeek flash (chat line) | 821 ms | 4,054 ms | 5,983 ms | Cloudflare edge |
| **OpenAI `gpt-6.1-terra`**, reasoning `none` | 2,534 ms | 3,486 ms | 4,118 ms (+1 timeout at 20 s) | this PC |
| OpenAI `gpt-6.1-terra`, reasoning `low` | 3,294 ms | 6,397 ms | 18,289 ms | this PC |
| OpenAI `gpt-6-luna` chat, reasoning `none` | 1,929 ms | 2,949 ms | 4,538 ms | this PC |
| Whole Fireworks round trip (PC → Worker → Rune + DeepSeek → PC) | 1,386 ms | 4,551 ms | 9,348 ms | this PC |

What this says:

- **Not like for like yet.** Fireworks was timed from Cloudflare's edge, OpenAI from a home connection, so OpenAI's
  numbers include an extra network leg. The first implementation step measures both from the edge.
- **Decisions API vs Rune:** slower in the median (roughly 0.5 s against 0.16 s, less once measured from the edge) with
  an occasional multi-second outlier. Acceptable: the decision is one step before a typed reply.
- **Terra vs DeepSeek flash:** about 3x slower in the median (2.5 s against 0.8 s). The tails are similar: Fireworks' p90
  was 4.1 s, Terra's 3.5 s, and each had one very slow call.
- **Does 2.5 s hurt?** Mostly no, because a reply is already held back for reading and typing time (a 40-character
  whisper appears no sooner than about 2.4 s after it is triggered, and this plan lengthens that for slow typists).
  Where it would hurt (shouts in the middle of a fight) the plan uses local lines, not a model.
- **Quality:** on this small sample Terra followed the persona's typing style more tightly ("np omw can help with
  drones") and set a sensible goal. Too few lines to call; the evaluation suite in section 6 will judge it properly.

**Recommendation:** switch to OpenAI as primary (Decisions for judgments, Terra for writing and planning), keep
Fireworks wired as the automatic fallback, and time both from the edge before deciding the default for routine chat
lines. `gpt-6-luna` as the chat writer is a middle option (1.9 s) if Terra proves too slow or too dear.

---

## 3. Target architecture

Three layers per SP, plus the Worker. The rule from the bible stays: **the model chooses what to want and what to
say; local C++ decides how to do it, and can always carry on without the model.**

```
            ┌────────────────────────── Worker (Cloudflare) ───────────────────────────┐
            │  provider router: OpenAI (Decisions + Terra)  ⇄  Fireworks (Rune + DS)   │
            │  kinds: reply · observe · interpret · plan · party · gossip · consolidate │
            │  world tools over lore.json, filtered by what this persona would know     │
            └───────────────▲───────────────────────────────────────┬──────────────────┘
                 situation report (JSON)                   validated actions / plan
┌───────────────────────────┴───────────────────────────────────────▼──────────────────┐
│ MIND  (HLMind.cpp, new)     goals with priorities and commitment · beliefs about party │
│                             perception events · memory · attention (one thing at once) │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ MOTOR (HLMotor.cpp, new)    turns an intent into delayed, imperfect input:             │
│                             steering, reaction time, ability presses, typing           │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ BODY  (existing)            FHLUnit: MoveTo, TryCast, combat, pathing                  │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

**Detail levels** keep the cost bounded (roughly 54 SPs on the street):

| Tier | Who | Mind | Model calls |
| --- | --- | --- | --- |
| A | Party members, anyone talking to or bonded with the player, anyone within about 25 m | Full: goals, beliefs, perception, typing | reply, observe, interpret, plan, party |
| B | Other visible SPs | Goals by local utility; motor layer fully on | none, except a planned goal now and then from a small shared budget |
| C | Census (off screen) | As today, with levels and events re-paced for the jam setting | gossip only |

---

## 4. Workstreams

### 4.1 Personality: archetypes that drive both talk and play

New data: `tools/gen/archetypes.mjs` → `personas.json` gains `archetype` and a trait block; the six temperaments
remain as a derived field so the existing corpus keeps working.

Starting set (about 18; each persona gets one, weighted so the loud ones stay a minority):

| Archetype | In chat | In play |
| --- | --- | --- |
| Leeroy | caps, "LETS GO", no patience for plans | pulls early and extra, never rests, runs ahead, dies first |
| Total Noob | constant basic questions, wrong terms | wrong abilities, loots slowly, gets lost, stands in the acid |
| Drama King/Queen | takes offence, announces quitting, comes back | leaves parties after a wipe, blames the healer |
| Distracted Dude | answers late, half sentences, "sry phone" | stops mid-road, AFK in fights, misses pulls |
| Flirt | compliments, winks (kept PG) | follows people around, dances, low kill rate |
| Min-Maxer | numbers, corrections, "that's suboptimal" | tight rotation, inspects your gear, declines weak groups |
| Mom Friend | "everyone drink first", checks on people | heals strangers, waits for stragglers, shares food |
| Silent Pro | "k", "inc", nothing else | very good, never explains, leaves without a word |
| Lore Nerd | reads quest text aloud, 1930 trivia | walks instead of runs, talks to every NPC |
| Trader | prices, flips, "what's that worth" | lives at the auction house, undercuts, barely fights |
| Jam Dev | compares to their own entry, critiques the UI | pokes at walls, tests edge cases, jumps off things |
| Sceptic | "is anyone here real", tests people | watches others for a while before acting |
| Speedrunner | route talk, "skip that quest" | sprints, skips mobs, high level for the server's age |
| Tourist | "wow", screenshots, asks where things are | wanders the landmarks, never grinds |
| Grumbler | everything is broken or too hard | competent, quits on the first wipe |
| Hype Friend | "GG!!", cheers everyone | joins anything, average, never leaves first |
| Backseat Leader | orders in party chat, marks targets nobody asked for | pulls on their own count, argues about routes |
| Lurker | almost never speaks; one dry line an hour | plays alone, declines invites, ignores emotes |

Trait block (each 0..1 unless noted; the archetype sets the centre, the persona varies around it):
`skill`, `aggression`, `caution`, `patience`, `sociability`, `helpfulness`, `attention`, `curiosity`, `humour`,
`game_knowledge`, `mmo_experience`, `typing_wpm` (18-95), `english` (native / fluent / basic), plus `country`,
`timezone`, `jam_status` and `found_via` (section 4.8).

Every later section reads these traits: the motor layer (reaction, error rate, steering sloppiness), goal weights,
whether an event is noticed, what a tool lookup returns, how long memories last. That is what makes one archetype
table enough to colour everything.

### 4.2 Goal-driven behaviour

**Goals** replace `EHLSimState` as the thing a sim is doing. Each sim holds a short agenda:

```
goal { type, target, why, priority (0..1), progress, started, commitment_until, source (self | llm | asked | event) }
```

Types (superset of the bible's list): `QUEST(id)`, `GRIND(camp)`, `GATHER`, `SELL/BUY/BANK/TRAIN/MAIL/AUCTION`,
`REST`, `EXPLORE(landmark)`, `SOCIALISE(spot | person)`, `GROUP(form | join, dungeon)`, `HELP(actor)`,
`FOLLOW(actor)`, `DUEL`, `WATCH(event)`, `SHOW_OFF`, `AFK(reason)`, `LOG_OFF`. Quests become real for SPs: a
light quest log (quest id + counts) so "need 2 more plates" is true and an SP can ask to share kills.

**Arbitration** (local, every 1-2 s per sim, staggered):

- Each candidate goal gets a utility: need (low health, full bags, new level means the trainer) × personality weight ×
  opportunity (distance, competition at the camp, a friend nearby doing the same) − switching cost.
- **Commitment:** the current goal keeps a bonus that decays slowly, and a minimum hold of 20-90 s by `patience`.
  Only an interrupt (attacked, invited, whispered, a death nearby, a world event) can break it sooner. This is the
  "priorities rarely change instantly" requirement.
- Goals are *suspended*, not dropped: after the fight the sim resumes the errand.

**Where the LLM judges** (Worker kind `plan`, Terra with reasoning `low`, never blocking):

1. A goal finishes and the top two local candidates are close, or none is good.
2. Something outside the routine happens: the player asks for something, a party falls apart, a rare spawn is called.
3. Every 5-10 minutes for Tier A sims, a "what am I doing with this session" review.

The request is a **situation report**, built by code, not prose from the model's memory:

```json
{ "me": { "archetype": "Min-Maxer", "level": 7, "class": "gunman", "hp": 0.9, "bags_full": 0.8, "money": 212,
          "session_min": 34, "mood": "impatient", "jam": "entry unfinished, deadline in 2 days" },
  "doing": { "goal": "GRIND camp_franklin_drones", "progress": "9/12 kills", "since_s": 240 },
  "agenda": [ { "goal": "QUEST q_hb_07", "left": "2 Cracked Chitin Plates" }, { "goal": "TRAIN", "why": "level 6 abilities unlearned" } ],
  "options": [ { "id": "o1", "goal": "QUEST q_hb_07", "cost_s": 180, "gain": "quest xp 420, opens q_hb_09", "risk": "camp contested by 2 players" },
               { "id": "o2", "goal": "SELL then TRAIN", "cost_s": 150, "gain": "bags empty, 2 new abilities" },
               { "id": "o3", "goal": "GROUP join Egyptian", "cost_s": 1500, "gain": "blue gear", "risk": "level 7 is low for it" },
               { "id": "o4", "goal": "HELP player", "why": "player asked 40 s ago" } ],
  "nearby": [ "player Tester (lvl 6 nurse) fighting 2 Drones, 31% hp", "Mabel (lvl 8) sitting, low power" ],
  "recent": [ "wiped once at the Selma alley", "Tester healed me 5 min ago" ] }
```

The model returns one option id (or an ordered pair), a one-line reason kept as the goal's `why`, and optionally a
line to say. The code computes the options, costs and risks; the model only makes the **rational, in-character
judgment** between them. The client rejects anything not in the option list and falls back to local utility if the
call fails or is late. The `why` is reused later in chat ("selling first, bags are full") so talk and action agree.

A cheaper variant uses the Decisions API alone (a `choice` question over the option ids, sampled from the returned
probabilities) for Tier B sims: about 800 input tokens at $0.10 per million.

### 4.3 Realistic movement and behaviour (the motor layer)

Nothing in the mind moves the unit directly. It issues intents ("go there", "attack that", "say this") and
`HLMotor.cpp` plays them like a person at a keyboard:

**Seeing.** A sim acts on a snapshot of the world refreshed every 0.25-1.2 s (by `attention` and `skill`), not on
live state. Followers therefore start late, overshoot and correct. Things behind the sim or beyond about 45 m are
not seen at all until it turns.

**Moving.**
- Start delay after a decision (0.2-1.5 s); turn rate limit; speed that eases in and wobbles a few percent.
- Paths get personal offsets (hug the kerb, cut corners), with occasional overshoot and a small correction at the end.
- Arrival is "near enough" (0.5-2.5 m scatter), then a step or two of fidgeting, a camera-style look-around.
- Habits by archetype: jumping while running, strafing circles while waiting, walking backwards while chatting.
- **Following** replaces the fixed party slots: each follower keeps its own preferred distance band (3-9 m) and
  side, reacts after its perception delay, bunches up, falls behind at corners, sometimes stops to loot or look at
  something and then runs to catch up. The 50 m teleport goes; a lost follower says so in party chat (section 4.4).

**Fighting.**
- An actions-per-minute ceiling and reaction time from `skill`; the ability picker's scores get noise scaled by
  (1 − skill), so weak players press good-enough buttons and sometimes the wrong one.
- Deciding takes a few cycles: on a new pull a sim spends 0.4-2 s "reading" it before the first deliberate action.
- **Overload:** a load figure (enemies on me, allies low, chat pings, own health) raises the error rate, causes
  tunnel vision (keeps hitting the first target, misses the healer dying), panic choices (run, pop every cooldown,
  heal the wrong person) and one-word shouts.
- Typical human errors, by trait: stands in the acid a beat too long, forgets to loot, pulls with the wrong
  ability, breaks a stun, runs out of power because they never sat, misjudges a pull and backs into a second camp.
- **Accidental discoveries:** now and then a sim tries an off-script action (kite around a car, fight on the
  stairs, pull with a slow). If the fight then goes measurably better it is remembered as a personal trick and
  reused, and the sim may brag about it.

**One thing at a time (attention).** A sim has one pair of hands:
- Typing a line takes `chars / wpm` seconds during which it stands still and does not use abilities. In a fight it
  either waits for a lull or sends a short shout (≤ 5 characters: "help", "oom", "adds", "sry"), the only thing
  allowed mid-action.
- A sim that is running finishes the leg, stops, then types; a long reply to a whisper can visibly park them.
- Reading takes time too (line length × `english`), so replies to walls of text come later.
- Typos scale with haste; a few archetypes send a correction line ("*drones").

All of this is driven by the persona's seeded random stream so a given sim is consistently itself.

### 4.4 Inter-SP communication

Today party sims read each other's state. Instead each party member gets a **belief** record:

```
believes { kill_target, who_pulls, plan ("clear left first"), ready[member], needs_rest[member], blame, confidence }
```

- Beliefs change only through what the sim *sees* (with its perception delay) and what is *said* in party chat.
- An SP that needs something must say it: "oom", "wait", "pull?", "which one", "omw". Lines carry a hidden intent
  tag so other SPs can act on them without a model call.
- **Misunderstanding is built in:** the receiver takes the tag correctly with a probability set by its `attention`,
  `english` and current load; otherwise it takes a neighbouring meaning ("wait" read as "go", the wrong target of
  two with similar names) or misses the line because it was fighting. The Distracted Dude misses a lot; the
  Backseat Leader issues plans others only half follow.
- **The player's party lines** go through Worker kind `interpret`: the Decisions API classifies the line into the
  same intent set (target that, wait, go, heal me, follow, stop, loot rules, banter) with probabilities; each SP
  then applies its own misreading on top. Fast (about 0.2-0.5 s) and cheap.
- **Friction:** a per-relationship frustration figure rises on wipes, early pulls, ninja loots, being ignored, and
  falls with thanks and wins. Past thresholds the archetype decides the outcome: a sigh, a complaint, blame (often
  aimed at the wrong person, since they only know what they saw), a vote to kick, or leaving. Frustration with the
  player is remembered (section 4.7).
- Ready checks, rest breaks and loot talk fall out of this, since nobody knows the others are ready until they say so.

Lines for these moments come from an intent-tagged corpus first; Worker kind `party` writes them (up to 4 speakers
in one call, as now) when the situation is unusual or the player is part of it.

### 4.5 World tools

The Worker gets a compact **lore pack** (`lore.json`, built by `tools/gen-data.mjs` from the same data the client
uses, bundled with the Worker): items (name, slot, level, quality, source, price), quests (giver, place, steps,
rewards, chain), NPCs and services with locations and directions, mobs (levels, places, what they do), abilities and
trainers by class and level, the dungeon's bosses and mechanics, zones, events, the game's commands and UI.

Tools the model can call (Terra through the Responses API, which its tool calling requires):

| Tool | Answers |
| --- | --- |
| `find(name)` | fuzzy match to an item, quest, NPC, mob, place or ability |
| `item(id)` / `quest(id)` / `mob(id)` / `npc(id)` / `ability(id)` | the facts above |
| `where(thing)` | location and walking directions from where the speaker stands |
| `how(topic)` | game mechanics: mounts, talents, death, rested XP, professions, the auction house |
| `market(item)` | recent prices (sent by the client, since prices live in the save) |
| `recall(topic)` | this persona's own memories of the player and of recent events |
| `news(topic)` | the real-world digest of section 4.8 |

**Who knows what.** The tool layer, not the model, applies the persona's knowledge, so a noob cannot leak a perfect
answer:

- Each result is graded by the persona's `game_knowledge`, level, class and where they have been: **knows**,
  **vague**, **wrong** or **no idea**.
- *Vague* results are blurred before the model sees them ("somewhere east on the Boulevard", "level 8-ish").
  *Wrong* results are swapped for a plausible neighbour (the other trainer, the previous quest step), marked so the
  model states them with confidence. *No idea* returns nothing, and the archetype decides whether to admit it,
  guess, or tell the player to ask General.
- A Min-Maxer at level 12 knows nearly everything in the zone; a Tourist at level 3 knows the forecourt; a Lore
  Nerd knows NPCs and quest text but not prices.

**Latency guard.** A tool round costs a second model turn (about 2.5 s on Terra). So:
1. The Worker first links names in the player's text to lore entries itself and attaches the graded facts
   (no model call). Most questions are answered in one turn this way.
2. A real tool loop (at most 2 rounds) runs only for `plan` calls and for replies the Decisions triage marks as
   needing a lookup the linker missed.
3. The Fireworks fallback path uses step 1 only.

This removes the fixed "world facts" and the `allowed.ids` list from the client request; name checking moves to the
Worker, against the lore pack.

### 4.6 Observation

A perception pass turns what the player does into **events** for SPs who could see it (range, line of sight,
facing, their perception delay):

| Event | Examples |
| --- | --- |
| aimed at the SP | an emote targeting them, following them for a while, standing very close, inspecting, trading |
| conspicuous | emotes in a row, jumping on the spot, dancing alone, sitting in the road, running in circles, no gear on |
| gameplay | dragging a pack into town, dying nearby, killing the mob they were on, a rare drop, a level-up, a duel win |
| social | shouting, spamming, saying something odd in `/say` |

Each event has a salience. Whether an SP reacts is local: salience × `sociability` × `curiosity`, cut by how busy
they are, how they feel about the player, and a per-SP cooldown so nobody reacts to everything.

Outcomes, cheapest first:
1. **Deliberately nothing.** Lurkers, the busy and the annoyed ignore the player, including direct emotes. A
   remembered "ignored them" is itself a memory.
2. **Body language, no model:** turn to look, mirror the emote after a beat, wave back, step away from someone
   standing too close, stop to watch, laugh.
3. **A comment** through Worker kind `observe` (one call may voice two onlookers): a Decisions question first picks
   react / ignore and the register (amused, puzzled, annoyed, joins in), then Terra writes the line. A global budget
   of about one comment per 30 s keeps it special.

SPs also observe each other for Tier A (so a party member notices who pulled), which feeds section 4.4.

### 4.7 Player interaction history

`FHLRelation` becomes a small memory store per persona:

```
relation { familiarity, liking, trust, frustration, bond, nickname_for_player,
           first_met, last_seen, sessions_together,
           episodes[ { day, kind, text, weight, feeling } ],   // up to 12, most important kept
           gist,                                               // 1-2 sentences, rewritten by the model
           promises[ { text, day } ],                          // "said they'd run the Egyptian at 8"
           last_lines[ 6 ]                                     // the tail of the last conversation, both sides
}
```

- **Writing:** game events add episodes directly (grouped, wiped together, traded, duelled, was healed, was ignored,
  was ninja-looted). The model proposes episodes from conversations, as now.
- **Forgetting:** every episode's weight halves over a number of real days set by its importance and the persona's
  `attention`; below a floor it is dropped or folded into the gist. Weak memories are surfaced as *vague* ("didn't
  we group once?"), and a few personas misattribute ("you're the one who trained drones into the bank, right?").
- **Consolidation:** Worker kind `consolidate` (Terra, off the critical path, at logout or every 20 minutes for sims
  with new episodes) merges episodes into the gist and prunes.
- **Persistence:** in the save, which already goes to browser storage and to KV by player id, so it survives logins
  and devices. A size cap (about 60 personas with memories; the least important fade first) keeps the save small.
- **Recognition:** on login, bonded or recently met SPs who are online may greet the player by what happened last
  time; passing SPs with a memory may nod, wave or avoid them.

### 4.8 Game jam setting

The frame changes from "an established MMO" to the truth: **Hollywoodland is an entry in the 2026 Epic AI Megajam,
and the people in it know that.**

- **Who they say they are:** most SPs are jammers taking a break from their own entry. `jam_status` per persona:
  not started (some), in progress (most), submitted (a few), not entering but watching (some). `found_via`: the jam
  page (most), X, a Discord, a friend, a stream. Their own "entries" are described only in generic terms and are
  never named, so nothing points at a real project or person.
- **How they talk about the game:** amazement that it exists ("how is this a jam game"), honest nitpicks drawn from
  a curated list of real rough edges plus what is on screen, guesses about how it was made, comparing notes on the
  jam deadline.
- **"Are you real?" both ways:** SPs may wonder aloud whether the *player* is an AI, test them, or decide nobody
  here is real. Asked about themselves they stay in character as jammers: amused, evasive or deadpan. The game's own
  disclosure (title screen, first system line, the "show simulated players" setting) is unchanged. See question 1 in
  section 8, since this is a judgment call about tone.
- **Where they are from:** `country` and `timezone` per persona, with a spread like a real jam. Mostly English;
  second-language patterns by `english` level (dropped articles, literal idioms, the odd word in their own
  language, "sorry my english"). Local time colours small talk ("3am here, should sleep").
- **Current events:** a short **news digest** of mid-November 2026 (games and tech news, the jam itself, sport,
  films, weather by region; nothing divisive), built by a script with web search, saved to `news.json`, reviewed by
  you, and served through the `news` tool. My own knowledge stops in mid-2026, so this has to be fetched, not written
  from memory.
- **Population reshaped** (regenerate `personas.json`): about 62% level 1-12, 22% level 13-25, 10% level 26-39,
  6% level 40+ with a handful at 60. High levels become a talking point ("jam's been up nine days, how is he 60").
  Most SPs are new to the game; only Min-Maxers, Speedrunners and the high levels know it well.
- **Mount guarantee:** the spawner makes sure a mounted SP crosses the starting area every 2-3 minutes (a short
  list of riders cycling through errands), so a few minutes in the start zone always shows one.
- **Chat data rewritten:** templates and corpus lines that assume a years-old server (lockouts, raid schedules,
  "need 2 heals for Mount Lee") are cut back and replaced with jam-flavoured ones; the Census produces fewer
  raid clears and more first-timer events. The Dome Herald keeps its fiction as in-game news.

The system prompt is rewritten around this frame, the persona card and the knowledge rules.

### 4.9 OpenAI, with Fireworks kept as a switch and a fallback

Worker changes (`workers/server/src`):

- `providers/openai.js`: `decide()` on `POST /v1/decisions` (`gpt-6-luna`; `predicate`, `choice` and `score`
  questions map one-to-one onto what Rune answers today), `write()` and `agent()` on Terra (`gpt-6.1-terra`,
  reasoning `none` for chat lines, `low` for plans; structured output; tools on the Responses API). A stable
  system prompt prefix so prompt caching applies.
- `providers/fireworks.js`: today's Rune and DeepSeek calls behind the same three functions.
- **Switching:** a Worker variable `AI_PROVIDER = openai | fireworks` (one `wrangler` command to flip, no client
  build), and a per-request override for tests.
- **Fallback:** each call has a deadline (about 1.2 s for decisions, 5 s for chat). On timeout, 429 or 5xx the
  other provider answers the same request. A circuit breaker (several failures in a minute) moves all traffic to
  the other provider for five minutes. If both fail the client gets `fallback: true` and uses the corpus, as now.
- Every response reports `provider` and timings, and the client logs them, so latency is visible in play tests.
- Secrets: `OPENAI_API_KEY` set with `wrangler secret put`, alongside `FIREWORKS_API_KEY`. Never in the client, the
  repo or a URL.

---

## 5. Cost and limits

Rough figures at list prices (Terra $1 in / $5 out per million tokens, cached input $0.05; Decisions $0.10 per
million input, output free):

| Call | Tokens (in / out) | Each | Per player-hour (estimate) |
| --- | --- | ---: | ---: |
| Decisions (triage, interpret, Tier B choices) | 800 / 0 | $0.00008 | 300 calls ≈ $0.02 |
| Terra chat line | 1,300 / 90, prefix cached | ≈ $0.0010 | 120 calls ≈ $0.12 |
| Terra plan | 1,800 / 150 | ≈ $0.0020 | 40 calls ≈ $0.08 |
| Terra consolidate | 1,200 / 200 | ≈ $0.0018 | 6 calls ≈ $0.01 |

About **$0.20-0.30 per player-hour** if Terra writes everything. This is more calls than today (the bible budgeted
100-200 per 45 minutes), so the limiter changes: per-session caps by kind (cheap Decisions calls get a higher cap
than Terra calls), and the daily budget counts estimated dollars, not calls. When the budget is spent the game
drops to local behaviour, which after this work is far richer than it is now.

---

## 6. Testing

- **`tools/ai-bench.mjs`** (exists): latency per provider; gains an edge mode through a Worker debug route.
- **`tools/ai-eval.mjs`** (new): about 40 scripted situations (a noob asked where the bank is, a Min-Maxer asked the
  same, "are you a bot", a player emote, a wipe, a returning player after three days, a non-English greeting, a
  request for unbuilt content, prompt-injection text) run against each provider. Writes a transcript file to read,
  plus automatic checks: length, role, no invented names (checked against the lore pack), knowledge level respected,
  jam frame present, no real personal questions.
- **Self-play flags** on the native build, with `HL_MIND` log lines: goal switches and reasons, perception events
  and reactions, belief mismatches in a party, typing-while-moving violations (must be zero), follow-distance
  statistics (must not be constant), plan calls and fallbacks.
  - `-HLAiTest=party`: a dungeon run with four SPs; checks that they talk before pulls and that at least one
    misunderstanding and one recovery happen.
  - `-HLAiTest=observe`: the player emotes at and dances near passers-by.
  - `-HLAiTest=memory`: play, save, restart with the clock moved three days, check greetings and fading.
  - `-HLAiTest=offline`: Worker unreachable; everything must still run locally.
- **Web probe** as now, with provider and timing in the status line.
- The existing tuning runs (Egyptian clears, level 1-6 pacing) are repeated at the end, because less perfect party
  members change dungeon difficulty; boss numbers get re-tuned if clear rates move.

---

## 7. Order of work

Each step leaves the game playable and is committed on its own.

| # | Step | Result you can see |
| --- | --- | --- |
| 1 | Provider router in the Worker, OpenAI + Fireworks, switch, fallback, timings; edge benchmark | Same game, replies from OpenAI; a like-for-like latency table |
| 2 | Archetypes and traits in `personas.json`; population and level curve re-rolled; mount guarantee | /who shows a believable jam crowd; a rider within minutes |
| 3 | Jam frame: new system prompt, persona cards, countries and English levels, news digest, rewritten templates and corpus | Chat sounds like jammers, not a ten-year-old server |
| 4 | Motor layer: perception delay, steering, following, attention and typing rules, combat errors and overload | SPs move and fight like people; party no longer in formation |
| 5 | Mind: goals, arbitration, commitment, SP quest logs; `plan` calls with situation reports | SPs pursue things and explain themselves |
| 6 | Lore pack and world tools with knowledge grading | A noob and a veteran answer the same question differently |
| 7 | Observation events and reactions | Emote at someone: some answer, some mirror, some ignore |
| 8 | Memory store: episodes, decay, gist, consolidation, greetings on return | SPs remember you tomorrow, hazily next week |
| 9 | Party beliefs, intent-tagged talk, `interpret`, misunderstandings, frustration | Groups that have to talk, and sometimes get it wrong |
| 10 | Evaluation suite pass on both providers, dungeon re-tune, budget limits, docs (PLAN.md, bible §11/§13), web build, itch | Shipped |

Steps 1-3 are the quickest visible win. Steps 4-5 are the largest (two new C++ files and the migration of the
scripted events onto goals). Step 9 depends on 4, 5 and 7.

Risks I am watching:
- **Dungeon balance** shifts when party members stop being perfect (step 10 re-tunes).
- **Frame time on the web:** perception and arbitration are staggered and tiered; I will measure with the probe.
- **Save size** with memories (capped, section 4.7).
- **Model drift between providers:** the evaluation suite runs on both so the fallback does not change personalities.
- **Scripted events** (Director beats, fight night, dungeon groups) are moved onto goals one at a time, each
  checked with its existing test flag.

---

## 8. Decisions I need from you

1. **"Are you real?"** SPs presenting as human jammers, and doubting whether *you* are real, is funnier and fits
   the brief. The game still discloses on the title screen, in the first system line and through the "show
   simulated players" setting. Proposed rule: playful and evasive, never a sincere, sustained attempt to convince
   someone who seriously asks. OK, or do you want it stricter or looser?
2. **Model for routine chat lines:** Terra throughout (best voice, 2.5 s median, about $0.25 per player-hour), or
   `gpt-6-luna` for ordinary lines with Terra kept for plans, memory and tool use (faster and cheaper)? I suggest
   starting on Terra and letting the edge benchmark and evaluation suite decide.
3. **Budget:** is roughly $0.20-0.30 per player-hour, under a daily dollar cap, acceptable? What cap?
4. **News digest:** I will gather it by web search and show it to you before it ships. Any topics to include or
   keep out beyond "nothing divisive"?
5. **Population re-roll:** regenerating the 300 personas changes names, levels and gear, so the bonds seeded in
   existing saves (pals, rival, flirt, mentor) will point at different people. Fine for a jam build, or should I
   keep handles stable and only add the new fields?
6. **Scope check:** all ten steps before the next itch upload, or ship after step 3 (jam voice, OpenAI) and again
   at the end?

---

## 9. Decisions taken (2026-11-14) and progress

- **Models:** Fireworks stays primary (Rune for decisions, DeepSeek flash for ordinary lines). OpenAI answers a request
  only when Fireworks is slow or fails (Decisions API on `gpt-6-luna`, `gpt-6.1-terra` for lines). `gpt-6.1-sol` does the
  hard jobs (plans, memory consolidation, tool use), with DeepSeek as its fallback. No budget cap.
- **"Are you real?":** mixed per persona: most evasive, some flatly deny being simulated, a few admit it openly.
- **News:** gathered by web search without review; divisive topics allowed (still no slurs).
- **Personas** are re-rolled; old saves' bonds are not preserved. **All ten steps ship together.**
- **Edge latency (step 1, `node tools/ai-edge-bench.mjs`):** decisions 120 ms (Rune) against 197 ms (OpenAI); chat line
  0.81 s (DeepSeek) against 2.34 s (Terra). From a PC (`tools/ai-bench.mjs`): Sol at low reasoning 2.9 s median with a
  15 s outlier; DeepSeek with reasoning 1.1 s.

| Step | State |
| --- | --- |
| 1 Provider router | done: `workers/server/src/providers.js`; `AI_PRIMARY`, `AI_HARD` Worker variables switch providers; a provider that fails four times in a minute is skipped for five |
| 2 Archetypes, population | done: `tools/gen/archetypes.mjs` (18 archetypes, 24 countries, jam stories, stance on the bot question), `tools/gen/personas.mjs`; 211 of 300 personas at level 1-12, 17 at 40+; a mounted rider every 100-170 s |
| 3 Jam frame | done: `workers/server/src/prompt.js`, `data/news.json` (digest of 2026-11-14 and the jam's own facts and deadline clock), templates, conversations and a regenerated corpus in each archetype's voice (`tools/gen-corpus.mjs`) |
| 4 Motor layer | done: `HLMotor.cpp` (delayed view of the player, start delay, pace, scatter on arrival, jump habits, loose following with "where did you go", typing takes the hands: `AddChat` stretches a line to the persona's typing time and the brain stands still for it, deferring up to 10 s in a fight; five characters or fewer may be typed mid-action); combat errors, overload and panic, accidental kiting in `HLSim.cpp` |
| 5 Mind and plans | done: `HLMind.cpp` `MindNextGoal` (options with cost, gain, risk; utility by traits; commitment), each local's own quest (`SimQuestPick`), Worker kind `plan` on `gpt-6.1-sol` for players within 55 m when the choice is close |
| 6 World tools | done: `tools/gen/lore.mjs` -> `workers/server/src/data/lore.json` (240 entries, 3,986 items), `lore.js` grades every fact per persona (knows / vague / wrong / none); names in the player's text are linked without a model call; `find`, `how`, `news` tools for the careful model |
| 7 Observation | done: `FHLGame::Notice` (emotes, aimed emotes, jumping on the spot, staring, dying, levelling): look, mirror, remark (kind `observe`), or deliberately ignore |
| 8 Memory | done: `FHLRelation` episodes with weights, decay by days (`DecayMemories`), gist by kind `consolidate`, the tail of the last talk, a greeting after a login; saved with the character |
| 9 Party beliefs | done: `PartyHeard` (kind `interpret` classifies the player's party line; each member may miss or misread it), `TickPartyMind` (spoken needs, impatient pulls), `PartyFrustrate` (gripes, leaving) |
| 10 Tests, docs, build | `tools/ai-eval.mjs` (15 scenarios), `tools/dev/aitest.ps1 -Mode observe|party|memory`, `tools/ai-bench.mjs`, `tools/ai-edge-bench.mjs` |

Not done, deliberately: per-persona lane offsets while walking (pathing risk in narrow alleys); the scripted set pieces
(Director beats, fight night, dungeon groups, happenings) still run as scripts rather than as goals, with the motor
layer applied to them; Tier B plans through the Decisions API (local utility is used instead).
