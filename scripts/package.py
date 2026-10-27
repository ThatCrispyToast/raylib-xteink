#!/usr/bin/env python3
"""Package built hardware firmware and the reproducible project source."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import tarfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hardware-tested',action='store_true',help='Require a matching physical QA report for this application binary')
args = parser.parse_args()
build, dist = root / "firmware/build", root / "dist"
application_hash = hashlib.sha256((build / "raylib_x4_firmware.bin").read_bytes()).hexdigest()
if args.hardware_tested:
    report_path=root/'artifacts/hardware/qa-release-report.json'
    if not report_path.exists():
        raise SystemExit('Physical QA report is missing for this application')
    report=json.loads(report_path.read_text())
    if report.get('result') != 'PASS' or report.get('application_sha256') != application_hash:
        raise SystemExit('Physical QA report does not match the application being packaged')
dist.mkdir(exist_ok=True)
cache = (build / "CMakeCache.txt").read_text()
if any(line.startswith("X4_QEMU_SMOKE:") and line.endswith(("=ON", "=1", "=TRUE")) for line in cache.splitlines()):
    raise SystemExit("Refusing to package a test-only QEMU image as hardware firmware")
parts = [(0, "bootloader/bootloader.bin", "bootloader.bin"),
         (0x8000, "partition_table/partition-table.bin", "partition-table.bin"),
         (0x10000, "raylib_x4_firmware.bin", "application.bin")]
image = bytearray()
for offset, source, name in parts:
    data = (build / source).read_bytes()
    if len(image) > offset:
        raise SystemExit("Firmware sections overlap")
    image.extend(b"\xff" * (offset - len(image)))
    image.extend(data)
    (dist / name).write_bytes(data)
(dist / "raylib-x4.bin").write_bytes(image)
diagnostic = root/'build-hardware/raylib_x4_firmware.bin'
(dist/'diagnostics.bin').unlink(missing_ok=True)
if diagnostic.exists() and 'examples/diagnostics.c' in (diagnostic.parent/'CMakeCache.txt').read_text():
    shutil.copy2(diagnostic,dist/'diagnostics.bin')
for name in ["LICENSE", "THIRD_PARTY.md"]:
    shutil.copy2(root / name, dist / name)
(dist / "manifest.json").write_text(json.dumps({
    "target": "original Xteink X4 / ESP32-C3", "version": "1.1.2", "application_sha256": application_hash,
    "flash_size": "16MB", "flash_mode": "dio", "flash_frequency": "40m",
    "merged_image": {"file": "raylib-x4.bin", "offset": 0},
    "parts": [{"file": name, "offset": offset} for offset, _, name in parts],
    "hardware_tested": args.hardware_tested,
    "diagnostics_image": {"file":"diagnostics.bin","offset":65536} if (dist/'diagnostics.bin').exists() else None,
    "validation": "microreader desktop emulator, native ASan/UBSan, ESP32-C3 QEMU renderer smoke" +
        (", original X4 production boot and physical storage regression; historical platform acceptance in docs/validation.md" if args.hardware_tested else "")
}, indent=2) + "\n")
paths = ["CMakeLists.txt", "cmake", "src", "include", "examples", "tests", "scripts",
         "docs", "patches", "README.md", "LICENSE", "THIRD_PARTY.md", "flake.nix", "flake.lock",
         ".gitignore", "firmware/CMakeLists.txt", "firmware/main", "firmware/partitions.csv",
         "firmware/sdkconfig.defaults", "firmware/sdkconfig.qemu", "vendor/raylib/src", "vendor/raylib/LICENSE",
         "artifacts/demo.png", "artifacts/puzzle.png", "artifacts/status.png",
         "artifacts/native-tests.txt", "artifacts/sanitizer-tests.txt", "artifacts/qemu-test.log",
         "artifacts/firmware-build.log", "artifacts/minimal-build.log", "artifacts/image-info.txt"]
if args.hardware_tested:
    paths += ["artifacts/hardware/qa-1.1.1-report.json", "artifacts/hardware/region-performance.log",
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
with tarfile.open(dist / "raylib-x4-source.tar.gz", "w:gz") as archive:
    def clean(info):
        if "__pycache__" in info.name or info.name.endswith(".pyc"):
            return None
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        return info
    for path in paths:
        archive.add(root / path, arcname="raylib-x4/" + path, filter=clean)
files = sorted(p for p in dist.iterdir() if p.is_file() and p.name != "SHA256SUMS")
(dist / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in files))
print(f"Packaged {len(image):,}-byte merged firmware and source in {dist}")
