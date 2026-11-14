"""Exact nominal storefront inventory. Family neutral props are planned separately."""
import json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
styles={'BRK':'1910s brick commercial block: sepia brick, chunky cream stone sills, bottle green double-hung frames, oxblood doors, corbelled cornice','SPN':'Spanish Colonial Revival: warm ivory stucco, terracotta tile, arched openings, bottle green ironwork, ochre accents','DECO':'1930 Art Deco Zigzag Moderne: cream and buff terracotta, vertical piers, chevron spandrels, oxblood accents, dark marble shop bulkheads','MSN':'Mission Revival: sand stucco, exposed dark wooden rafter tails, small dark windows, scalloped parapets, terracotta accents'}
def family(f):
 rows=[]
 def add(s,w,h,desc,kind='module'):rows.append({'id':f+'_'+s,'family':f,'nominal':[w,h],'description':desc,'kind':kind})
 for w in [4,6,8]:
  for variant in 'AB':add(f'G_Bay{w}{variant}',w,4.5,'ground-floor shop window and door; '+('door right, broad window left' if variant=='A' else 'central door between two windows'))
 add('G_Entry4',4,4.5,'recessed upper-storey stair entrance, dark double doors, 1 metre recess')
 for w in [2,3,4]:
  for v in 'AB':add(f'U_Bay{w}{v}',w,3.5,'upper-floor '+('double-hung window' if v=='A' else 'paired narrow windows'))
 for tag,h in [('G',4.5),('U',3.5)]:
  add('Pilaster1_'+tag,1,h,'plain vertical pier and width filler')
  add('Corner_'+tag,1,h,'outside corner, 0.5 metre wrapping each face at 90 degrees')
 for w in [2,4]:add(f'Parapet{w}',w,1.5,'top parapet, lower 70 percent plain, coping upper 30 percent')
 for v in 'AB':add('Crest'+v,4,2,'ornamental parapet centrepiece, '+('simple curved gable' if v=='A' else 'stepped signboard with blank panel'))
 for w in [4,6]:add(f'Awning{w}',w,3.5,'separate canvas awning, bottom at 2.7 m, top at 3.5 m, projects 1.25 m, blank space below, no facade')
 add('Wall',4,3.5,'plain seamless wall panel','wall');add('Roof',4,4,'plain flat tar-and-gravel roof cap','roof')
 add('Damaged_G_Bay6',6,4.5,'ground storefront, boards over one window, soot above lintel, otherwise intact')
 add('Damaged_U_Bay3',3,3.5,'upper window bay with cracked plaster or brick and taped glass, otherwise intact')
 return rows
if __name__=='__main__':
 plan=[a for f in styles for a in family(f)]
 (R/'src/kit_plan.json').write_text(json.dumps(plan,indent=2)+'\n');print(len(plan),'modules planned')
