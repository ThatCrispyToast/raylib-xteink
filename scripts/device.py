#!/usr/bin/env python3
"""USB client for examples/diagnostics.c. Run in the firmware development shell."""
import argparse
import json
from pathlib import Path
import struct
import termios
import time
import zlib

import serial


class Device:
    def __init__(self, port, log=None, synchronize=True):
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.2, exclusive=True)
        # Linux asserts both lines on open. Keep them together to avoid a USB reset pulse.
        self.serial.dtr = True
        self.serial.rts = True
        self.serial.port = port
        self.serial.open()
        attrs = termios.tcgetattr(self.serial.fileno())
        attrs[2] &= ~termios.HUPCL
        termios.tcsetattr(self.serial.fileno(),termios.TCSANOW,attrs)
        self.log = log
        self.pending = bytearray()
        self.boot = None
        # Drain boot events before matching replies to application commands.
        if synchronize:
            for attempt in range(10):
                try:
                    self.command('status',timeout=3)
                    break
                except TimeoutError:
                    if attempt == 9: raise
                except RuntimeError as error:
                    if error.args[0] != {'event':'error','message':'Unknown command'} or attempt == 9:
                        raise

    def close(self):
        self.serial.close()

    def line(self, deadline):
        while time.monotonic() < deadline:
            if b'\n' in self.pending:
                line, _, self.pending = self.pending.partition(b'\n')
                text = line.decode('utf-8', 'replace').rstrip('\r')
                if self.log:
                    self.log.write(text + '\n')
                    self.log.flush()
                return text
            self.pending.extend(self.serial.read(self.serial.in_waiting or 1))
        raise TimeoutError('Device response timed out')

    def send(self, command):
        self.serial.write((command + '\n').encode('ascii'))
        self.serial.flush()

    def event(self, name, timeout=30):
        deadline = time.monotonic() + timeout
        while True:
            line = self.line(deadline)
            if line.startswith('X4 {'):
                value = json.loads(line[3:])
                if value.get('event') == 'boot':
                    self.boot = value
                if value.get('event') == 'error':
                    raise RuntimeError(value)
                if value.get('event') == name:
                    return value
            elif 'panic' in line.lower() or 'abort()' in line:
                raise RuntimeError(line)

    def command(self, command, event='status', timeout=30):
        self.send(command)
        return self.event(event, timeout)

    def frame(self, output):
        self.send('frame')
        deadline = time.monotonic() + 30
        while True:
            header = self.line(deadline)
            if header.startswith('X4 FRAME '):
                break
        _, _, length, checksum = header.split()
        data = bytearray()
        while True:
            line = self.line(deadline)
            if line == 'X4 END':
                break
            data.extend(bytes.fromhex(line))
        digest = 2166136261
        for byte in data:
            digest = ((digest ^ byte) * 16777619) & 0xffffffff
        if len(data) != int(length) or digest != int(checksum):
            raise ValueError('Incomplete or corrupt frame readback')
        # Controller landscape memory -> portrait application coordinates.
        raw = bytearray()
        for y in range(800):
            raw.append(0)
            for x in range(480):
                pixel = (479-x)*800+y
                raw.append(255 if data[pixel//8] & (0x80 >> (pixel%8)) else 0)
        def chunk(kind, payload):
            return struct.pack('>I', len(payload))+kind+payload+struct.pack('>I', zlib.crc32(kind+payload))
        output = Path(output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB',480,800,8,0,0,0,0))+
                           chunk(b'IDAT', zlib.compress(raw))+chunk(b'IEND', b''))
        return {'bytes':len(data), 'hash':digest, 'output':str(output)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--log', type=Path)
    parser.add_argument('--listen', type=float, default=0, metavar='SECONDS')
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--raw', action='store_true', help='Monitor any firmware without the diagnostic handshake')
    parser.add_argument('commands', nargs='*')
    args = parser.parse_args()
    log = args.log.open('a') if args.log else None
    device = Device(args.port, log, synchronize=not args.raw)
    try:
        for command in (args.commands or ([] if args.raw else ['status'])):
            event = 'frame' if command in ('step','redraw','full') or command.startswith('scene ') else 'status'
            if command == 'sdtest': event = 'sd_test'
            if command == 'oom': event = 'oom_test'
            if command == 'assets': event = 'asset_test'
            if command == 'texturetest': event = 'texture_test'
            if command == 'reopen': event = 'ready'
            if command in ('sleep','sleep3'): event = 'sleep'
            if command == 'reboot': event = 'reboot'
            print(json.dumps(device.command(command,event)),flush=True)
        if args.capture:
            print(json.dumps(device.frame(args.capture)),flush=True)
        deadline = time.monotonic()+args.listen
        while time.monotonic() < deadline:
            try: print(device.line(deadline),flush=True)
            except TimeoutError: break
            except serial.SerialException:
                if not args.raw: raise
                device.close()
                print('USB disconnected; waiting for wake',flush=True)
                while time.monotonic() < deadline:
                    time.sleep(.1)
                    try:
                        device=Device(args.port,log,synchronize=False)
                        break
                    except (OSError,serial.SerialException): pass
    finally:
        device.close()
        if log: log.close()


if __name__ == '__main__':
    main()
