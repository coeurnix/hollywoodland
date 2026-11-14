"""Build MSN with both crest outlines traced from its generated concepts.

All shared builder rules remain active. This isolated family entry point prevents
the generic fallback stepped CrestB silhouette from replacing the Mission gable.
Run Blender --background --python art/buildings/src/deco_msn_build.py -- MSN,
then prepare_brk_masters.py -- MSN.
"""
import sys
from pathlib import Path
assert '--' in sys.argv and sys.argv[sys.argv.index('--')+1]=='MSN'
path=Path(__file__).with_name('build_brk_kit.py')
source=path.read_text()
old="if aid.endswith('A'):outline=crest_outline(aid,w,h)"
assert old in source
source=source.replace(old,"if aid.endswith('A') or FAM=='MSN':outline=crest_outline(aid,w,h)")
exec(compile(source,str(path),'exec'))
