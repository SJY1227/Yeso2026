"""Download retained, individual Figma motion art; original source sheets only."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib,json,urllib.request
ROOT=Path(__file__).resolve().parents[1]
dest=ROOT/'assets/source/motion';dest.mkdir(parents=True,exist_ok=True)
def fetch(a):
    if 'url' not in a:return
    path=dest/a['file']
    if not path.is_file() or not path.stat().st_size:
        request=urllib.request.Request(a['url'],headers={'User-Agent':'RoutineDevice-development'})
        with urllib.request.urlopen(request,timeout=60) as r:data=r.read()
        if not data:raise ValueError('Empty Figma asset: '+a['file'])
        path.write_bytes(data)
    data=path.read_bytes();a.update(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
for name in ('motion','letter','bite'):
    path=ROOT/f'design/{name}-assets.json';m=json.loads(path.read_text(encoding='utf-8'))
    with ThreadPoolExecutor(max_workers=5) as pool:list(pool.map(fetch,m['assets']))
    path.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(name,sum('url' in a for a in m['assets']))
