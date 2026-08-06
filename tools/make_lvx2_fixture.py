#!/usr/bin/env python3
"""Generate tests/fixtures/mini.lvx2 (LVX2 Specifications v1.0 layout).

1 device (sn FIXTURESN0000001, lidar_id 1234, device_type 9), frame_duration
50 ms, 2 frames x 3 packages x 96 Cartesian-high points. Point k of global
package g: x = g*1000+k, y = -x, z = g, reflectivity = k%256, tag = 0.
"""
import os
import struct
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), "..", "tests", "fixtures", "mini.lvx2")

MAGIC = 0xAC0EA767
LIDAR_ID = 1234
FRAMES, PKGS_PER_FRAME, POINTS = 2, 3, 96


def package(global_idx: int, frame_idx: int) -> bytes:
    payload = b"".join(
        struct.pack("<iiiBB", global_idx * 1000 + k, -(global_idx * 1000 + k),
                    global_idx, k % 256, 0)
        for k in range(POINTS))
    hdr = struct.pack("<BIBB8sHBIB4s",
                      0,                      # version
                      LIDAR_ID,               # lidar id
                      0,                      # lidar type (reserved)
                      0,                      # timestamp type (no sync)
                      struct.pack("<Q", global_idx * 480_000),  # timestamp ns
                      global_idx,             # udp counter
                      1,                      # data type: Cartesian high
                      len(payload),           # payload length only
                      frame_idx,              # frame counter
                      b"\x00" * 4)            # reserved
    assert len(hdr) == 27
    return hdr + payload


public = b"livox_tech" + b"\x00" * 6 + bytes([2, 0, 0, 0]) + struct.pack("<I", MAGIC)
assert len(public) == 24
private = struct.pack("<IB", 50, 1)  # frame duration 50 ms, 1 device
device = struct.pack("<16s16sIBBB6f",
                     b"FIXTURESN0000001", b"\x00" * 16, LIDAR_ID,
                     0,      # lidar type (reserved)
                     9,      # device type: Mid-360
                     0,      # extrinsic enable
                     0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
assert len(device) == 63

frames = []
offset = len(public) + len(private) + len(device)
g = 0
for f in range(FRAMES):
    body = b"".join(package(g + i, f) for i in range(PKGS_PER_FRAME))
    g += PKGS_PER_FRAME
    header = struct.pack("<QQQ", offset, offset + 24 + len(body), f)
    frames.append(header + body)
    offset += 24 + len(body)

os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
with open(OUT, "wb") as fh:
    fh.write(public + private + device + b"".join(frames))
print(f"wrote {OUT} ({os.path.getsize(OUT)} bytes)")
