# Preparing a release

Start with a clean checkout and the pinned environments in the README. Build Paper in `firmware/build` and, optionally, Tetris in `build-tetris`. `build-hardware` may contain the diagnostic app. Do not distribute QEMU smoke images as hardware firmware.

Run the native suite, sanitizer suite, and QEMU smoke test. Hardware behavior requires a physical device; keep its backup outside the project. The generic diagnostic runner writes temporary test files on the SD card, so use a test card or back it up first. Record the exact tested application hash and distinguish boot verification from full interactive or optical testing.

```sh
python3 scripts/public_audit.py --history
python3 scripts/package.py
python3 scripts/public_audit.py --dist dist
```

`--hardware-tested` requires a passing Paper report matching the exact current binary. Tetris has its own hash and attestation in the manifest. The optional diagnostic image is identified separately and carries no production hardware attestation. Rebuilding on another machine may change the binary hash; do not reuse an older report to claim the new image was tested.

Packaging writes into a temporary directory, scans firmware and source content, and replaces `dist/` only after success. Unexpected files in an existing `dist/` cause a refusal so unrelated files cannot slip into a release. The source archive uses explicit source directories and files, includes only Git-tracked files when built from a checkout, strips filesystem owner names, and excludes Git metadata, build outputs, research checkouts, SD data, and device backups. Firmware compilation maps project paths in `__FILE__` strings to relative paths. Audit the resulting binaries as well as the source.

Distribute the files in `dist/` with `SHA256SUMS`, `manifest.json`, and `raylib-x4-source.tar.gz`. Confirm the source archive builds from a fresh extraction with `X4_BUILD_EMULATOR=OFF`; fetch the pinned microreader source to run emulator tests. The archive includes the target's license, third-party notices, and the GPL-2.0 text for the emulator adapter. Emulator executables are not included in the firmware release.

The checked-in historical logs have local checkout paths replaced with `/workspace/raylib-x4`. Test measurements, firmware hashes, and failure traces are preserved. Commit references were updated during pre-publication history cleanup. Contributor identities and upstream license notices remain intact.
