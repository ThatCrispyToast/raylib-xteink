#!/usr/bin/env python3
"""A stale successful hardware report must not attest a different binary."""
from pathlib import Path
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
print("PASS: missing and stale hardware attestations rejected before packaging")
