"""Run four-view reviews in isolated Blender processes, then refresh checkpoint.

No API use. Only selects IDs named on command line; root publishes after review.
"""
from pathlib import Path
import subprocess,sys,concurrent.futures,json
R=Path(__file__).resolve().parents[1]
B=r'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'
def review(aid):
    for script in ['render_brk_candidate.py','render_landmark_boxes.py']:
        with (R/f'work/{aid}_{script.removesuffix(".py")}_signed.log').open('w') as log:
            result=subprocess.run([B,'--background','--python-exit-code','1','--python',str(R/'src'/script),'--',aid],stdout=log,stderr=subprocess.STDOUT)
        if result.returncode:raise RuntimeError(f'{aid} {script}: see isolated log')
    return aid
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for aid in pool.map(review,sys.argv[1:]):print('SIGNED_REVIEW_READY',aid,flush=True)
