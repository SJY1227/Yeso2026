"""Fetch pinned Arduino libraries and the original assets returned by Figma MCP."""
import base64
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = {
    "Adafruit_GFX_Library": "1.12.6",
    "Adafruit_ST7735_and_ST7789_Library": "1.11.0",
    "Adafruit_BusIO": "1.17.4",
}


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "RoutineDevice-development"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def main():
    lock = []
    lib_root = ROOT / ".tools" / "libraries"
    lib_root.mkdir(parents=True, exist_ok=True)
    for name, version in LIBRARIES.items():
        url = f"https://downloads.arduino.cc/libraries/github.com/adafruit/{name}-{version}.zip"
        archive = lib_root / f"{name}-{version}.zip"
        if not archive.exists():
            archive.write_bytes(fetch(url))
        data = archive.read_bytes()
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for member in z.infolist():
                target = (lib_root / member.filename).resolve()
                if not target.is_relative_to(lib_root.resolve()):
                    raise ValueError("Unsafe archive path")
            z.extractall(lib_root)
        lock.append({"name": name, "version": version, "url": url,
                     "sha256": hashlib.sha256(data).hexdigest()})
        print(f"Library: {name} {version}", flush=True)
    (ROOT / "tools" / "libraries.lock.json").write_text(
        json.dumps(lock, indent=2) + "\n", encoding="utf-8")

    manifest = json.loads((ROOT / "design" / "home-assets.json").read_text(encoding="utf-8"))
    for theme in manifest["themes"]:
        for asset in theme["assets"]:
            path = ROOT / "assets" / "source" / asset["file"]
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists():
                path.write_bytes(fetch(asset["url"]))
            data = path.read_bytes()
            if not data:
                raise ValueError(f"Empty asset: {path}")
            asset["sha256"] = hashlib.sha256(data).hexdigest()
            asset["bytes"] = len(data)
            print(f"Asset: {asset['file']} ({len(data)} bytes)", flush=True)
    (ROOT / "design" / "home-assets.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    references = ROOT / "tmp" / "home-reference-images.json"
    if references.exists():
        for theme, image in json.loads(references.read_text(encoding="utf-8")).items():
            target = ROOT / "design" / "references" / f"{theme}-home.png"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(base64.b64decode(image["data"]))
    font_target = ROOT / "assets" / "fonts" / "NotoSansKR-VF.ttf"
    font_target.parent.mkdir(parents=True, exist_ok=True)
    if not font_target.exists():
        font_source = Path("C:/Windows/Fonts/NotoSansKR-VF.ttf")
        if not font_source.is_file():
            raise FileNotFoundError("Install Noto Sans KR or provide assets/fonts/NotoSansKR-VF.ttf")
        font_target.write_bytes(font_source.read_bytes())
    license_path = font_target.parent / "OFL.txt"
    if not license_path.exists():
        license_path.write_bytes(fetch("https://raw.githubusercontent.com/google/fonts/main/ofl/notosanskr/OFL.txt"))


if __name__ == "__main__":
    main()
