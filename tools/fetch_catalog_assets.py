"""Fetch only the individual asset URLs returned in the retained Figma contexts."""
import base64
import hashlib
import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from fetch_sources import fetch

ROOT = Path(__file__).resolve().parents[1]


def main(collection='catalog'):
    prefix='theme' if collection=='themes' else 'catalog'
    manifest_path = ROOT / f'design/{prefix}-assets.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    asset_root = (ROOT / f'assets/source/{collection}').resolve()
    asset_root.mkdir(parents=True, exist_ok=True)
    assets = [a for f in manifest['frames'] for a in f['assets']]

    def retrieve(asset):
        path = (asset_root / asset['file']).resolve()
        if not path.is_relative_to(asset_root):
            raise ValueError(f'Asset path outside source directory: {path} / {asset_root}')
        if not asset['url'].startswith('https://www.figma.com/api/mcp/asset/'):
            raise ValueError('Expected a Figma asset URL')
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file():
            path.write_bytes(fetch(asset['url']))
        data = path.read_bytes()
        if not data:
            raise ValueError(f'Empty asset: {path}')
        digest = hashlib.sha256(data).hexdigest()
        if asset.get('sha256', digest) != digest:
            raise ValueError(f'Asset hash mismatch: {path}')
        asset.update(sha256=digest, bytes=len(data))

    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(retrieve, assets))
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    pattern='theme-screenshots*.json' if collection=='themes' else 'figma-catalog-screenshots*.json'
    for screenshots in (ROOT / '.local').glob(pattern):
        destination = ROOT / f'design/references/{collection}'
        destination.mkdir(parents=True, exist_ok=True)
        for entry in json.loads(screenshots.read_text(encoding='utf-8')):
            (destination / (entry['id'].replace(':', '-')+'.png')).write_bytes(base64.b64decode(entry['data']))
    print(f'Fetched and hashed {len(assets)} assets from {len(manifest["frames"])} frames')


if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--collection',choices=['catalog','themes'],default='catalog')
    main(parser.parse_args().collection)
