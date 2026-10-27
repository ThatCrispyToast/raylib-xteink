#!/usr/bin/env python3
"""Check real deep-sleep entry, USB removal, and timer wake on the X4 diagnostic app."""
import argparse
import json
from pathlib import Path
import time

from device import Device

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',default='/dev/ttyACM0')
parser.add_argument('--cycles',type=int,default=3)
parser.add_argument('--log',type=Path,default=Path('artifacts/hardware/sleep.log'))
args=parser.parse_args()
with args.log.open('w') as log:
    device=Device(args.port,log)
    try:
        for cycle in range(args.cycles):
            device.command('sleep3','sleep')
            device.close()
            deadline=time.monotonic()+3
            while Path(args.port).exists() and time.monotonic()<deadline: time.sleep(.05)
            if Path(args.port).exists(): raise AssertionError('USB did not disappear on deep sleep')
            deadline=time.monotonic()+12
            while not Path(args.port).exists() and time.monotonic()<deadline: time.sleep(.05)
            if not Path(args.port).exists(): raise AssertionError('USB did not return after timer wake')
            time.sleep(.1)  # Let udev apply the serial permissions.
            device=Device(args.port,log)
            status=device.command('status')
            if status['reset_reason'] != 8 or status['wake_reason'] != 4:
                raise AssertionError(f'Unexpected wake: {status}')
            if not status['sd'] or not status['heap_ok'] or status['errors']:
                raise AssertionError(status)
            print(json.dumps(dict(cycle=cycle+1,boot=device.boot,status=status)),flush=True)
    finally:
        device.close()
print(f'PASS: {args.cycles} deep-sleep / timer-wake cycles')
