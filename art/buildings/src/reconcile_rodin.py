"""Reconcile submission reservations against official account usage, without saving credentials."""
import json,datetime,requests
from pathlib import Path
from rodin_client import key
R=Path(__file__).resolve().parents[1];p=R/'rodin_ledger.json';ledger=json.loads(p.read_text())
since=ledger.get('started_date_utc','2026-11-13');records=[];complete=False
session=requests.Session();session.headers['Authorization']='Bearer '+key()
for page in range(1,51):
 r=session.get('https://api.hyper3d.com/api/v2/check_balance',params={'usage_page':page},timeout=60);r.raise_for_status();d=r.json()
 if d.get('error'):raise RuntimeError('Account usage error: '+d['error'])
 rows=d.get('usage',[]);records.extend(rows)
 if not rows or min(u['time'][:10] for u in rows)<since:complete=True;break
if not complete:raise RuntimeError('Usage window incomplete; ledger unchanged')
known={c['task_uuid'] for c in ledger['calls'] if c.get('task_uuid')}
charges={}
for row in records:
 if row.get('task_uuid') in known:charges[row['task_uuid']]=charges.get(row['task_uuid'],0)+row['amount']
for call in ledger['calls']:
 call.setdefault('credits_reported_on_submission',call['credits_consumed'])
 uuid=call.get('task_uuid')
 if uuid in charges:call['credits_consumed']=charges[uuid];call['billing_status']='matched account usage'
 elif call['status']=='failed':call['credits_consumed']=0;call['billing_status']='failed submission/task absent from complete usage window'
ledger['credits_used']=sum(c['credits_consumed'] for c in ledger['calls'])
ledger['started_date_utc']=since;ledger['reconciled_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
ledger['reconciliation']={'complete_since_date':since,'matched_charged_tasks':len(charges),'unassigned_tasks_in_window':sum(u['time'][:10]>=since and u.get('task_uuid') not in known for u in records)}
p.write_text(json.dumps(ledger,indent=2)+'\n')
filtered=[{k:u[k] for k in ['time','item','amount','task_uuid']} for u in records if u.get('task_uuid') in known]
(R/'work/account_usage_check.json').write_text(json.dumps(filtered,indent=2)+'\n')
print('Reconciled credits:',ledger['credits_used'],'charged tasks:',len(charges),'unassigned:',ledger['reconciliation']['unassigned_tasks_in_window'])
if ledger['credits_used']>200:raise RuntimeError('STOP: over 200 credits')
