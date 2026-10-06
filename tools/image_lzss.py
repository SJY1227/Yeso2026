"""Lossless 4KiB-window image byte stream; no runtime allocation or library dependency.

Groups contain a flags byte followed by up to 8 tokens (low bit first).
0 = literal byte. 1 = LE16 match: 12 bits distance-1, 4 bits length-3.
Length 18 has an extra byte (0..255), allowing a 273-byte match.
"""
from collections import defaultdict, deque

# Set only by generate_asset_pack.py, which regenerates all descriptors together.
archive = None
archive_entries = []


def image_bytes(image):
    """Crop without changing RGB565 or 8-bit coverage at any visible pixel."""
    import numpy as np
    image = image.convert('RGBA')
    box = image.getbbox()
    if box is None:
        raise ValueError('Empty asset')
    rgba = np.asarray(image.crop(box), dtype=np.uint16)
    rgb = ((rgba[:, :, 0] >> 3) << 11) | ((rgba[:, :, 1] >> 2) << 5) | (rgba[:, :, 2] >> 3)
    rgb[rgba[:, :, 3] == 0] = 0
    raw = np.stack((rgb & 255, rgb >> 8, rgba[:, :, 3]), axis=2).astype('uint8').tobytes()
    return box, raw


def image_cpp(name, image, numbers):
    box, raw = image_bytes(image)
    packed = compress(raw)
    assert decompress(packed, len(raw)) == raw
    x, y, right, bottom = box
    if archive is not None:
        offset = len(archive)
        archive.extend(packed)
        archive_entries.append(dict(name=name, offset=offset, size=len(packed), box=box))
        return f'const CompressedImage {name} = {{nullptr, {len(packed)}, {x}, {y}, {right-x}, {bottom-y}, {offset}}};\n', len(packed)
    cpp = f'const uint8_t {name}Data[] = {{\n{numbers(packed)}\n}};\n'
    cpp += f'const CompressedImage {name} = {{{name}Data, sizeof({name}Data), {x}, {y}, {right-x}, {bottom-y}}};\n'
    return cpp, len(packed)


def compress(data):
    positions = defaultdict(lambda: deque(maxlen=32))
    output = bytearray()
    at = 0
    while at < len(data):
        flag_at = len(output)
        output.append(0)
        for bit in range(8):
            if at == len(data):
                break
            key = data[at:at+3]
            length, distance = 0, 0
            if len(key) == 3:
                for prior in reversed(positions[key]):
                    if at-prior > 4096:
                        break
                    n = 3
                    while n < min(273, len(data)-at) and data[prior+n] == data[at+n]:
                        n += 1
                    if n > length:
                        length, distance = n, at-prior
                    if n == 273:
                        break
            used = length if length >= 3 else 1
            if length >= 3:
                output[flag_at] |= 1 << bit
                token = distance-1 | (min(length,18)-3) << 12
                output.extend((token & 255, token >> 8))
                if length >= 18:
                    output.append(length-18)
            else:
                output.append(data[at])
            for p in range(at, at+used):
                if p+3 <= len(data):
                    positions[data[p:p+3]].append(p)
            at += used
    return bytes(output)


def decompress(data, size):
    out, at = bytearray(), 0
    while len(out) < size:
        flags = data[at]
        at += 1
        for bit in range(8):
            if len(out) == size:
                break
            if flags & (1 << bit):
                token = data[at] | data[at+1] << 8
                at += 2
                distance, count = (token & 4095)+1, (token >> 12)+3
                if count == 18:
                    count += data[at]
                    at += 1
                if distance > len(out) or count > size-len(out):
                    raise ValueError('Invalid match')
                for _ in range(count):
                    out.append(out[-distance])
            else:
                out.append(data[at])
                at += 1
    if at != len(data):
        raise ValueError('Trailing data')
    return bytes(out)
