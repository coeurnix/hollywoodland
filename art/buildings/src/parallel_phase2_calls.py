"""Concurrent texture jobs with isolated receipts; no shared ledger writes.
User authorized this on 2026-11-13. Merge receipts after the legacy queue exits.
"""
import json,time,importlib.util
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor,as_completed
R=Path(__file__).resolve().parents[1];J=R/'work/rodin_receipts';J.mkdir(exist_ok=True)
original=json.loads((R/'rodin_ledger.json').read_text())
tasks=[{'id':aid} for aid in ['MSN_G_Bay6A','MSN_Parapet4','MSN_Awning6']]
tasks+= [a for a in json.loads((R/'work/dressing/texture_request_queue.json').read_text()) if a['id'] not in ['DR_BoardedWindow15','DR_BoardedWindow23','DR_SootA','DR_SootB']]
def job(a):
 aid=a['id'];receipt=J/f'{aid}.json'
 if not receipt.exists():
  previous=[c for c in original['calls'] if c['asset']==aid]
  receipt.write_text(json.dumps({'credits_used':sum(c['credits_consumed'] for c in previous),'calls':previous,'deferrals':[]},indent=2)+'\n')
 spec=importlib.util.spec_from_file_location('isolated_'+aid,R/'src/rodin_client.py');client=importlib.util.module_from_spec(spec);spec.loader.exec_module(client);client.LEDGER=receipt
 start=time.monotonic()
 while True:
  ok=client.run(aid,reference_path=a.get('reference'),model_path=a.get('model'),prompt=a.get('prompt'))
  d=json.loads(receipt.read_text());last=d['calls'][-1]
  if ok:return aid,'downloaded'
  error=last.get('error','')
  if last.get('task_uuid') or not any(s in error for s in ['429','API_PARALLELISM_LIMIT_REACHED']):return aid,'needs review: '+error
  # Explicitly rejected submissions consumed no credits and no generation attempt.
  d['deferrals'].append(d['calls'].pop());receipt.write_text(json.dumps(d,indent=2)+'\n')
  if time.monotonic()-start>1200:return aid,'capacity deferral deadline'
  time.sleep(30)
with ThreadPoolExecutor(max_workers=3) as pool:
 futures=[pool.submit(job,a) for a in tasks]
 for future in as_completed(futures):print('PARALLEL_RESULT',future.result(),flush=True)
print('Isolated receipts complete; merge only after the legacy queue has exited',flush=True)
