#!/usr/bin/env python3
"""Convert a standard XYZ PNG tile tree (z/x/y.png) to ESP32 RGB565 tiles."""
from pathlib import Path
import sys

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Install Pillow first: python -m pip install Pillow") from exc


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: python tiles_to_rgb565.py <xyz_png_folder> <output_map_folder>")
        print("Input example: source/16/10482/25333.png")
        return 2
    source, output = map(Path, sys.argv[1:])
    tiles = sorted(source.glob("*/*/*.png"))
    if not tiles:
        print(f"No XYZ PNG tiles found under {source}")
        return 1
    converted = 0
    for path in tiles:
        try:
            zoom, x, filename = path.relative_to(source).parts
            y = Path(filename).stem
            image = Image.open(path).convert("RGB")
            if image.size != (256, 256):
                print(f"Skip {path}: expected 256x256, got {image.size}")
                continue
            out = output / zoom / x / f"{y}.rgb565"
            out.parent.mkdir(parents=True, exist_ok=True)
            data = bytearray()
            for r, g, b in image.getdata():
                pixel = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                data.extend((pixel & 0xFF, pixel >> 8))  # little-endian
            with out.open("wb") as f:
                f.write(data)
            converted += 1
        except (OSError, ValueError) as exc:
            print(f"Skip {path}: {exc}")
    print(f"Converted {converted} tiles to {output}")
    print("Copy the output folder contents into the SD card's /maps directory.")
    return 0 if converted else 1


if __name__ == "__main__":
    raise SystemExit(main())
