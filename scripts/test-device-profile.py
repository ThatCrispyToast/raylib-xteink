#!/usr/bin/env python3
"""Read back all four visible corners through SDL for each selected profile."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from emulator_profile import PROFILES

build = Path(sys.argv[1]).resolve()
width, height, _ = PROFILES[sys.argv[2]]
with tempfile.TemporaryDirectory(prefix="xteink-profile-") as directory:
    frame = Path(directory) / "frame.ppm"
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", X4_EMULATOR_FAST="1",
               X4_SD_ROOT="", X4_SCREENSHOT=str(frame))
    env.pop("X4_INPUT_SCRIPT", None)
    subprocess.run([build / "xteink_profile_tests"], env=env, check=True, timeout=20)
    with frame.open("rb") as stream:
        assert stream.readline() == b"P6\n"
        assert tuple(map(int, stream.readline().split())) == (width, height)
        assert stream.readline() == b"255\n"
        pixels = stream.read()
    assert len(pixels) == width*height*3
    def pixel(x, y):
        offset = (y*width+x)*3
        return tuple(pixels[offset:offset+3])
    for x in (0, width-1):
        for y in (0, height-1):
            assert pixel(x, y) == (24, 26, 28), (x, y, pixel(x, y))
    assert pixel(width//2, height//2) == (232, 220, 200)
print(f"PASS: {sys.argv[2]} SDL readback {width}x{height}, all visible edges")
