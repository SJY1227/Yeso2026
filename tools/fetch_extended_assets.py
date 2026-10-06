"""Retain individual Figma source assets and their hashes. No frame screenshots."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib, json, urllib.request

ROOT = Path(__file__).resolve().parents[1]
manifest_path = ROOT / 'design/extended-assets.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
dest = ROOT / 'assets/source/extended'
dest.mkdir(parents=True, exist_ok=True)

def fetch(asset):
    path = dest / asset['file']
    if path.exists() and path.stat().st_size:
        data = path.read_bytes()
    else:
        request=urllib.request.Request(asset['url'], headers={'User-Agent':'RoutineDevice-development'})
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read()
        if not data:
            raise ValueError(f'Empty response: {asset["file"]}')
        path.write_bytes(data)
    asset.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())

with ThreadPoolExecutor(max_workers=5) as workers:
    list(workers.map(fetch, [a for f in manifest['frames'] for a in f['assets']]))
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Retained', sum(len(f['assets']) for f in manifest['frames']), 'individual source assets')
