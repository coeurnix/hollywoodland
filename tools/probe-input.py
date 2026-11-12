"""Manual-input smoke test: types a name, clicks Enter Hollywood, walks, turns with right-drag, opens windows, chats.
usage: python tools/probe-input.py <url> <out-prefix>"""
import sys, time
from playwright.sync_api import sync_playwright
url, out = sys.argv[1], sys.argv[2]
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe', headless=True,
        args=['--enable-unsafe-webgpu', '--enable-features=SharedArrayBuffer', '--ignore-gpu-blocklist', '--use-angle=d3d11'])
    page = b.new_context(viewport={'width': 1600, 'height': 900}).new_page()
    page.on('console', lambda m: print(m.text[:200]) if 'HL' in m.text or 'rror' in m.text else None)
    page.on('pageerror', lambda e: print('PAGEERROR', e))
    page.goto(url); page.wait_for_timeout(12000)
    page.mouse.click(800, 450); page.wait_for_timeout(300)
    page.keyboard.type('Probe', delay=80); page.wait_for_timeout(300)
    page.mouse.click(1060, 389); page.wait_for_timeout(300)          # Electrician
    page.screenshot(path=out + '_a.png')
    page.mouse.click(800, 693); page.wait_for_timeout(3000)          # Enter Hollywood
    page.screenshot(path=out + '_b.png')
    page.keyboard.press('h'); page.keyboard.down('w'); page.wait_for_timeout(1500); page.keyboard.up('w')
    page.mouse.move(800, 300); page.mouse.down(button='right'); page.mouse.move(950, 320, steps=8); page.mouse.up(button='right'); page.wait_for_timeout(400)
    page.keyboard.press('f'); page.wait_for_timeout(800)             # talk to the nearest NPC
    page.screenshot(path=out + '_c.png')
    page.keyboard.press('Escape'); page.keyboard.press('m'); page.wait_for_timeout(600)
    page.screenshot(path=out + '_d.png')
    page.keyboard.press('m'); page.keyboard.press('Enter'); page.keyboard.type('/1 anyone want to group for workers? are you guys real people?', delay=20); page.keyboard.press('Enter')
    page.keyboard.press('o'); page.wait_for_timeout(9000)
    page.screenshot(path=out + '_e.png')
    b.close()
