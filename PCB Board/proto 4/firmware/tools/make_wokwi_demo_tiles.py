"""Create four fictional RGB565 XYZ tiles used only by the Wokwi demo."""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[1] / "sdcard" / "maps"
ZOOM = 16
LAT, LON = 37.4220, -122.0840
N = 1 << ZOOM
center_x = (LON + 180.0) / 360.0 * N * 256
lat_rad = math.radians(LAT)
center_y = (1 - math.asinh(math.tan(lat_rad)) / math.pi) / 2 * N * 256
base_x, base_y = int(center_x // 256), int(center_y // 256)


def rgb565(r, g, b):
    p = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return bytes((p & 255, p >> 8))


background = rgb565(218, 228, 215)
park = rgb565(194, 220, 194)
water = rgb565(177, 211, 224)
road_edge = rgb565(210, 197, 165)
road = rgb565(255, 253, 242)

for ty in (base_y, base_y + 1):
    for tx in (base_x, base_x + 1):
        data = bytearray()
        for py in range(256):
            gy = ty * 256 + py
            for px in range(256):
                gx = tx * 256 + px
                color = background
                if -110 < gx-center_x < -12 and -170 < gy-center_y < -85:
                    color = park
                river_y = center_y - 45 + 22 * math.sin((gx-center_x)/58)
                road_y = center_y + 32 + 19 * math.sin((gx-center_x)/77)
                road_x = center_x + 37 + 17 * math.sin((gy-center_y)/63)
                if abs(gy-river_y) < 7:
                    color = water
                if abs(gy-road_y) < 9 or abs(gx-road_x) < 9:
                    color = road_edge
                if abs(gy-road_y) < 6 or abs(gx-road_x) < 6:
                    color = road
                data.extend(color)
        path = ROOT / str(ZOOM) / str(tx) / f"{ty}.rgb565"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(path.relative_to(ROOT.parent.parent))
