# Changelog

## 1.2.0 — 2026-10-27

- Reuse temperature-selected factory partial waveforms during activity; shut panel power down after two idle seconds and refresh temperature selection after 30 seconds.
- Add optional asynchronous presentation with immutable in-flight frames and latest-frame coalescing. Paper and Tetris keep processing ordinary controls during refreshes while critical screens still wait for completion.
- Reduce packed conversion to about 5.4 ms and cursor/menu completion to about 0.44 seconds on the tested X4. Preserve the 192 KB pixel-buffer budget and 20 MHz SPI clock.
- Add the Tetris example with Normal/Practice modes, retained drawing, accelerated held controls, and visible landing grace.
- Harden display timeouts, USB diagnostic replies, and ESP32-C3 QEMU timing. Extend emulator and physical-device stress tests.

## 1.1.2 — 2026-10-26

- Transfer only changed panel regions and retain puzzle rendering.
- Make Paper cleaning explicit: startup/wake and Power, with no navigation or idle cleaning flashes.

## 1.1.1 — 2026-10-26

- Reduce drawing/conversion latency and preserve ordered input across blocking refreshes.
- Protect confirmations from buffered input and improve retained cursor/menu drawing.

## 1.1.0 — 2026-10-26

- Add undo, restart, fresh puzzles, help, and storage feedback to Paper.
- Use alternating checksummed save records and validate legacy imports without changing unrelated files.
- Harden file loading and no-card emulation; expand independent user acceptance tests.

## 1.0.0 — 2026-10-26

- Initial original-X4 target, Paper example, microreader adapter, ESP-IDF/QEMU builds, and physical acceptance tools.
