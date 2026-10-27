#!/usr/bin/env python3
"""Fetch the pinned third-party emulator without modifying an existing checkout."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
destination = root / "research/microreader"
revision = "a4adfdb9a37b83bcb724cac3dc8990f335b49e8a"
url = "https://github.com/CidVonHighwind/microreader.git"
if destination.exists():
    actual = subprocess.check_output(["git", "-C", destination, "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit(f"Existing emulator is {actual}, expected {revision}; choose a separate checkout")
else:
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "init", destination], check=True)
    subprocess.run(["git", "-C", destination, "fetch", "--depth", "1", url, revision], check=True)
    subprocess.run(["git", "-C", destination, "checkout", "--detach", "FETCH_HEAD"], check=True)
print(f"microreader {revision}: {destination}")
