#!/usr/bin/env python3
"""Boot the ESP32-C3 smoke image and require its explicit success marker."""
from pathlib import Path
import selectors
import re
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "build-qemu"
flash = bytearray(b"\xff") * (16 * 1024 * 1024)
for offset, name in [(0, "bootloader/bootloader.bin"), (0x8000, "partition_table/partition-table.bin"),
                     (0x10000, "raylib_x4_firmware.bin")]:
    data = (build / name).read_bytes()
    flash[offset:offset + len(data)] = data
image = build / "qemu-flash.bin"
image.write_bytes(flash)
# Espressif's C3 model requires instruction counting for reliable timer delivery.
command = ["qemu-system-riscv32", "-nographic", "-icount", "3", "-machine", "esp32c3", "-drive",
           f"file={image},if=mtd,format=raw", "-no-reboot"]
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
    raise SystemExit("ESP32-C3 smoke test did not pass; inspect build-qemu/qemu.log")
