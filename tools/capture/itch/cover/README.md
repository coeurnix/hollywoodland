# Hollywoodland itch.io cover

Upload **`hollywoodland-cover-630x500.png`** to the game's **Cover image** field. This is the finished, exact **630 x 500** RGB PNG, **588,763 bytes**. It is not the page banner or background image.

The [official itch.io getting-started guide](https://itch.io/docs/creators/getting-started) specifies the 315:250 aspect ratio and recommends a larger image such as 630 x 500. Checked November 15, 2026.

## Design

A lively Hollywood Boulevard / Chinese Theatre social scene, with oversized gold-and-ivory outlined creator-thumbnail lettering:

> HOLLYWOODLAND:
> THE MMORPG

One yellow supporting hook reads **BUILT IN 4 DAYS!** The boulevard, crowd and lettering remain legible in the saved `preview-315x250.png` browse-grid preview.

## Source and production

The selected basis is actual in-engine capture footage: `../1-picture-night-crowd.gif` (frame at approximately 4 seconds) and `../2-hive-surge-crowd.gif` (first frame). The extracted references are preserved as `source-picture-night-later.png` and `source-hive-surge.png`.

Two passes used the **built-in imagegen tool**:

1. `01-scene-master.png`: remove HUD and labels, reframe the boulevard and crowd, improve exposure, and compose a more readable promotional scene.
2. `02-lettered-master.png`: add the title and four-day hook. Original generated output: **1408 x 1117**, preserved without recompression.

The cover is an **AI-altered promotional image based on game captures**, not an unmodified screenshot: the camera, crowd arrangement and character details have been reconstructed. Actual gameplay GIFs remain the appropriate evidence of in-game appearance.

FFmpeg performed only the final Lanczos resampling to exact 630 x 500 and the 315 x 250 preview. No script/model fallback generated the artwork. Exact generation prompts: [PROMPTS.md](PROMPTS.md).

Fresh high-resolution capture attempts also remain in the local `hollywoodland-ue/Saved/Capture/itch-cover-20261115/` and `itch-cover-crowd-20261115/` folders; their crowd/framing was weaker, so they were not used for the final cover. No game code or existing content assets were changed. Nothing was uploaded to itch.io.
