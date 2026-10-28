#!/usr/bin/env python3
"""Boot a C3/S3 smoke image and require its explicit success marker."""
from pathlib import Path
import json
import selectors
import re
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "build-qemu"
flash = bytearray(b"\xff") * (16 * 1024 * 1024)
config = json.loads((build / "flasher_args.json").read_text())
chip = config["extra_esptool_args"]["chip"]
if chip not in ("esp32c3", "esp32s3"):
    raise SystemExit(f"Unsupported QEMU chip: {chip}")
for address, name in config["flash_files"].items():
    offset = int(address, 0)
    data = (build / name).read_bytes()
    if offset < 0 or offset+len(data) > len(flash):
        raise SystemExit("Flash section exceeds the emulated 16 MB flash")
    flash[offset:offset + len(data)] = data
image = build / "qemu-flash.bin"
image.write_bytes(flash)
# C3 needs instruction counting; sleep=off advances idle time to the next timer deadline.
command = ["qemu-system-riscv32" if chip == "esp32c3" else "qemu-system-xtensa", "-nographic"]
if chip == "esp32c3":
    command += ["-icount", "shift=3,sleep=off"]
command += ["-machine", chip, "-drive", f"file={image},if=mtd,format=raw", "-no-reboot"]
process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
selector = selectors.DefaultSelector()
selector.register(process.stdout, selectors.EVENT_READ)
output = bytearray()
deadline = time.monotonic() + 120
passed = False
failed = False
try:
    while time.monotonic() < deadline:
        for key, _ in selector.select(1):
            chunk = key.fileobj.read1(8192)
            output.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
        if not failed and (b"X4_QEMU_FAIL:" in output or b"Guru Meditation" in output):
            failed=True
            deadline=min(deadline,time.monotonic()+2) # Keep the complete panic trace.
        if not failed and re.search(rb"X4_QEMU_PASS:[^\r\n]+\r?\n", output):
            passed = True
            break
        if process.poll() is not None:
            break
finally:
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    selector.close()
    process.stdout.close()
    (build / "qemu.log").write_bytes(output)
if not passed:
    raise SystemExit(f"{chip} smoke test did not pass; inspect {build / 'qemu.log'}")
