"""Generic concurrent texture-only queue with isolated receipts and bounded deferral."""
import json,time,importlib.util,sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor,as_completed
R=Path(__file__).resolve().parents[1];J=R/'work/rodin_receipts';J.mkdir(exist_ok=True)
tasks=json.loads((R/sys.argv[1]).read_text());tasks=tasks.get('assets',tasks) if isinstance(tasks,dict) else tasks
original=json.loads((R/'rodin_ledger.json').read_text())
def run_job(a):
 aid=a['id'];receipt=J/f'{aid}.json'
 if (R/f'work/{aid}/base_basic_shaded.glb').exists() and not a.get('retry'):return aid,'cached'
 if not receipt.exists():
  previous=[c for c in original['calls'] if c['asset']==aid]
  receipt.write_text(json.dumps({'credits_used':sum(c['credits_consumed'] for c in previous),'calls':previous,'deferrals':[]},indent=2)+'\n')
 spec=importlib.util.spec_from_file_location('isolated_'+aid,R/'src/rodin_client.py');client=importlib.util.module_from_spec(spec);spec.loader.exec_module(client);client.LEDGER=receipt
 model=a.get('model',a.get('input',a.get('input_glb')))
 if aid.startswith('LM_') and not a.get('prepared_proxy'):
  from texture_proxy import prepare
  proxy=prepare(R/(model or f'work/{aid}_input.glb'),R/f'work/{aid}_texture_proxy.glb');model=proxy.relative_to(R).as_posix()
 start=time.monotonic()
 while True:
  existing=json.loads(receipt.read_text())['calls'];second=a.get('second_reference',a.get('secondary_reference'));reference=second if existing and second else a.get('reference')
  ok=client.run(aid,reference_path=reference,model_path=model,prompt=a.get('prompt','Hand-painted early classic MMO1930 Hollywood architecture. Follow the reference architectural colors and materials, restrained painted brushwork, warm top light, cool corner ambient occlusion. Finish all roofs, sides and backs as actual materials. Painted opaque dark glass. Keep sign panels blank. No lettering, people, vehicles, sky, ground or added objects.'))
  d=json.loads(receipt.read_text());last=d['calls'][-1]
  if ok:return aid,'downloaded'
  error=last.get('error','')
  if last.get('task_uuid') or not any(s in error for s in ['429','API_PARALLELISM_LIMIT_REACHED']):return aid,'needs review: '+error
  d['deferrals'].append(d['calls'].pop());receipt.write_text(json.dumps(d,indent=2)+'\n')
  if time.monotonic()-start>1200:return aid,'capacity deferral deadline'
  time.sleep(30)
def job(a):
 lock=J/(a['id']+'.lock')
 try:
  handle=lock.open('x');handle.write(str(time.time()));handle.close()
 except FileExistsError:return a['id'],'already in flight'
 try:return run_job(a)
 finally:lock.unlink(missing_ok=True)
with ThreadPoolExecutor(max_workers=int(sys.argv[2]) if len(sys.argv)>2 else 4) as pool:
 for future in as_completed([pool.submit(job,a) for a in tasks]):print('TEXTURE_RESULT',future.result(),flush=True)
