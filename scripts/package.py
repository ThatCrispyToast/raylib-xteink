#!/usr/bin/env python3
"""Package built hardware firmware and the reproducible project source."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tarfile
import tempfile
from public_audit import check_file

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hardware-tested',action='store_true',help='Require a matching physical QA report for this application binary')
parser.add_argument('--firmware-only', action='store_true', help='Export firmware and checksums without a source archive')
args = parser.parse_args()
build, destination = root / "firmware/build", root / "dist"
release_names = {"bootloader.bin", "partition-table.bin", "application.bin", "raylib-x4.bin",
                 "diagnostics.bin", "tetris.bin", "LICENSE", "THIRD_PARTY.md", "manifest.json",
                 "raylib-x4-source.tar.gz", "SHA256SUMS"}
if destination.is_symlink():
    raise SystemExit('Refusing to replace a symlinked release directory')
if destination.exists() and any(p.name not in release_names or not p.is_file() or p.is_symlink() for p in destination.iterdir()):
    raise SystemExit('Release directory contains unexpected files; move them outside dist before packaging')
staging = tempfile.TemporaryDirectory(prefix='build-package-', dir=root)
dist = Path(staging.name) / 'dist'

def audit(name, data):
    findings = check_file(name, data)
    if findings:
        raise SystemExit(f'Public release rejected: {name}: {", ".join(findings)}')
application_data = (build / "raylib_x4_firmware.bin").read_bytes()
audit('application.bin', application_data)
application_hash = hashlib.sha256(application_data).hexdigest()
if args.hardware_tested:
    report_path=root/'artifacts/hardware/qa-release-report.json'
    if not report_path.exists():
        raise SystemExit('Physical QA report is missing for this application')
    report=json.loads(report_path.read_text())
    if report.get('result') != 'PASS' or report.get('application_sha256') != application_hash:
        raise SystemExit('Physical QA report does not match the application being packaged')
dist.mkdir(exist_ok=True)
cache = (build / "CMakeCache.txt").read_text()
application_source = next((line.split('=', 1)[1] for line in cache.splitlines()
                           if line.startswith('X4_APP_SOURCE:')), 'examples/paper.c')
def qemu_build(cache):
    return any(line.startswith("X4_QEMU_SMOKE:") and line.endswith(("=ON", "=1", "=TRUE")) for line in cache.splitlines())
if qemu_build(cache):
    raise SystemExit("Refusing to package a test-only QEMU image as hardware firmware")
tetris = root/'build-tetris/raylib_x4_firmware.bin'
tetris_cache = (tetris.parent/'CMakeCache.txt').read_text() if tetris.exists() else ''
if 'examples/tetris.c' in tetris_cache and qemu_build(tetris_cache):
    raise SystemExit("Refusing to package a Tetris QEMU test image as hardware firmware")
diagnostic = root/'build-hardware/raylib_x4_firmware.bin'
diagnostic_cache = (diagnostic.parent/'CMakeCache.txt').read_text() if diagnostic.exists() else ''
if 'examples/diagnostics.c' in diagnostic_cache and qemu_build(diagnostic_cache):
    raise SystemExit("Refusing to package a diagnostic QEMU test image as hardware firmware")
parts = [(0, "bootloader/bootloader.bin", "bootloader.bin"),
         (0x8000, "partition_table/partition-table.bin", "partition-table.bin"),
         (0x10000, "raylib_x4_firmware.bin", "application.bin")]
image = bytearray()
for offset, source, name in parts:
    data = (build / source).read_bytes()
    audit(name, data)
    if len(image) > offset:
        raise SystemExit("Firmware sections overlap")
    image.extend(b"\xff" * (offset - len(image)))
    image.extend(data)
    (dist / name).write_bytes(data)
(dist / "raylib-x4.bin").write_bytes(image)
(dist/'diagnostics.bin').unlink(missing_ok=True)
if diagnostic.exists() and 'examples/diagnostics.c' in diagnostic_cache:
    audit('diagnostics.bin', diagnostic.read_bytes())
    shutil.copy2(diagnostic,dist/'diagnostics.bin')
tetris_info = None
(dist/'tetris.bin').unlink(missing_ok=True)
if tetris.exists() and 'examples/tetris.c' in tetris_cache:
    audit('tetris.bin', tetris.read_bytes())
    shutil.copy2(tetris,dist/'tetris.bin')
    digest = hashlib.sha256(tetris.read_bytes()).hexdigest()
    evidence = root/'artifacts/hardware/tetris-report.json'
    report = json.loads(evidence.read_text()) if evidence.exists() else {}
    verified = args.hardware_tested and report.get('result') == 'PASS' and report.get('application_sha256') == digest
    tetris_info = dict(file='tetris.bin',offset=65536,application_sha256=digest,hardware_tested=verified)
for name in ["LICENSE", "THIRD_PARTY.md"]:
    shutil.copy2(root / name, dist / name)
(dist / "manifest.json").write_text(json.dumps({
    "target": "original Xteink X4 / ESP32-C3", "version": "1.2.0", "application_sha256": application_hash,
    "application_source": Path(application_source).name,
    "flash_size": "16MB", "flash_mode": "dio", "flash_frequency": "40m",
    "merged_image": {"file": "raylib-x4.bin", "offset": 0},
    "parts": [{"file": name, "offset": offset} for offset, _, name in parts],
    "hardware_tested": args.hardware_tested,
    "diagnostics_image": {"file":"diagnostics.bin", "offset":65536, "hardware_tested":False,
        "application_sha256":hashlib.sha256(diagnostic.read_bytes()).hexdigest()} if (dist/'diagnostics.bin').exists() else None,
    "tetris_example": tetris_info,
    "validation": "See docs/validation.md for platform evidence; hardware_tested applies to this exact binary."
}, indent=2) + "\n")
paths = ["CMakeLists.txt", "cmake", "src", "include", "examples", "tests", "scripts",
         "docs", "patches", "README.md", "CONTRIBUTING.md", "CHANGELOG.md", "LICENSE", "LICENSES", "THIRD_PARTY.md", "flake.nix", "flake.lock",
         ".gitignore", ".gitattributes", ".dockerignore", "Dockerfile", ".github", "firmware/CMakeLists.txt", "firmware/main", "firmware/partitions.csv",
         "firmware/sdkconfig.defaults", "firmware/sdkconfig.qemu", "vendor/raylib/src", "vendor/raylib/LICENSE",
         "artifacts/demo.png", "artifacts/puzzle.png", "artifacts/status.png", "artifacts/tetris.png",
         "artifacts/native-tests.txt", "artifacts/sanitizer-tests.txt", "artifacts/tetris-pace-tests.txt", "artifacts/qemu-test.log",
         "artifacts/firmware-build.log", "artifacts/minimal-build.log", "artifacts/image-info.txt"]
paths += ["artifacts/publication-native-tests.txt", "artifacts/publication-sanitizer-tests.txt",
          "artifacts/publication-archive-tests.txt", "artifacts/publication-report.json",
          "artifacts/speed-firmware-build.log", "artifacts/speed-native-tests.txt", "artifacts/speed-sanitizer-tests.txt",
          "artifacts/speed-qemu-1.log", "artifacts/speed-qemu-2.log", "artifacts/speed-qemu-3.log",
          "artifacts/speed-qemu-report.json", "artifacts/speed-qemu-before-icount.log"]
if args.hardware_tested:
    paths += ["artifacts/hardware/speed-performance.log", "artifacts/hardware/speed-report.json",
              "artifacts/hardware/async-display-test.log", "artifacts/hardware/tetris-speed-test.log",
              "artifacts/hardware/tetris-speed-pace.log", "artifacts/hardware/speed-sleep.log",
              "artifacts/hardware/speed-paper-boot.log", "artifacts/hardware/speed-tetris-boot.log",
              "artifacts/hardware/speed-archive-check.log",
              "artifacts/hardware/qa-release-report-1.1.2.json", "artifacts/hardware/tetris-report-1.1.2.json",
              "artifacts/hardware/speed-regression/report.json", "artifacts/hardware/speed-regression/partial-soak.png"]
    paths += ["artifacts/hardware/tetris-pace-before.log", "artifacts/hardware/tetris-pace-after.log",
              "artifacts/hardware/tetris-pace-report.json", "artifacts/hardware/tetris-before-pace-report.json",
              "artifacts/hardware/tetris-report.json", "artifacts/hardware/tetris-test.log", "artifacts/hardware/tetris-boot.log",
              "artifacts/hardware/qa-1.1.1-report.json", "artifacts/hardware/region-performance.log",
              "artifacts/hardware/region-performance-report.json", "artifacts/hardware/region-regression/report.json",
              "artifacts/hardware/region-regression/partial-soak.png", "artifacts/hardware/region-optical-report.json",
              "artifacts/hardware/performance-before.log", "artifacts/hardware/performance-after.log",
              "artifacts/hardware/performance-report.json", "artifacts/hardware/performance-regression/report.json",
              "artifacts/hardware/qa-release-report.json", "artifacts/hardware/qa-1.1.0-report.json", "artifacts/hardware/qa-paper.log",
              "artifacts/hardware/final-acceptance/report.json", "artifacts/hardware/buttons-report.json",
              "artifacts/hardware/sleep-console.log", "artifacts/hardware/final-regression.log",
              "artifacts/hardware/release-report.json", "artifacts/hardware/archive-check.log",
              "artifacts/hardware/final-acceptance/scene-0.png", "artifacts/hardware/final-acceptance/scene-3.png",
              "artifacts/hardware/final-acceptance/sd-asset.png"]
tracked = None
if (root/'.git').exists():
    tracked = set(subprocess.check_output(['git', '-C', root, 'ls-files', '-z']).decode().split('\0'))
if not args.firmware_only:
    with tarfile.open(dist / "raylib-x4-source.tar.gz", "w:gz") as archive:
        def clean(info):
            if "__pycache__" in info.name or info.name.endswith(".pyc"):
                return None
            if not (info.isfile() or info.isdir()):
                raise SystemExit(f'Public release rejected: nonregular source entry {info.name}')
            name = info.name.removeprefix('raylib-x4/')
            if tracked is not None:
                if info.isfile() and name not in tracked: return None
                if info.isdir() and not any(p.startswith(name+'/') for p in tracked): return None
            audit(name, (root/name).read_bytes() if info.isfile() else b'')
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            return info
        for path in paths:
            archive.add(root / path, arcname="raylib-x4/" + path, filter=clean)
files = sorted(p for p in dist.iterdir() if p.is_file() and p.name != "SHA256SUMS")
for file in files:
    if not file.name.endswith('.tar.gz'):
        audit(file.name, file.read_bytes())
(dist / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in files))
previous = Path(staging.name) / 'previous'
if destination.exists(): destination.rename(previous)
try:
    dist.rename(destination)
except OSError:
    if previous.exists(): previous.rename(destination)
    raise
staging.cleanup()
print(f"Packaged {len(image):,}-byte merged firmware in {destination}")
