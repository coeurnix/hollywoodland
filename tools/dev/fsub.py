"""Indentation-insensitive replace: fsub(path, old, new). `old` and `new` are written with any indentation; the
match ignores leading whitespace per line and `new` is re-indented to the matched block's first-line indent."""
import re, textwrap


def fsub(p, a, b):
    s = open(p, encoding='utf-8').read()
    a_lines = textwrap.dedent(a).strip('\n').split('\n')
    pat = '\n'.join(r'([ \t]*)' + re.escape(l.strip()) if i == 0 else r'[ \t]*' + re.escape(l.strip()) for i, l in enumerate(a_lines))
    m = list(re.finditer(pat, s))
    assert len(m) == 1, (p, a_lines[0][:60], len(m))
    ind = m[0].group(1)
    b_lines = textwrap.dedent(b).strip('\n').split('\n')
    nb = '\n'.join((ind + l) if l.strip() else l for l in b_lines)
    s = s[:m[0].start()] + nb + s[m[0].end():]
    open(p, 'w', encoding='utf-8').write(s)
