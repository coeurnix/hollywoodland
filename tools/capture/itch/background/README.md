# Hollywoodland itch.io page background

Upload `hollywoodland-itch-background.png` through **Edit Theme > Images > Background** on the game page.

Recommended settings:

- **Repeat:** None / No repeat. This is a single atmospheric illustration, not a seamless tile.
- **Align:** Center.
- **Fixed:** Enabled, so the content scrolls over the stationary artwork.
- **BG color:** `#111b23`, a dark fallback close to the vignetted image edges.
- **BG2 / content column:** `#17212b`, with BG2 Alpha at 100% as a starting point. Keep the description on an opaque panel for readability.
- Optional matching text: `#eee4ce`; links/buttons: muted gold `#d7af67`.

These are recommended theme choices, not itch.io requirements. No custom CSS is needed. Itch.io supports repeated backgrounds but does not require repetition; this composition is intentionally non-repeating. The official design guide does not prescribe a single mandatory background aspect ratio or pixel size.

The delivered original is **1672 x 941 pixels**, RGB PNG, **1,704,383 bytes** (about 1.7 MB). It has not been enlarged or recompressed. On wider screens the fallback color surrounds the image; this setup does not assume that the theme editor provides automatic cover scaling. The subdued edges reduce the transition, but it is not a mathematically seamless color match. On narrow displays most architectural detail may be hidden, deliberately leaving the page content dominant.

The standard itch.io content column is 960 pixels wide, though embedded-game settings can affect the actual layout. The quiet central composition accommodates that column; the low-contrast outer theatres, palms, hills and faint containment dome echo the approved title painting without competing with it. Use the title painting separately as the game's key art.

Before saving the theme, preview a narrow mobile width, a typical 1920-wide desktop, a larger desktop, and the bottom of the description. Keep the content panel opaque if readability suffers. This asset was inspected locally; it has not been uploaded or tested on the live itch.io page.

Sources checked November 15, 2026:

- [Official itch.io page design guide](https://itch.io/docs/creators/design): background images, BG/BG2 colors, content opacity and responsive pages.
- [Official itch.io CSS guide](https://itch.io/docs/creators/css-guide): default content column width of 960px. Custom CSS is not required for the recommended setup.
- [Star West's community image guide](https://hedgiespresso.itch.io/itch-page-image-templates#background-images): Background upload, Repeat, Align and Fixed controls. This is a community guide, not an official specification.

Generated with the **built-in imagegen tool**, referencing `suggested-improvements-v2/title/hollywoodland-title-proposed.png`. Exact prompt: [PROMPT.md](PROMPT.md). The supplied PNG is the unmodified generated master. No game code, existing game assets, page description, or live page settings were changed.
