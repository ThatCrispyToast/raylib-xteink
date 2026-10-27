#!/usr/bin/env python3
"""Play Tetris through SDL and verify pace, held controls, pause, and cleaning."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", nargs="?", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="x4-tetris-") as temporary:
        work = Path(temporary)
        if args.output:
            args.output.mkdir(parents=True, exist_ok=True)
            work = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
        binary = Path(temporary) / "x4_tetris"
        shutil.copy2(args.build.resolve() / "x4_tetris", binary)
        cases = []

        def case(name, keys, expected, idle=8, extra=()):
            events = [(8 + i*8, key, "down") for i, key in enumerate(keys)]
            events += [(9 + i*8, key, "up") for i, key in enumerate(keys)]
            events += list(extra)
            events.append((8 + len(keys)*8 + idle, "quit", "down"))
            cases.append(dict(name=name, events=events, expected=expected, timed=False))

        case("title", [], dict(screen=0, pieces=0, full=1))
        case("title-sleep", ["Escape", "Return"], dict(screen=6, pieces=0, full=1))
        case("title-cancel-sleep", ["Escape", "Escape"], dict(screen=0, pieces=0, full=1))
        case("practice-waits", ["Down", "Return"], dict(screen=1, practice=1, y=0, pieces=0, full=1), idle=180)
        case("normal-falls", ["Return"], dict(screen=1, practice=0, y=lambda y: y>=1, pieces=0), idle=180)
        case("move-rotate-drop", ["Down", "Return", "Right", "Up", "Down", "Return"],
             dict(screen=1, pieces=1, score=lambda score: score>0, full=1))
        case("batched-double-drop", ["Down", "Return"], dict(screen=1, pieces=1), idle=24,
             extra=[(26,"Return","down"),(26,"Return","up"),(26,"Return","down"),(26,"Return","up")])
        case("pause-freezes", ["Return", "Escape"], dict(screen=2, y=0, pieces=0), idle=180)
        case("resume", ["Down", "Return", "Escape", "Escape"], dict(screen=1, practice=1, y=0))
        case("change-mode", ["Down", "Return", "Escape", "Down", "Down", "Return", "Escape"],
             dict(screen=1, practice=0, y=lambda y: y>=1), idle=180)
        case("cancel-restart", ["Down", "Return", "Return", "Escape", "Down", "Return", "Escape", "Escape"],
             dict(screen=1, pieces=1))
        case("confirm-restart", ["Down", "Return", "Return", "Escape", "Down", "Return", "Return"],
             dict(screen=1, pieces=0, score=0))
        case("help", ["Down", "Return", "Escape", "Down", "Down", "Down", "Return"], dict(screen=3))
        case("sleep", ["Down", "Return", "Escape", "Up", "Return", "Return"], dict(screen=6, full=1))
        case("cancel-sleep", ["Down", "Return", "Escape", "Up", "Return", "Escape"], dict(screen=2))
        case("manual-clean", ["Down", "Return"]+["Right", "Left"]*10+["P"],
             dict(screen=1, full=2, pieces=0, partial=lambda partial: partial>=20), idle=180)

        helper = Path(temporary) / "timed-input.so"
        if sys.platform.startswith("linux"):
            flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
            subprocess.run(["cc", "-shared", "-fPIC", "-O2", str(ROOT / "tests/timed_sdl_input.c"),
                            "-o", str(helper), *flags, "-ldl", "-lpthread"], check=True)

            def timed(name, taps, quit_ms, expected, pause=0):
                events = [(at,key,"down") for at,key in taps] + [(at+60,key,"up") for at,key in taps]
                events.append((quit_ms,"quit","down"))
                cases.append(dict(name=name,events=events,expected=expected,timed=True,pause=pause))

            timed("clean-freezes-clock", [(100,"Return"),(900,"P")], 3200,
                  dict(screen=1,y=0,pieces=0,full=2), pause=2000)
            timed("input-during-clean", [(100,"Return"),(900,"P"),(1200,"Right"),(1400,"Right"),(1600,"Down")],
                  3550, dict(screen=1,x=5,y=1,pieces=0,score=1,full=2), pause=2000)
            timed("unseen-piece-drop-guard", [(100,"Return"),(900,"Return"),(1100,"Return")],
                  1700, dict(screen=1,pieces=1))
            timed("buffered-confirmation-guard", [(100,"Return"),(900,"Escape"),(1400,"Down"),
                  (1900,"Return"),(2100,"Return")], 2700, dict(screen=4,pieces=0))
            timed("real-time-gravity", [(100,"Return")], 6000,
                  dict(screen=1,y=lambda y: y>=6,pieces=0,full=1))
            for name,key,release,quit_ms,expected in [
                    ("held-soft-drop", "Down", 2800, 3200, dict(screen=1,y=18,pieces=0,score=18,practice=1)),
                    ("held-movement-release", "Right", 2050, 2250, dict(screen=1,x=lambda x: x>=6,pieces=0,practice=1))]:
                timed(name,[(100,"Down"),(700,"Return")],quit_ms,expected)
                cases[-1]["events"] += [(1300,key,"down"),(release,key,"up")]

        def run(item):
            path = work / item["name"]
            path.mkdir()
            script = path / "input.txt"
            # Preserve insertion order for multiple edges with the same timestamp.
            events = sorted(item["events"], key=lambda event: event[0])
            script.write_text("".join(f"{at} {key} {action}\n" for at,key,action in events))
            env = dict(os.environ, SDL_VIDEODRIVER="offscreen", X4_SD_ROOT="", X4_SCREENSHOT=str(path / "frame.ppm"))
            for key in ("LD_PRELOAD", "X4_INPUT_SCRIPT", "X4_EMULATOR_FAST", "X4_QA_TIMED_INPUT", "X4_QA_PAUSE_FRAME", "X4_QA_PAUSE_MS"):
                env.pop(key, None)
            if item["timed"]:
                env.update(LD_PRELOAD=str(helper), X4_QA_TIMED_INPUT=str(script))
                if item["pause"]:
                    env.update(X4_QA_PAUSE_FRAME="2", X4_QA_PAUSE_MS=str(item["pause"]))
            else:
                env.update(X4_EMULATOR_FAST="1", X4_INPUT_SCRIPT=str(script))
            result = subprocess.run([binary], env=env, capture_output=True, text=True, timeout=30)
            (path / "console.log").write_text(result.stdout + result.stderr)
            try:
                assert result.returncode == 0, result.stderr
                line = re.search(r"TETRIS: screen=.*", result.stdout)
                assert line, result.stdout
                state = {key:int(value) for key,value in re.findall(r"(\w+)=(-?\d+)", line[0])}
                assert state["errors"] == 0, state
                assert (path / "frame.ppm").read_bytes().startswith(b"P6\n480 786\n255\n")
                if item["timed"]:
                    assert "QA_ERROR" not in result.stderr, result.stderr
                    assert len(re.findall(r"^QA_INPUT ",result.stderr,re.MULTILINE)) == len(events)
                    if item["pause"]: assert "QA_PAUSE" in result.stderr, "Long refresh was not exercised"
                for key,value in item["expected"].items():
                    assert value(state[key]) if callable(value) else state[key] == value, (key,state)
                return dict(name=item["name"], passed=True, state=state)
            except AssertionError as error:
                return dict(name=item["name"], passed=False, error=str(error))

        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(run, cases))
        for result in results:
            print(("PASS" if result["passed"] else "FAIL") + ": " + result["name"] +
                  (": " + result["error"] if not result["passed"] else ""))
        (work / "report.json").write_text(json.dumps(results,indent=2)+"\n")
        if args.output: print(f"Evidence: {work}")
        return not all(result["passed"] for result in results)


if __name__ == "__main__":
    raise SystemExit(main())
