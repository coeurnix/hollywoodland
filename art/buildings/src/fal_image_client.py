"""User-authorized FLUX3Klein image route. Official fal schemas checked2026-11-13.
Runtime credential only; receipts contain prompts/paths/timings, never keys/dataURIs.
"""
import base64,json,time,sys
from pathlib import Path
from urllib.parse import urlparse
from concurrent.futures import ThreadPoolExecutor,as_completed
import requests
R=Path(__file__).resolve().parents[1]
def key():
 for line in (R.parents[1]/'.env').read_text(encoding='utf-8-sig').splitlines():
  if line.strip().startswith(('FAL_AI_API_KEY=','FAL_KEY=')):return line.split('=',1)[1].strip().strip('"').strip("'")
 raise RuntimeError('fal credential missing')
def generate(job):
 output=(R/job['output']).resolve();output.relative_to(R.resolve());output.parent.mkdir(parents=True,exist_ok=True)
 if output.exists() and not job.get('replace'):return {'id':job['id'],'status':'cached','output':job['output']}
 references=job.get('references',[]);endpoint='blackforestlabs/flux-3/klein/'+('edit-image' if references else 'text-to-image')
 payload={'prompt':job['prompt'],'image_size':job.get('image_size',{'width':1536,'height':1024}),'num_images':1,'enable_prompt_expansion':False,'enable_safety_checker':True,'output_format':'png','seed':job.get('seed',19301113)}
 if references:payload['image_urls']=['data:image/png;base64,'+base64.b64encode((R/p).read_bytes()).decode() for p in references]
 receipt={'id':job['id'],'provider':'fal.ai','model':endpoint,'prompt':job['prompt'],'references':references,'image_size':payload['image_size'],'seed':payload['seed'],'output':job['output'],'status':'submitting'}
 journal=R/'src/fal_receipts'/f"{job['id']}.json";journal.parent.mkdir(exist_ok=True)
 def save():journal.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
 def checked(response):
  if not response.ok:raise RuntimeError('fal HTTP '+str(response.status_code))
  return response.json()
 session=requests.Session();session.headers['Authorization']='Key '+key();start=time.monotonic();save()
 try:
  submit=checked(session.post('https://queue.fal.run/'+endpoint,json=payload,timeout=90));receipt.update(request_id=submit['request_id'],status='queued');save()
  for name in ['status_url','response_url']:
   assert urlparse(submit[name]).hostname=='queue.fal.run','Unexpected authenticated API host'
  while time.monotonic()-start<600:
   state=checked(session.get(submit['status_url'],timeout=45))
   if state['status']=='COMPLETED':break
   time.sleep(1)
  else:raise RuntimeError('fal generation deadline')
  result=checked(session.get(submit['response_url'],timeout=90));item=result['images'][0]
  if item['url'].startswith('data:'):content=base64.b64decode(item['url'].split(',',1)[1])
  else:
   download=requests.get(item['url'],timeout=90)
   if not download.ok:raise RuntimeError('fal image download HTTP '+str(download.status_code))
   content=download.content
  output.write_bytes(content);receipt.update(status='downloaded',elapsed_seconds=round(time.monotonic()-start,2),timings=result.get('timings'),width=item.get('width'),height=item.get('height'));save()
  return {k:receipt[k] for k in ['id','status','elapsed_seconds','output']}
 except Exception as error:
  receipt.update(status='failed',error=str(error).replace(key(),'[REDACTED]').split('https://')[0]);save();return {'id':job['id'],'status':'failed','error':receipt['error']}
if __name__=='__main__':
 jobs=json.loads((R/sys.argv[1]).read_text());jobs=jobs if isinstance(jobs,list) else [jobs]
 with ThreadPoolExecutor(max_workers=int(sys.argv[2]) if len(sys.argv)>2 else 4) as pool:
  for future in as_completed([pool.submit(generate,j) for j in jobs]):print(json.dumps(future.result()),flush=True)
