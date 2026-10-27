#!/usr/bin/env python3
"""Exercise the upstream emulator, read its actual SDL output, and test demo persistence."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

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
    # User journeys use the same SDL event path as physical-key emulation.
    import struct
    script = temp / "demo.input"
    env["X4_INPUT_SCRIPT"] = str(script)
    def run(keys, sd=None, idle_polls=0):
        if sd is not None:
            env["X4_SD_ROOT"] = str(sd)
        script.write_text("".join(f"{i*2} {key} down\n{i*2+1} {key} up\n" for i,key in enumerate(keys)) +
                          f"{len(keys)*2+2+idle_polls} quit down\n")
        result = subprocess.run([build / "x4_demo"], env=env, text=True, capture_output=True, timeout=30)
        assert result.returncode == 0 and "errors=0" in result.stdout, result.stdout + result.stderr
        return result
    def stats(result):
        values = re.search(r"X4: frames=(\d+) full=(\d+) partial=(\d+) unchanged=(\d+) errors=(\d+) buffers=(\d+)", result.stdout)
        assert values, result.stdout
        return dict(zip(("frames", "full", "partial", "unchanged", "errors", "buffers"), map(int, values.groups())))
    def saved(sd):
        records = [p.read_bytes() for p in (Path(sd)/"raylib-x4").glob("paper-*.sav")]
        data = max(records, key=lambda r: struct.unpack_from("<I", r, 8)[0])
        assert len(data) == 112 and data[:8] == b"X4PAPER2"
        return (*struct.unpack_from("<III", data, 12), data[24], data[25])
    sd = temp / "game"
    run(["Return", "Down", "Right", "Return", "P"], sd)
    board, initial, turns, cursor, undo = saved(sd)
    assert turns == 1 and cursor == 6 and undo == 1 and board != initial
    # Relaunch resumes directly into the puzzle; undo survives the restart.
    run(["Escape", "Down", "Return"])
    assert saved(sd) == (initial, initial, 0, 6, 0)
    # Reset cancellation preserves the existing state, then confirmation resets it.
    run(["Return", "Escape", "Down", "Down", "Return", "Escape", "Escape"])
    assert saved(sd)[2] == 1
    run(["Escape", "Down", "Down", "Return", "Return"])
    assert saved(sd) == (initial, initial, 0, 0, 0)
    # A new board is distinct, persists, and can be restarted from its own start.
    run(["Escape", "Down", "Down", "Down", "Return", "Return"])
    fresh = saved(sd)
    assert fresh[0] == fresh[1] and fresh[0] != initial and fresh[2:] == (0,0,0)
    run(["Return", "Escape", "Down", "Down", "Return", "Return"])
    assert saved(sd) == fresh
    # Solve the initial board with two-dimensional grid navigation.
    keys=["Return"]
    x=y=0
    for cell in [0,3,6,12,21]:
        tx,ty=cell%5,cell//5
        keys += ["Right"]*((tx-x)%5) + ["Down"]*((ty-y)%5) + ["Return"]
        x,y=tx,ty
    solve_sd=temp/"solve"
    run(keys, solve_sd)
    assert saved(solve_sd) == (0,initial,5,21,5)
    # Menu help and sleep work after solving; sleep saves the selected cell.
    run(["Escape"]+["Down"]*6+["Return","Return","Left","Escape","Up","Return"])
    assert saved(solve_sd)[3] == 20
    run(["Return","Return"], "")  # explicit no-card mode
    unavailable=temp/"not-a-directory"
    unavailable.write_text("untouched")
    run(["Return","Return"], unavailable)
    assert unavailable.read_text() == "untouched"
    collision=temp/"collision"
    collision.mkdir()
    (collision/"paper.txt").mkdir()
    run(["Return","Return"], collision)
    assert (collision/"paper.txt").is_dir() and saved(collision)[2] == 1
    foreign=temp/"foreign"
    foreign.mkdir()
    (foreign/"paper.txt").write_text("Personal notes.\n")
    run(["Return","Return"], foreign)
    assert (foreign/"paper.txt").read_text() == "Personal notes.\n"
    blocked=temp/"blocked"
    blocked.mkdir()
    (blocked/"raylib-x4").write_text("conflict")
    run(["Return","Return"], blocked)
    assert (blocked/"raylib-x4").read_text() == "conflict"
    # More than 100 real presentations and six seconds idle must not trigger a clean.
    navigation = stats(run(["Return"]+["Right"]*64+["Escape"]+["Down"]*64+["Escape"],
                           temp/"long-navigation", idle_polls=300))
    assert navigation["partial"] > 100 and navigation["full"] == 1, navigation
    navigation_idle = time.time() - (temp/"frame.ppm").stat().st_mtime
    assert navigation_idle >= 5, f"Long-navigation idle interval too short: {navigation_idle:.3f}s"
    # Power adds exactly one full update and preserves the visible game through idle.
    manual_sd = temp/"manual-clean"
    ordinary = stats(run(["Return"], manual_sd))
    ordinary_pixels = (temp/"frame.ppm").read_bytes()
    cleaned = stats(run(["Return","P"], manual_sd, idle_polls=300))
    assert ordinary["full"] == 1 and cleaned["full"] == 2, (ordinary, cleaned)
    assert cleaned["partial"] == ordinary["partial"], (ordinary, cleaned)
    assert (temp/"frame.ppm").read_bytes() == ordinary_pixels
    manual_idle = time.time() - (temp/"frame.ppm").stat().st_mtime
    assert manual_idle >= 5, f"Post-clean idle interval too short: {manual_idle:.3f}s"
    # Sleep uses a partial update; reopening initializes the panel with one full.
    sleep_sd = temp/"partial-sleep"
    slept = stats(run(["Return","Right","Return","Escape","Up","Return"], sleep_sd))
    before_wake = saved(sleep_sd)
    sleep_pixels = (temp/"frame.ppm").read_bytes()
    assert slept["full"] == 1 and slept["partial"] >= 1, slept
    assert before_wake[2:] == (1,1,1), before_wake
    woke = stats(run([], sleep_sd))
    assert woke["full"] == 1 and woke["partial"] == 0, woke
    assert saved(sleep_sd) == before_wake
    assert (temp/"frame.ppm").read_bytes() != sleep_pixels
    print(f"PASS: Paper manual-clean policy ({navigation['partial']} partial updates, {navigation_idle:.1f}s idle, one Power clean, {manual_idle:.1f}s idle, partial sleep, full initialization)")
print("PASS: SDL pixels, grid controls, solve, durable resume/undo, restart/cancel/new game, help/sleep, unavailable/conflicting storage")
