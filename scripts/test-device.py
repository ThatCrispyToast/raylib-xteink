#!/usr/bin/env python3
"""Exercise the real X4 diagnostic image; writes only its private temporary SD file."""
import argparse
import json
from pathlib import Path
import time

from device import Device


def require(value, message):
    if not value:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--output', type=Path, default=Path('artifacts/hardware/acceptance'))
    parser.add_argument('--steps', type=int, default=40)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    start = time.monotonic()
    with (args.output/'serial.log').open('w') as log:
        device = Device(args.port, log)
        try:
            def command(text, event='frame'):
                result = device.command(text, event)
                records.append(dict(command=text, **result))
                if 'heap_ok' in result:
                    require(result['heap_ok'], 'Heap corruption')
                    require(result['errors'] == 0, 'Display transfer error')
                    require(result['stack_free'] > 2048, 'Insufficient stack headroom')
                print(json.dumps(records[-1]), flush=True)
                return result

            initial = command('status', 'status')
            require(initial['sd'], 'Insert a FAT microSD card for acceptance testing')
            require(command('sdtest', 'sd_test')['ok'], 'SD round trip failed')
            device.event('status')
            require(command('assets', 'asset_test')['ok'], 'PNG asset round trip failed')
            device.event('status')
            device.frame(args.output/'sd-asset.png')
            require(command('oom', 'oom_test')['ok'], 'Texture allocation failure was not handled')
            require(command('texturetest', 'texture_test')['ok'], 'Texture update/readback failed')
            for scene in (4,5,0,2,3):
                first = command(f'scene {scene}')
                frame = device.frame(args.output/f'scene-{scene}.png')
                require(frame['hash'] == first['hash'], 'Frame changed during readback')
                same = command('redraw')
                require(same['hash'] == first['hash'], 'Nondeterministic rendering')
                require(same['skipped'] == first['skipped']+1, 'Unchanged frame refreshed')
                if scene in (4,5):
                    expected = 2166136261
                    for _ in range(48000): expected = ((expected ^ (255 if scene==5 else 0))*16777619)&0xffffffff
                    require(first['hash'] == expected, 'Black/white panel packing mismatch')
            before = command('full')
            require(before['full'] > initial['full'], 'Explicit full refresh missing')
            command('interval 4', 'status')
            before = command('scene 1')
            baseline = before['heap']
            for index in range(args.steps):
                after = command('step')
                require(after['hash'] != before['hash'], 'Moving bar did not change')
                require(after['heap'] == baseline, 'Heap changed during steady drawing')
                before = after
                if index % 10 == 9:
                    require(command('sdtest', 'sd_test')['ok'], 'Shared SPI SD round trip failed')
                    device.event('status')
            require(after['full'] >= args.steps//5, 'Periodic cleaning missing')
            command('interval 12', 'status')
            command('scene 0')
            for _ in range(3):
                reopened = command('reopen','ready')
                require(reopened['sd'], 'SD remount failed')
                require(reopened['heap'] >= baseline-512, 'Resource leak after reopen')
            final = command('status', 'status')
        finally:
            device.close()
    report = dict(result='PASS',elapsed_seconds=round(time.monotonic()-start,2),
                  initial=initial,final=final,records=records)
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {len(records)} physical-device checks; logs and captures in {args.output}')


if __name__ == '__main__':
    main()
