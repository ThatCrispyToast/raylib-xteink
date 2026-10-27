#!/usr/bin/env python3
"""Exercise the upstream emulator, read its actual SDL output, and test demo persistence."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "build"
with tempfile.TemporaryDirectory(prefix="raylib-x4-") as directory:
    temp = Path(directory)
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", X4_EMULATOR_FAST="1",
               X4_SD_ROOT=str(temp / "sd"), X4_SCREENSHOT=str(temp / "frame.ppm"),
               X4_INPUT_SCRIPT=str(root / "tests/emulator.input"))
    subprocess.run([build / "x4_emulator_tests"], env=env, check=True)
    with (temp / "frame.ppm").open("rb") as f:
        assert f.readline() == b"P6\n"
        width, height = map(int, f.readline().split())
        assert (width, height) == (480, 786), (width, height)
        assert f.readline() == b"255\n"
        pixels = f.read()
    def pixel(x, y):
        # microreader crops ten panel columns at the top in portrait.
        i = ((y - 10) * width + x) * 3
        return tuple(pixels[i:i + 3])
    assert pixel(25, 35) == (24, 26, 28)
    assert pixel(15, 35) == (232, 220, 200)
    assert pixel(410, 710) == (24, 26, 28)
    # Navigate to the puzzle, move the cursor, toggle a cell, clean, and quit.
    script = temp / "demo.input"
    script.write_text("0 Right down\n1 Right up\n2 Down down\n3 Down up\n"
                      "4 Return down\n5 Return up\n6 P down\n7 P up\n8 quit down\n")
    env["X4_INPUT_SCRIPT"] = str(script)
    result = subprocess.run([build / "x4_demo"], env=env, text=True, capture_output=True, check=True)
    assert "errors=0" in result.stdout, result.stdout
    save = temp / "sd/paper.txt"
    board, turns = map(int, save.read_text().split())
    assert turns == 1 and board != 0
    before = save.read_bytes()
    script.write_text("0 Right down\n1 Right up\n2 quit down\n")
    subprocess.run([build / "x4_demo"], env=env, check=True, stdout=subprocess.DEVNULL)
    assert save.read_bytes() == before
    # Solve the initial board through real key events, using its generating moves.
    actions = ["Right"]
    cursor = 0
    for target in [0, 3, 6, 12, 21]:
        actions += ["Down"] * (target - cursor) + ["Return"]
        cursor = target
    events = "".join(f"{i*2} {key} down\n{i*2+1} {key} up\n" for i,key in enumerate(actions))
    script.write_text(events + f"{len(actions)*2} quit down\n")
    env["X4_SD_ROOT"] = str(temp / "solve-sd")
    subprocess.run([build / "x4_demo"], env=env, check=True, stdout=subprocess.DEVNULL)
    assert (temp / "solve-sd/paper.txt").read_text().split() == ["0", "5"]
print("PASS: SDL pixel orientation/polarity/crop, navigation, SD save/reload, complete puzzle solution")
