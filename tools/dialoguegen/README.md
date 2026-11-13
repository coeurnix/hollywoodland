# dialoguegen: spoken dialogue for Hollywoodland

Writes every NPC, quest, player bark and emote line with Claude Opus 5.5, casts voices, speaks them with
ElevenLabs v4 (`elevenlabs/tts/eleven-v4` on fal.ai), and masters the audio for the game.

```
python tools/dialoguegen/dialoguegen.py plan       # jobs, line counts, character estimate, a sample request
python tools/dialoguegen/dialoguegen.py text       # Claude writes each job (37: 28 NPC scripts, 8 classes, emotes)
python tools/dialoguegen/dialoguegen.py cast       # Claude picks a stock voice + settings per speaker
python tools/dialoguegen/dialoguegen.py apply      # -> hollywoodland-ue/Content/Data/dialogue.json (subtitles)
python tools/dialoguegen/dialoguegen.py review     # -> tools/dialoguegen/review.md (read this before speaking)
python tools/dialoguegen/dialoguegen.py tts        # fal.ai speech -> vo-src/ (raw, not committed); billed
python tools/dialoguegen/dialoguegen.py soften     # rewrites lines fal's content checker flagged, then apply + tts again
python tools/dialoguegen/dialoguegen.py master     # -> sfx/vo_*.mp3 (-17 LUFS, mono 64 kbps) + Content/Data/vo.json
```

Options: `--only <job or speaker or sound ids>`, `--redo <job ids|all>` (text), `--effort` (default high),
`--workers`, `--max-lines N` (tts pilot). Keys come from the root `.env` (`ANTHROPIC_API_KEY`, `FAL_AI_API_KEY`) and are
never printed.

Every step is resumable: `state/lines.json` (scripts), `state/cast.json`, `state/tts.json` (per line: text+voice hash,
fal request, file). Changing a line or a voice regenerates only that line's audio. A `text` job that fails a check
(line counts, quest offers that don't state their objective counts) is reported and can be redone with `--redo`.

## What the game does with it

- `dialogue.json` holds the subtitles: `npcs{id:{kind:[lines], directions:{poi:line}}}`, `quests{id:{offer,progress,complete}}`,
  `players{<class>_<m|f>:{kind:[lines]}}`, `emotes{id:{self_alone,self_target,aliases,anim,voice}}`. Audio tags in
  `[brackets]` are spoken directions for the voice and are stripped from subtitles.
- Line *n* of a kind is audio variant *n* of `vo_<speaker>_<kind>`, so what you read is what you hear. Patrolmen share one
  script in three voices (`variant = line * 3 + voice`). Quest audio is `vo_<quest>_<offer|progress|complete>`.
- `HLDialogue.cpp` plays them: hello/first greeting on talk, goodbye when the window closes, an annoyed line after five
  quick clicks, a service line when a vendor/trainer/bank/auction/inn/red-car window opens, quest text read aloud, guard
  directions (which also mark the map), player barks on stock errors, level-up, quest done and low health, and the emotes.
- Everything is optional: without `dialogue.json` the game keeps its stock lines; without `vo.json` it is silent.

## Voices and rules

Stock ElevenLabs library voices only: no cloned voices (real people such as Sid Grauman, Max Factor and Chief Davis
appear as characters; design bible §2). The writer's brief is `preamble.md` plus design-bible sections 3, 4, 5, 7 and 10.
fal's content checker rejects some combat talk as "violence"; `tts` records those lines as flagged and `soften` rewrites
just them with period euphemism, keeping counts and jokes.

Speech costs $0.08 per 1,000 characters: the full set is 1,367 lines, 99,680 characters, about $8 (2026-11).
`text` and `cast` print their token usage and an estimate when they finish.
