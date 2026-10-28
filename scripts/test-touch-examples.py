#!/usr/bin/env python3
"""Play both Pro examples and adjust the frontlight using only touch events."""
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='xteink-touch-') as directory:
    temp = Path(directory)
    script, frame = temp/'input', temp/'frame.ppm'
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', X4_EMULATOR_FAST='1',
               X4_SD_ROOT=str(temp/'sd'), X4_INPUT_SCRIPT=str(script), X4_SCREENSHOT=str(frame))
    def run(app, taps):
        script.write_text(''.join(f'{4*i+2} touch:{x}:{y} down\n{4*i+3} touch:{x}:{y} up\n'
                                  for i, (x, y) in enumerate(taps)) + f'{4*len(taps)+4} quit down\n')
        result = subprocess.run([build/app], env=env, text=True, capture_output=True, timeout=20)
        assert result.returncode == 0 and 'errors=0' in result.stdout, result.stdout+result.stderr
        return result.stdout, frame.read_bytes()
    # Showcase -> status -> cool light -> warm light, without an e-paper refresh.
    cool_log, cool = run('x4_demo', [(360, 740), (240, 628)])
    warm_log, warm = run('x4_demo', [(360, 740), (380, 628)])
    assert re.search(r'frames=(\d+)', cool_log).group(1) == re.search(r'frames=(\d+)', warm_log).group(1)
    def paper_pixel(ppm):
        pixels = ppm.split(b'\n', 3)[3]
        return tuple(pixels[:3])
    assert paper_pixel(cool) == (245, 241, 233), paper_pixel(cool)
    assert paper_pixel(warm) == (245, 223, 170), paper_pixel(warm)
    # Enter puzzle, tap cell 6, use menu to undo, then tap cell 12.
    run('x4_demo', [(80, 740), (130, 340), (360, 780), (100, 210), (210, 415)])
    records = [p.read_bytes() for p in (temp/'sd/raylib-x4').glob('paper-*.sav')]
    state = max(records, key=lambda data: struct.unpack_from('<I', data, 8)[0])
    assert struct.unpack_from('<I', state, 20)[0] == 1 and state[24] == 12 and state[25] == 1
    # Start practice, move, rotate, soft/hard drop, pause and resume.
    log, _ = run('x4_tetris', [(100, 320), (100, 520), (40, 720), (240, 720),
                              (330, 720), (440, 720), (360, 780), (100, 170)])
    assert 'screen=1' in log and 'practice=1' in log, log
    assert int(re.search(r'pieces=(\d+)', log).group(1)) == 1, log
print('PASS: Pro touch-only Paper/Tetris journeys and frontlight color readback')
