#!/usr/bin/env python3
"""Draw the NetSpaceWar icon (the game's ship inside a full shield) and write
netspacewar.ico next to this script. Uses only the standard library.

    python res/make_icon.py

The ship is the outline gui_draw_ship() draws: nose at (1,0), wing tips at
(-.5,+-.866), with lines from the centre to the nose and both wing tips.
"""

import math
import os
import struct
import zlib

SIZES = [256, 64, 48, 32, 24, 16]

BACKGROUND = (8, 12, 30)
STAR = (255, 255, 255)
SHIP = (200, 200, 210)
SHIELD = (0, 255, 255)


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def coverage(dist, half_width):
    """Antialiased coverage of a pixel whose centre is dist from a stroke's centre line."""
    return max(0.0, min(1.0, half_width - dist + 0.5))


def rounded_rect_dist(px, py, n, radius):
    """Signed distance from (px,py) to an n x n square with rounded corners (negative inside)."""
    c = n / 2
    qx = abs(px - c) - (c - radius)
    qy = abs(py - c) - (c - radius)
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    return outside + min(max(qx, qy), 0.0) - radius


def render(n):
    c = n / 2
    shield_r = n * 0.40
    ship_r = shield_r * 0.72
    # The ship's outline spans -1..+.5 along its axis; shift it so it sits centred in the shield.
    ship_shift = ship_r * 0.25
    stroke = max(1.0, n / 26)          # ship line width in pixels
    shield_w = max(1.0, n / 22)

    def ship_point(u, v):
        # Ship coordinates (u along the nose, v sideways) to pixels, nose pointing up.
        return c + v * ship_r, c - u * ship_r + ship_shift

    nose = ship_point(1, 0)
    left = ship_point(-0.5, -0.866)
    right = ship_point(-0.5, 0.866)
    centre = ship_point(0, 0)
    ship_lines = [(nose, left), (nose, right), (centre, left), (centre, right), (centre, nose)]
    windows = [ship_point(0.3, -0.15), ship_point(0.3, 0.15)]

    # A few fixed stars outside the shield, only where there is room to see them.
    stars = []
    if n >= 48:
        for fx, fy, size in [(0.14, 0.18, 1.0), (0.86, 0.13, 0.8), (0.90, 0.80, 1.0),
                             (0.12, 0.84, 0.7), (0.78, 0.92, 0.6), (0.20, 0.55, 0.6)]:
            stars.append((fx * n, fy * n, max(0.6, size * n / 128)))

    corner = n * 0.18
    rows = []
    for y in range(n):
        row = bytearray()
        for x in range(n):
            px, py = x + 0.5, y + 0.5
            # Tile background, with an antialiased rounded edge
            a = max(0.0, min(1.0, 0.5 - rounded_rect_dist(px, py, n, corner)))
            r, g, b = BACKGROUND

            def blend(colour, alpha):
                nonlocal r, g, b
                r = r + (colour[0] - r) * alpha
                g = g + (colour[1] - g) * alpha
                b = b + (colour[2] - b) * alpha

            for sx, sy, sr in stars:
                blend(STAR, coverage(math.hypot(px - sx, py - sy), sr) * 0.9)

            ring = abs(math.hypot(px - c, py - c) - shield_r)
            if n >= 48:
                # Soft glow around the shield
                blend(SHIELD, 0.35 * math.exp(-(ring / (shield_w * 1.5)) ** 2))
            blend(SHIELD, coverage(ring, shield_w / 2))

            d = min(seg_dist(px, py, *p, *q) for p, q in ship_lines)
            blend(SHIP, coverage(d, stroke / 2))
            if n >= 48:
                for wx, wy in windows:
                    blend(SHIP, coverage(math.hypot(px - wx, py - wy), stroke * 0.6))

            row += bytes((round(r), round(g), round(b), round(a * 255)))
        rows.append(bytes(row))
    return rows


def png(n, rows):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
    raw = b"".join(b"\x00" + row for row in rows)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def ico(images):
    """An .ico holding PNG images (supported since Windows Vista)."""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries, data = b"", b""
    for n, blob in images:
        entries += struct.pack("<BBBBHHII", n % 256, n % 256, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    images = [(n, png(n, render(n))) for n in SIZES]
    with open(os.path.join(here, "netspacewar.ico"), "wb") as f:
        f.write(ico(images))
    with open(os.path.join(here, "netspacewar.png"), "wb") as f:
        f.write(images[0][1])


if __name__ == "__main__":
    main()
