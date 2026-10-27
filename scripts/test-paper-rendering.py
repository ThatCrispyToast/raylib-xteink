#!/usr/bin/env python3
"""Compare retained puzzle updates with full-page renders through the real emulator.

Back followed by Back forces a full puzzle redraw without changing game state.
Each comparison starts from identical independent saves. Refresh delays are
disabled: this tests pixel correctness, not panel ghosting or physical latency.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
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


def seed(directory, board, turns, cursor, undo, damaged=False):
    directory.mkdir(parents=True)
    data = bytearray(112)
    data[:8] = b"X4PAPER2"
    struct.pack_into("<IIII", data, 8, 1, board, INITIAL, turns)
    data[24] = cursor
    data[25] = len(undo)
    data[32:32 + len(undo)] = bytes(undo)
    checksum = 2166136261
    for value in data[:108]:
        checksum = ((checksum ^ value) * 16777619) & 0xFFFFFFFF
    struct.pack_into("<I", data, 108, checksum)
    (directory / "paper-a.sav").write_bytes(data)
    if damaged:
        (directory / "paper-b.sav").write_bytes(b"interrupted record")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", nargs="?", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path, help="Preserve screenshots, logs, and JSON results")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    original = args.build.resolve() / "x4_demo"
    if not original.is_file():
        parser.error(f"Build Paper first: {original}")
    cases = []

    def case(name, keys, **state):
        values = dict(board=toggle(INITIAL, 0), turns=1, cursor=0, undo=[0],
                      damaged=False, blocked_save=False, no_card=False)
        values.update(state)
        cases.append(dict(name=name, keys=keys, **values))

    for cell in range(25):
        case(f"flip-cell-{cell:02}", ["Return"], cursor=cell)
    for cell in (0, 6, 12, 24):
        case(f"solve-cell-{cell:02}", ["Return"], board=toggle(0, cell), cursor=cell)
    case("leave-solved", ["Return"], board=0, cursor=12, turns=5)
    case("navigate-solved", ["Right", "Down"], board=0, turns=5)
    case("flip-twice", ["Return", "Return"], cursor=12)
    case("overlapping-crosses", ["Right", "Return", "Down", "Return", "Left", "Return"])
    case("undo", ["Escape", "Down", "Return"])
    case("undo-notice-clear", ["Escape", "Down", "Return", "Return"])
    case("empty-undo-notice", ["Escape", "Down", "Return", "Right"], turns=0, undo=[], board=INITIAL)
    case("menu-round-trip", ["Right", "Return", "Escape", "Down", "Down", "Escape"])
    for turns in (9, 99, 999, 4294967294):
        case(f"counter-{turns}", ["Return"], turns=turns, cursor=6)
    case("undo-counter-shrink", ["Escape", "Down", "Return"], turns=10)
    case("move-limit", ["Return"], turns=4294967295)
    case("reach-move-limit", ["Return", "Return"], turns=4294967294)
    case("save-failure", ["Return"], blocked_save=True)
    case("recovered-status-clears", ["Return"], damaged=True)
    case("no-card", ["Return", "Return"], no_card=True)

    with tempfile.TemporaryDirectory(prefix="x4-paper-rendering-") as temporary:
        if args.output:
            args.output.resolve().mkdir(parents=True, exist_ok=True)
            work = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
        else:
            work = Path(temporary)
        binary = Path(temporary) / "x4_demo"
        shutil.copy2(original, binary)
        digest = hashlib.sha256(binary.read_bytes()).hexdigest()

        def render(item, full):
            path = work / item["name"] / ("full" if full else "retained")
            path.mkdir(parents=True)
            sd = path / "sd"
            save = sd / "raylib-x4"
            seed(save, item["board"], item["turns"], item["cursor"], item["undo"], item["damaged"])
            if item["blocked_save"]:
                (save / "paper-b.sav").mkdir()
            keys = item["keys"] + (["Escape", "Escape"] if full else [])
            script = path / "input.txt"
            script.write_text("".join(f"{i*2} {key} down\n{i*2+1} {key} up\n" for i, key in enumerate(keys)) +
                              f"{len(keys)*2+2} quit down\n")
            env = dict(os.environ, SDL_VIDEODRIVER="offscreen", X4_EMULATOR_FAST="1",
                       X4_INPUT_SCRIPT=str(script), X4_SD_ROOT="" if item["no_card"] else str(sd),
                       X4_SCREENSHOT=str(path / "frame.ppm"))
            env.pop("LD_PRELOAD", None)
            result = subprocess.run([binary], env=env, capture_output=True, text=True, timeout=30)
            (path / "console.log").write_text(result.stdout + result.stderr)
            if result.returncode or "errors=0" not in result.stdout:
                raise AssertionError(f"{item['name']}: emulator failed: {result.stdout}\n{result.stderr}")
            records = {p.name: p.read_bytes() if p.is_file() else None for p in save.glob("paper-*.sav")}
            return (path / "frame.ppm").read_bytes(), records

        def compare(item):
            try:
                retained, retained_save = render(item, False)
                full, full_save = render(item, True)
                if retained_save != full_save:
                    raise AssertionError("Full redraw changed the saved game")
                different = sum(a != b for a, b in zip(retained, full)) + abs(len(retained) - len(full))
                if different:
                    raise AssertionError(f"{different} screenshot bytes differ")
                return dict(name=item["name"], passed=True)
            except (AssertionError, subprocess.TimeoutExpired) as error:
                return dict(name=item["name"], passed=False, error=str(error))

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(compare, cases))
        report = dict(binary=str(original), sha256=digest, results=results)
        (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        for result in results:
            if not result["passed"]:
                print(f"FAIL: {result['name']}: {result['error']}")
        passed = sum(result["passed"] for result in results)
        print(f"{passed}/{len(results)} retained/full puzzle renders are pixel-identical and preserve saves")
        if args.output:
            print(f"Evidence: {work}")
        elif passed != len(results):
            print("Repeat with --output /tmp/x4-paper-rendering to preserve evidence.")
        return passed != len(results)


if __name__ == "__main__":
    raise SystemExit(main())
