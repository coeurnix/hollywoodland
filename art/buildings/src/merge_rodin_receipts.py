"""Merge isolated receipts after all API workers have exited; then reconcile usage."""
import json
from pathlib import Path
R=Path(__file__).resolve().parents[1];path=R/'rodin_ledger.json';ledger=json.loads(path.read_text())
for p in sorted((R/'work/rodin_receipts').glob('*.json')):
 d=json.loads(p.read_text())
 for call in d['calls']:
  found=next((i for i,c in enumerate(ledger['calls']) if c['asset']==call['asset'] and c['attempt']==call['attempt']),None)
  if found is None:ledger['calls'].append(call)
  else:ledger['calls'][found]=call
 if d.get('deferrals'):ledger.setdefault('unbilled_capacity_deferrals',{})[p.stem]=len(d['deferrals'])
ledger['credits_used']=sum(c['credits_consumed'] for c in ledger['calls'])
path.write_text(json.dumps(ledger,indent=2)+'\n')
print('Merged receipts. Recorded credits:',ledger['credits_used'])
