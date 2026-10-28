#!/usr/bin/env python3
"""Validate MCU selection and reject incompatible board/controller combinations."""
from pathlib import Path
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[1]
cmake = sys.argv[1]
with tempfile.TemporaryDirectory(prefix='xteink-config-') as directory:
    script = Path(directory) / 'check.cmake'
    script.write_text('cmake_minimum_required(VERSION 3.20)\n'
                      f'include("{root}/cmake/xteink-firmware.cmake")\n'
                      'message(STATUS "MCU=${IDF_TARGET}")\n')
    for device, panels, mcu in [('x4', ['ssd1677'], 'esp32c3'), ('x3', ['uc8253', 'uc8279'], 'esp32c3'),
                              ('x4-classic', ['ssd1677', 'uc8179', 'uc8279'], 'esp32s3'),
                              ('x4-pro', ['ssd1677', 'uc8179', 'uc8279'], 'esp32s3')]:
        for panel in ['ssd1677', 'uc8253', 'uc8179', 'uc8279', 'unknown']:
            result = subprocess.run([cmake, f'-DXTEINK_DEVICE={device}', f'-DXTEINK_PANEL={panel}', '-P', script], capture_output=True, text=True)
            if panel in panels:
                assert result.returncode == 0 and f'MCU={mcu}' in result.stdout, result
            else:
                assert result.returncode != 0 and 'Incompatible XTEINK_PANEL' in result.stderr, result
        wrong = 'esp32s3' if mcu == 'esp32c3' else 'esp32c3'
        result = subprocess.run([cmake, f'-DXTEINK_DEVICE={device}', f'-DIDF_TARGET={wrong}', '-P', script], capture_output=True, text=True)
        assert result.returncode != 0 and 'requires IDF_TARGET=' in result.stderr, result
    result = subprocess.run([cmake, '-DXTEINK_DEVICE=unknown', '-P', script], capture_output=True, text=True)
    assert result.returncode != 0 and 'Unknown XTEINK_DEVICE' in result.stderr, result
print('PASS: all nine board/panel builds select the correct MCU; invalid pairs and wrong MCU rejected')
