#!/usr/bin/env python3
"""A stale successful hardware report must not attest a different binary."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="x4-package-test-") as temporary:
    test=Path(temporary)
    (test/"scripts").mkdir()
    shutil.copy2(root/"scripts/package.py",test/"scripts/package.py")
    shutil.copy2(root/"scripts/public_audit.py",test/"scripts/public_audit.py")
    (test/"firmware/build").mkdir(parents=True)
    (test/"firmware/build/raylib_x4_firmware.bin").write_bytes(b"changed firmware")
    (test/"artifacts/hardware").mkdir(parents=True)
    report=test/"artifacts/hardware/qa-release-report.json"
    for data, message in [(None,"missing"), ({"result":"PASS","application_sha256":"0"*64},"does not match")]:
        if data is not None:
            report.write_text(json.dumps(data))
        result=subprocess.run([sys.executable,test/"scripts/package.py","--hardware-tested"],capture_output=True,text=True)
        assert result.returncode != 0 and message in result.stderr, result
        assert not (test/"dist").exists()
    (test/"firmware/build/CMakeCache.txt").write_text("X4_QEMU_SMOKE:BOOL=OFF\n")
    (test/"build-tetris").mkdir()
    (test/"build-tetris/raylib_x4_firmware.bin").write_bytes(b"test-only firmware")
    (test/"build-tetris/CMakeCache.txt").write_text("X4_APP_SOURCE:FILEPATH=/example/examples/tetris.c\nX4_QEMU_SMOKE:BOOL=ON\n")
    result=subprocess.run([sys.executable,test/"scripts/package.py"],capture_output=True,text=True)
    assert result.returncode != 0 and "Tetris QEMU" in result.stderr, result
    assert not (test/"dist/tetris.bin").exists()
    (test/"build-tetris/CMakeCache.txt").write_text("X4_QEMU_SMOKE:BOOL=OFF\n")
    output=test/"dist"
    output.mkdir()
    (output/"application.bin").write_bytes(b"previous release")
    (output/"notes.txt").write_bytes(b"unrelated file")
    result=subprocess.run([sys.executable,test/"scripts/package.py"],capture_output=True,text=True)
    assert result.returncode != 0 and "unexpected files" in result.stderr, result
    assert (output/"notes.txt").read_bytes()==b"unrelated file"
    (output/"notes.txt").unlink()
    application=test/"firmware/build/raylib_x4_firmware.bin"
    application.write_bytes(b"firmware\0" + b"/home/" + b"builder/project.c\0")
    result=subprocess.run([sys.executable,test/"scripts/package.py"],capture_output=True,text=True)
    assert result.returncode != 0 and "home directory" in result.stderr, result
    application.write_bytes(b"clean firmware")
    for name in ("bootloader/bootloader.bin","partition_table/partition-table.bin"):
        path=test/"firmware/build"/name; path.parent.mkdir(exist_ok=True); path.write_bytes(b"fixture")
    (test/"LICENSE").write_text("fixture")
    (test/"THIRD_PARTY.md").write_text("fixture")
    (test/"CMakeLists.txt").write_bytes(b"/home/" + b"builder/project/source")
    result=subprocess.run([sys.executable,test/"scripts/package.py"],capture_output=True,text=True)
    assert result.returncode != 0 and "home directory" in result.stderr, result
    assert (output/"application.bin").read_bytes()==b"previous release"
    assert list(output.iterdir())==[output/"application.bin"]
    assert not list(test.glob('build-package-*'))
    (test/'build-hardware').mkdir()
    (test/'build-hardware/raylib_x4_firmware.bin').write_bytes(b'test-only firmware')
    (test/'build-hardware/CMakeCache.txt').write_text('X4_APP_SOURCE:FILEPATH=/example/examples/diagnostics.c\nX4_QEMU_SMOKE:BOOL=ON\n')
    result=subprocess.run([sys.executable,test/"scripts/package.py"],capture_output=True,text=True)
    assert result.returncode != 0 and "diagnostic QEMU" in result.stderr, result
    assert (output/"application.bin").read_bytes()==b"previous release"
    shutil.rmtree(test/'build-hardware')
    result=subprocess.run([sys.executable,test/"scripts/package.py","--firmware-only"],capture_output=True,text=True)
    assert result.returncode == 0, result
    assert not (output/'raylib-x4-source.tar.gz').exists()
    assert (output/'raylib-x4.bin').read_bytes()[0x10000:] == application.read_bytes()
    assert not json.loads((output/'manifest.json').read_text())['hardware_tested']
    for line in (output/'SHA256SUMS').read_text().splitlines():
        digest, name = line.split('  ')
        assert hashlib.sha256((output/name).read_bytes()).hexdigest() == digest
    (test/'firmware/build/CMakeCache.txt').write_text('X4_QEMU_SMOKE:BOOL=ON\n')
    previous = (output/'SHA256SUMS').read_bytes()
    result=subprocess.run([sys.executable,test/"scripts/package.py","--firmware-only"],capture_output=True,text=True)
    assert result.returncode != 0 and "test-only QEMU" in result.stderr, result
    assert (output/'SHA256SUMS').read_bytes() == previous
    for device in ('x3', 'x4-classic', 'x4-pro'):
        (test/'firmware/build/CMakeCache.txt').write_text(f'XTEINK_DEVICE:STRING={device}\n')
        result=subprocess.run([sys.executable,test/"scripts/package.py","--firmware-only"],capture_output=True,text=True)
        assert result.returncode != 0 and 'original X4 release' in result.stderr, result
        assert (output/'SHA256SUMS').read_bytes() == previous
print("PASS: missing and stale hardware attestations rejected before packaging")
print("PASS: Tetris QEMU image rejected before packaging firmware")
print("PASS: private binary/source content and unexpected files rejected; previous release preserved")
print("PASS: diagnostic QEMU image rejected before packaging firmware")
print("PASS: firmware-only export checksums, flash offset, and QEMU rejection")
