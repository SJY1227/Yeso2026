"""One reproducible image build: descriptors in firmware, versioned pack on FAT.

Run all source preparation scripts before this command when source art changes.
The payload CRC and length are compiled into firmware. Deploy with install-assets.ps1.
"""
import json, struct, zlib, hashlib
import image_lzss
from generate_assets import ROOT, OUT, main as home
from generate_product_assets import main as product
from generate_catalog_assets import main as catalog
from generate_theme_assets import main as themes
from generate_motion_assets import main as motions

image_lzss.archive=bytearray()
home(); product(); catalog(); themes(); motions()
payload=bytes(image_lzss.archive)
crc=zlib.crc32(payload)
pack=b'RDAS0001'+struct.pack('<II',len(payload),crc)+payload
dest=ROOT/'assets/packed';dest.mkdir(parents=True,exist_ok=True)
(dest/'ui.pak').write_bytes(pack)
manifest=dict(schema=1,payloadBytes=len(payload),bytes=len(pack),crc32=f'{crc:08x}',sha256=hashlib.sha256(pack).hexdigest(),entries=image_lzss.archive_entries)
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(OUT/'AssetPack.h').write_text(f'''#pragma once
#include <cstddef>
#include <cstdint>
namespace assets {{
inline constexpr size_t kAssetPayloadBytes = {len(payload)};
inline constexpr size_t kLargestImage = {max(e['size'] for e in image_lzss.archive_entries)};
inline constexpr uint32_t kAssetCrc = 0x{crc:08x};
inline constexpr char kAssetPath[] = "/ffat/ui-{crc:08x}.pak";
}}
''')
print(f'Asset pack: {len(pack):,} bytes, {len(manifest["entries"])} images, CRC {crc:08x}')
