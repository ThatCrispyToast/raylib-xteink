#!/usr/bin/env python3
"""Require byte-identical packed pixels from accelerated and reference rasterizers."""
from pathlib import Path
import subprocess
import sys
import tempfile
build=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="x4-raster-") as directory:
    files=[]
    for name in ["x4_raster_fast", "x4_raster_reference"]:
        path=Path(directory)/name
        subprocess.run([build/name,path],check=True)
        files.append(path.read_bytes())
    assert files[0] == files[1], "Accelerated renderer differs from reference"
print("PASS: 32 scenes match byte-for-byte: clipping, scales, text alpha/color, rectangles, camera and triangles")
