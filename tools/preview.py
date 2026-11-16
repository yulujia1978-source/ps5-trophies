"""Produce an offline, explicitly labelled UI demo. No console data included."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
html=(root/'web/index.html').read_text()
css=(root/'web/style.css').read_text()
model=(root/'web/model.js').read_text().replace('export ','')
app=(root/'web/app.js').read_text().split('\n',1)[1]
app=app.replace("const demo=new URLSearchParams(location.search).get('demo')==='1';",'const demo=true;')
# Inline script intentionally has no external module imports, so file:// works.
html=html.replace('<link rel="stylesheet" href="/style.css">',f'<style>{css}</style>')
html=html.replace('<script type="module" src="/app.js"></script>',f'<script type="module">{model}\n{app}</script>')
(root/'releases/网页演示.html').write_text(html)
