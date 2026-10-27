# Contributing

Use the pinned environments in the [emulator](docs/emulator.md) and [hardware](docs/hardware.md) guides. Keep changes focused and describe the behavior they fix. The target's C code uses four-space indentation and short comments; preserve surrounding style. Changes to vendored raylib must also be reflected in `patches/raylib-x4.patch`, against the revision in [THIRD_PARTY.md](THIRD_PARTY.md).

Follow [the testing guide](docs/testing.md): run the native CTest suite for target or example changes, and the ASan/UBSan build for memory-related changes. Controller changes also need the SSD1677 bus-model tests. Report emulator, CPU-emulator, and physical-device results separately. A framebuffer capture does not establish panel ghosting or optical latency. CI runs the native emulator and sanitizer suites with the locked Nix toolchain; it cannot certify physical behavior.

Before sharing logs or a release:

```sh
python3 scripts/public_audit.py
python3 scripts/public_audit.py --history
python3 scripts/public_audit.py --dist dist
```

These checks catch common home-directory paths, device identifiers, credential patterns, private-network addresses, backup filenames, and PNG metadata. They do not recognize every possible secret or personal detail; review logs and images too. Contributor names/emails and third-party license attribution are intentionally retained. Do not commit full flash backups, SD contents, credentials, or machine-specific configuration.

See [release preparation](docs/releasing.md) for packaging and hardware attestation. Contributions retain the applicable file license: the main target uses zlib, the microreader adapter uses GPL-2.0-only, and vendored files retain upstream notices.
