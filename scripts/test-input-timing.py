#!/usr/bin/env python3
"""Test ordered user input while the real SDL emulator blocks on refresh.

Run inside the native development shell:
    python3 scripts/test-input-timing.py build --output /tmp/x4-input-timing

A separate pthread injects events by monotonic time, including while the app is
blocked. The normal microreader 400 ms delay stays enabled. Selected cases extend
one refresh to 1600 ms; this is a controlled pause, not a physical waveform model.
The test checks final state and actual SDL pixels, not ESP32 rendering speed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INITIAL = 0x73315D


def toggle(board, cell):
    cells = [cell]
    if cell % 5:
        cells.append(cell - 1)
    if cell % 5 < 4:
        cells.append(cell + 1)
    if cell >= 5:
        cells.append(cell - 5)
    if cell < 20:
        cells.append(cell + 5)
    for index in cells:
        board ^= 1 << index
    return board


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def seed_game(directory):
    """Start in a known resumable game with one move available to undo."""
    directory.mkdir(parents=True)
    record = bytearray(112)
    record[:8] = b"X4PAPER2"
    struct.pack_into("<IIII", record, 8, 1, toggle(INITIAL, 0), INITIAL, 1)
    record[25] = 1
    struct.pack_into("<I", record, 108, checksum(record[:108]))
    (directory / "paper-a.sav").write_bytes(record)


def read_game(directory):
    valid = []
    for path in directory.glob("paper-*.sav"):
        data = path.read_bytes()
        if (len(data) == 112 and data[:8] == b"X4PAPER2"
                and struct.unpack_from("<I", data, 108)[0] == checksum(data[:108])):
            valid.append(data)
    if not valid:
        raise AssertionError("No valid save record")
    data = max(valid, key=lambda value: struct.unpack_from("<I", value, 8)[0])
    return {"board": struct.unpack_from("<I", data, 12)[0],
            "turns": struct.unpack_from("<I", data, 20)[0],
            "cursor": data[24], "undo": data[25]}


def check_game(case, board, turns, cursor, undo):
    expected = dict(board=board, turns=turns, cursor=cursor, undo=undo)
    if case["state"] != expected:
        raise AssertionError(f"expected {expected}, got {case['state']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", nargs="?", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path, help="Preserve inputs, logs, PPM screenshots, and JSON results")
    args = parser.parse_args()
    binary = args.build.resolve() / "x4_demo"
    if not binary.is_file():
        parser.error(f"Build Paper first: {binary}")
    if not shutil.which("pkg-config") or not shutil.which("cc"):
        parser.error("Install a C compiler, pkg-config, and SDL2 development files")
    with tempfile.TemporaryDirectory(prefix="x4-timed-input-") as temporary:
        if args.output:
            args.output.resolve().mkdir(parents=True, exist_ok=True)
            work = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
        else:
            work = Path(temporary)
        helper = Path(temporary) / "timed-input.so"
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
        subprocess.run(["cc", "-std=c11", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra",
                        str(ROOT / "tests/timed_sdl_input.c"), "-o", str(helper),
                        *flags, "-ldl", "-lpthread"], check=True)
        results = []

        def run(name, taps, quit_ms, held=(), pause=False):
            path = work / name
            path.mkdir(exist_ok=True)
            sd = path / "sd"
            seed_game(sd / "raylib-x4")
            events = [(at, key, "down") for at, key in taps]
            events += [(at + 60, key, "up") for at, key in taps]
            for down, up, key in held:
                events += [(down, key, "down"), (up, key, "up")]
            events.append((quit_ms, "quit", "down"))
            events.sort()
            script = path / "input.txt"
            script.write_text("".join(f"{at} {key} {action}\n" for at, key, action in events))
            env = dict(os.environ, SDL_VIDEODRIVER="offscreen", LD_PRELOAD=str(helper),
                       X4_QA_TIMED_INPUT=str(script), X4_SD_ROOT=str(sd),
                       X4_SCREENSHOT=str(path / "frame.ppm"))
            for key in ("X4_EMULATOR_FAST", "X4_INPUT_SCRIPT", "X4_QA_PAUSE_FRAME", "X4_QA_PAUSE_MS"):
                env.pop(key, None)
            if pause:
                env.update(X4_QA_PAUSE_FRAME="1", X4_QA_PAUSE_MS="1600")
            result = subprocess.run([binary], env=env, capture_output=True, text=True,
                                    timeout=quit_ms/1000 + 10)
            (path / "stdout.log").write_text(result.stdout)
            (path / "timing.log").write_text(result.stderr)
            if result.returncode or "QA_ERROR" in result.stderr:
                raise AssertionError(f"{name} failed to run: {result.stdout}\n{result.stderr}")
            if "errors=0" not in result.stdout or not (path / "frame.ppm").is_file():
                raise AssertionError(f"{name}: missing clean exit or screenshot")
            if pause and "QA_PAUSE" not in result.stderr:
                raise AssertionError(f"{name}: refresh pause was not exercised")
            input_count = len(re.findall(r"^QA_INPUT ", result.stderr, re.MULTILINE))
            if input_count != len(events):
                raise AssertionError(f"{name}: only {input_count}/{len(events)} events injected")
            return {"state": read_game(sd / "raylib-x4"),
                    "pixels": (path / "frame.ppm").read_bytes(),
                    "frames": [float(t) for t in re.findall(r"^QA_FRAME ([\d.]+)", result.stderr, re.MULTILINE)]}

        def check(name, action):
            try:
                detail = action()
                results.append({"name": name, "passed": True, "detail": detail})
                print(f"PASS: {name}", flush=True)
            except (AssertionError, subprocess.TimeoutExpired) as error:
                results.append({"name": name, "passed": False, "detail": str(error)})
                print(f"FAIL: {name}: {error}", flush=True)

        original = toggle(INITIAL, 0)
        reference = run("confirmation-reference", [(100, "Escape"), (750, "Down"),
                        (1400, "Down"), (2050, "Return")], 2900)
        check_game(reference, original, 1, 0, 1)
        menu_reference = run("menu-reference", [(100, "Escape"), (750, "Down"),
                             (1400, "Down")], 2250)
        check_game(menu_reference, original, 1, 0, 1)

        def rapid_taps():
            case = run("rapid-taps", [(100, "Right"), (250, "Right"), (400, "Right"),
                                     (550, "Right"), (1300, "Return")], 2100)
            check_game(case, toggle(original, 4), 2, 4, 2)
            return case["state"]
        check("four rapid taps preserve all four moves", rapid_taps)

        def mixed_order():
            case = run("mixed-order", [(100, "Right"), (180, "Return"),
                                      (260, "Down"), (340, "Return")], 1600)
            check_game(case, toggle(toggle(original, 1), 6), 3, 6, 3)
            return case["state"]
        check("mixed movement and flips preserve order", mixed_order)

        def confirmation(name, extra):
            taps = [(100, "Escape"), (180, "Down"), (260, "Down"), (340, "Return")]
            case = run(name, taps + extra, 1700)
            check_game(case, original, 1, 0, 1)
            if case["pixels"] != reference["pixels"]:
                raise AssertionError("Expected the same visible Restart confirmation as the slow reference")
            return "Restart confirmation visible; existing game unchanged"
        check("rapid menu selection reaches intended action", lambda: confirmation("rapid-menu", []))
        check("buffered Enter cannot auto-confirm reset", lambda: confirmation(
            "buffered-confirm", [(420, "Return"), (650, "Return")]))

        def cancel_during_confirmation():
            case = run("cancel-during-confirmation", [(100, "Escape"), (180, "Down"),
                       (260, "Down"), (340, "Return"), (650, "Escape")], 1700)
            check_game(case, original, 1, 0, 1)
            if case["pixels"] != menu_reference["pixels"]:
                raise AssertionError("Back during confirmation presentation did not return to the menu")
            return "Cancel survives the confirmation's presentation pause"
        check("Back during confirmation presentation still cancels", cancel_during_confirmation)

        def fresh_confirm():
            case = run("fresh-confirm", [(100, "Escape"), (180, "Down"), (260, "Down"),
                       (340, "Return"), (420, "Return"), (650, "Return"), (1200, "Return")], 2100)
            check_game(case, INITIAL, 0, 0, 0)
            return "A fresh Enter after the dialog appears restarts successfully"
        check("fresh confirmation remains usable", fresh_confirm)

        def clean_order():
            case = run("clean-order", [(100, "P"), (250, "Escape"),
                       (400, "Down"), (550, "Return")], 2800, pause=True)
            check_game(case, INITIAL, 0, 0, 0)
            return "Back, Down, Enter queued during a 1.6-second clean execute Undo"
        check("full-clean pause preserves menu input order", clean_order)

        def released_hold():
            case = run("released-hold", [(100, "P"), (2200, "Return")], 3100,
                       held=[(250, 1000, "Down")], pause=True)
            check_game(case, toggle(original, 5), 2, 5, 2)
            return "Released hold produces its original press without stale repeats"
        check("release during clean cancels later repeat", released_hold)

        def held_navigation():
            case = run("held-navigation", [(2200, "Return")], 3100,
                       held=[(100, 1300, "Down")])
            movement_frames = [t for t in case["frames"] if t < 2.1]
            if len(movement_frames) < 2:
                raise AssertionError("Held navigation did not repeat")
            if any(t > 1.8 for t in movement_frames):
                raise AssertionError(f"Navigation continued more than one refresh after release: {movement_frames}")
            if case["state"]["turns"] != 2:
                raise AssertionError("Held navigation changed the board before the explicit flip")
            return {"navigation_frames_seconds": movement_frames, "state": case["state"]}
        check("held navigation repeats and settles after release", held_navigation)

        report = {"binary": str(binary), "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                  "scope": "Wall-clock SDL input and simulated refresh blocking; not physical rendering latency",
                  "results": results}
        (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        failures = sum(not case["passed"] for case in results)
        print(f"{len(results)-failures}/{len(results)} input timing checks passed")
        if args.output:
            print(f"Evidence: {work}")
        elif failures:
            print("Repeat with --output /tmp/x4-input-timing to preserve evidence.")
        return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
