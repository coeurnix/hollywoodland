"""Open the game page in Edge/Chrome with WebGPU, print console + page errors, optionally self-play and screenshot.
usage: python tools/probe.py <url> [--seconds N] [--stop REGEX] [--shot out.png] [--auto CLASS] [--headed]
  --auto CLASS  sets localStorage hl_auto=<class> so the game creates a character and plays itself (HL_STATUS lines).
Run with the Playwright venv: C:\\Users\\theco\\Projects\\unreal59\\tools-venv\\Scripts\\python.exe"""
import argparse, os, re, time
from playwright.sync_api import sync_playwright

ap = argparse.ArgumentParser()
ap.add_argument('url'); ap.add_argument('--seconds', type=float, default=120); ap.add_argument('--stop', default='')
ap.add_argument('--shot', default=''); ap.add_argument('--auto', default=''); ap.add_argument('--headed', action='store_true')
ap.add_argument('--shots', type=int, default=1, help='number of screenshots spread over the run')
ap.add_argument('--autoplay', action='store_true', help='let audio start without a click (music test)')
ap.add_argument('--browser', default=os.environ.get('PROBE_CHROME', r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'))
a = ap.parse_args()
stop = re.compile(a.stop) if a.stop else None
t0 = time.time()
hit = {'t': None}
def say(kind, text):
    print(f'[{time.time() - t0:7.2f}] {kind}: {text}'[:600], flush=True)
    if stop and stop.search(text) and hit['t'] is None:
        hit['t'] = time.time()
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=a.browser, headless=not a.headed,
        args=['--enable-unsafe-webgpu', '--enable-features=SharedArrayBuffer', '--ignore-gpu-blocklist', '--use-angle=d3d11'] + (['--autoplay-policy=no-user-gesture-required'] if a.autoplay else []))
    ctx = b.new_context(viewport={'width': 1600, 'height': 900})
    if a.auto:
        ctx.add_init_script(f"try {{ localStorage.setItem('hl_auto', '{a.auto}'); }} catch (e) {{}}")
    page = ctx.new_page()
    page.on('console', lambda m: say(m.type, m.text))
    page.on('pageerror', lambda e: say('PAGEERROR', str(e)))
    page.on('requestfailed', lambda r: say('REQFAIL', f'{r.method} {r.url[:150]} {r.failure}'))
    page.goto(a.url)
    shot_n = 0
    while time.time() - t0 < a.seconds and (hit['t'] is None or time.time() - hit['t'] < 2):
        page.wait_for_timeout(500)
        if a.shot and a.shots > 1 and time.time() - t0 > (shot_n + 1) * a.seconds / a.shots:
            page.screenshot(path=a.shot.replace('.png', f'_{shot_n}.png')); shot_n += 1
    if a.shot: page.screenshot(path=a.shot)
    for f in page.frames:
        try: print('frame', f.url[:80], 'isolated:', f.evaluate('crossOriginIsolated'), flush=True)
        except Exception as e: print('frame', f.url[:80], 'eval failed', e, flush=True)
    b.close()
