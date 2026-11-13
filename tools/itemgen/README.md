# itemgen: item descriptions and icons

`itemgen.py` writes a tooltip description and an inventory icon for every item in `hollywoodland-ue/Content/Data/items.json`.

- **Descriptions** come from Claude Opus 5.5 at medium effort, with structured JSON output.
  - Requests are grouped by family: all variants of one base item, or small families that share a slot.
  - Each request carries the same system prompt: `preamble.md` plus world sections 3, 4, 5, 7, 8, 9 and 10 of `DESIGN-BIBLE.md`. That prompt is cached for an hour.
- **Icons** come from FLUX 3 Klein on fal.ai (`blackforestlabs/flux-3/klein/text-to-image`) at 1024×1024.
  - The prompt is a fixed house style in `ICON_STYLE` plus a short subject brief that Claude writes alongside each description.
  - Prompt expansion is off, and each item gets a fixed seed derived from its key, so results are repeatable.

Items that share a name are the same object at different level bands. They share one description and one icon, keyed by the slugged name (`Newsboy Cap of the Deacon` becomes `newsboy_cap_of_the_deacon`): 1,447 keys for 3,962 item ids.

## Setup

```
pip install -r tools/itemgen/requirements.txt
```

Set these environment variables:

- `ANTHROPIC_API_KEY`: needed for the `text` step.
- `FAL_KEY`: needed for the `icons` step.

The script reads keys only from the environment and never writes them anywhere.

## Steps

| Command | What it does |
| --- | --- |
| `python tools/itemgen/itemgen.py plan` | Shows counts and batch layout and writes `state/sample_request.txt`. Needs no keys. |
| `... text` | Generates descriptions and icon briefs, using parallel streaming requests (`--workers 4`). |
| `... text --batch-api` | Does the same through the Message Batches API: half price, asynchronous. Rerun the command to collect a batch that is still running. |
| `... icons` | Fetches the 1024×1024 originals into `item-art/icons-1024/`. |
| `... atlas` | Writes the 96×96 icons to `item-art/icons-96/`. Packs them into 2048² sheets in `item-art/atlas/item_icons_N.png` (441 icons per sheet, 21 × 21) and writes the mapping to `item-art/atlas/item_icons.json`. |
| `... apply` | Adds `desc` and `icon` (a global atlas index) to each row of `items.json`. |
| `... review` | Writes the contact sheet `item-art/review.html`, with names, descriptions, icon briefs and icons at 64 and 32 px. |
| `... all` | Runs text, icons, atlas, apply and review in order. |

Options:

- **Pilot runs:** `--max-batches N`, `--max-icons N`.
- **Targeting:** `--only key,key` limits a step to those keys; `--redo key,key` (or `--redo all`) regenerates them.
- **Tuning:** `--batch-size` (default 24), `--effort`, `--workers`.

## Resume and state

**Resume.** Every step skips finished work, so the same command can be rerun after an interruption.
- Descriptions are saved after each request.
- A fal request id is saved as soon as it is submitted. An interrupted icon run polls the same request instead of paying for a new image.

**Briefs and icons.** If a description's icon brief changes (after `text --redo`), the next `icons` run remakes that icon.

**Atlas slots.** Slots are append-only, so an icon's index never moves.

**`gen-data.mjs`.** It reads `state/descriptions.json` and `item-art/atlas/item_icons.json`, so a regenerated `items.json` keeps its `desc` and `icon` fields.

**What is committed.**
- Committed: `state/`, the 96 px icons and the atlas.
- Local only (gitignored): the 1024 px originals in `item-art/icons-1024/`, about 1–2 GB for all items. Back them up separately if you want to keep them.

**Atlas lookup.** For an item's `icon` value `i`:
- sheet = `i // 1024`
- x = `(i % 32) * 64`
- y = `((i % 1024) // 32) * 64`
