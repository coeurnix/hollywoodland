# Hollywoodland

Simulated, player-populated MMORPG set in Hollywood, 1930 (alien insect occupation). Single-player; every other "player" is AI.
UE 5.9.0 Preview (source build) → WebAssembly/WebGPU, served by a Cloudflare Worker, embedded on itch.io.
Design reference: `DESIGN-BIBLE.md`. Roadmap and status: `PLAN.md`. Placeholder inventory: `ASSETS.md`.

- Client: https://hollywoodland.coeurnix.workers.dev/
- Server API: https://hollywoodland-server.coeurnix.workers.dev/

## Layout

| Path | What |
|---|---|
| `hollywoodland-ue/` | UE project. All gameplay is C++ + Slate in `Source/Hollywoodland`; world is built procedurally from `Content/Data/*.json`. |
| `tools/gen-data.mjs`, `tools/gen/` | Content generator → `hollywoodland-ue/Content/Data/*.json` (items, mobs, zone layout, quests, personas, chat). |
| `workers/server/` | `hollywoodland-server` Worker: sessions, `/v1/sim` (Rune decisions + DeepSeek chat via Fireworks), cloud saves (KV). |
| `workers/client/` | `hollywoodland` Worker: serves the packaged Wasm stage from R2 with COOP/COEP, Brotli siblings and Range. |
| `workers/deploy-stage.mjs` | Uploads a packaged stage to R2 under a new prefix and flips the live `DEPLOY` var. |
| `itch/` | The itch.io HTML wrapper (iframe to the client Worker). |

## Build and deploy

```powershell
node tools/gen-data.mjs                                   # regenerate Content/Data (deterministic)
hollywoodland-ue\Scripts\BuildWeb.ps1                      # editor + Wasm Shipping build, cook, package -> web-dist-Shipping\Wasm (~8 min cold)
cd workers; node deploy-stage.mjs ../hollywoodland-ue/web-dist-Shipping/Wasm v<N>   # upload + switch live build
cd workers/server; npx wrangler deploy                    # API worker (secrets: FIREWORKS_API_KEY, SESSION_SECRET)
```

Native test run (fast iteration, same code): `hollywoodland-ue\Scripts\RunNative.ps1`.
Engine: `C:\Users\theco\Projects\unreal59\UnrealEngine` (5.9.0-preview-1, MSVC 14.50, Emscripten 6.0.10).

## Engine/web constraints that shaped the code (do not rediscover)

- `FHttpModule` is null on Wasm and the game thread never yields to JS: all HTTP/localStorage goes through `HLBridge` (EM_JS on the
  browser main thread via `emscripten_proxy_async`, results polled each tick). Pointers cross as `double` (wasm64).
- Shipping web builds have no `UE_LOG`; use `HLBridge::Log` (console.log).
- Quote URLs in ini files. `FTickableGameObject` CDOs tick unless excluded.
- No authored map/Blueprint assets: the cook only needs `/Engine/Maps/Entry`, `/Engine/BasicShapes`, `/Game/HL` (materials made by
  `Scripts/make_assets.py`) and `Content/Data` (staged as UFS).
- Wasm target uses the `WebMinimal` trim profile: no nav mesh (own grid A*), no StateTree, no ICU. Cook must pass
  `-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge`.
- Hosting needs R2 (files > 25 MiB), COOP/COEP, raw 206 ranges, `.br` siblings with `encodeBody: 'manual'`.
