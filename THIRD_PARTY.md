# Third-party code

| Component | Revision | License | Use |
|---|---|---|---|
| raylib / rlsw | `debc1f6b1f86ca12119824f15974758d2d875394` | zlib; embedded dependencies retain their notices | Vendored drawing/runtime, modified as recorded in `patches/raylib-x4.patch` |
| microreader | `a4adfdb9a37b83bcb724cac3dc8990f335b49e8a` | GPL-2.0 | Original X4 desktop display emulator/runtime only; fetched by `scripts/fetch-emulator.py` |
| FreeInk SDK | `252e9bb05fb5e37d41cc506b359f8e62e9739665` | MIT | Adapted UltraChip register sequences and X3 LUT tables; board/peripheral documentation |
| SDL2 API implementation | Nix lock | zlib | Desktop emulator; Nix uses sdl2-compat 2.32.58 |
| ESP-IDF | v5.5.2 | Apache-2.0 and component licenses | Hardware SDK, FreeRTOS, FAT, newlib, chip drivers |
| Espressif QEMU | `esp-develop-9.2.2-20250817` | GPL-2.0 and component licenses | Test tool only |
| nixpkgs-esp-dev | `5287d6e1ca9e15ebd5113c41b9590c468e1e001b` | CC0 | Declarative toolchain and QEMU derivations |

The main target's zlib license does not relicense these dependencies. The
microreader emulator adapter is GPL-2.0-only; binaries that link it are GPL-2.0
combined works. Other desktop profiles use the same GPL-2.0-only adapter with a
local SDL display implementation instead of microreader. The hardware firmware does not link microreader. Upstream
raylib's license is at `vendor/raylib/LICENSE`; individual dependency notices are
in its source files. The GPL-2.0 text is included at `LICENSES/GPL-2.0-only.txt`; microreader's original license also accompanies its fetched source.

Source locations: [raylib](https://github.com/raysan5/raylib),
[microreader](https://github.com/CidVonHighwind/microreader),
[ESP-IDF](https://github.com/espressif/esp-idf/tree/v5.5.2),
[Espressif QEMU](https://github.com/espressif/qemu),
[Nix toolchain](https://github.com/mirrexagon/nixpkgs-esp-dev).

Crosspoint-Emulator and raylib-esp32 were inspected during research and are not
linked or redistributed as part of the deliverable.

The UltraChip driver (`src/xteink_epd.c`) and selected LUT tables
(`src/xteink_luts.h`) derive from [FreeInk](https://github.com/Free-Ink/freeink-sdk/tree/252e9bb05fb5e37d41cc506b359f8e62e9739665).
Its copyright and MIT license are preserved in `LICENSES/FreeInk-MIT.txt`.
The firmware does not require a FreeInk checkout or link its Arduino runtime.
