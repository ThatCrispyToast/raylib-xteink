#!/usr/bin/env python3
"""Capture a scripted demo run from the emulator's SDL output to a PNG."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1] if len(sys.argv) > 1 else root / "artifacts/demo.png").resolve()
page = int(sys.argv[2]) if len(sys.argv) > 2 else 0
with tempfile.TemporaryDirectory(prefix="x4-capture-") as directory:
    temp = Path(directory)
    script = "".join(f"{i*2} Right down\n{i*2+1} Right up\n" for i in range(page))
    (temp / "input").write_text(script + f"{page*2+2} quit down\n")
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", X4_EMULATOR_FAST="1",
               X4_INPUT_SCRIPT=str(temp / "input"), X4_SD_ROOT=str(temp / "sd"),
               X4_SCREENSHOT=str(temp / "frame.ppm"))
    subprocess.run([root / "build/x4_demo"], env=env, check=True, stdout=subprocess.DEVNULL)
    with (temp / "frame.ppm").open("rb") as f:
        assert f.readline() == b"P6\n"
        width, height = map(int, f.readline().split())
        assert f.readline() == b"255\n"
        pixels = f.read()
def chunk(kind, data):
    return struct.pack(">I",len(data)) + kind + data + struct.pack(">I",zlib.crc32(kind + data))
raw = b"".join(b"\0" + pixels[y*width*3:(y+1)*width*3] for y in range(height))
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,2,0,0,0)) +
                  chunk(b"IDAT",zlib.compress(raw)) + chunk(b"IEND",b""))
print(output)
