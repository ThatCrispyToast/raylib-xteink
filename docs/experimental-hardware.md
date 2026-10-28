# Experimental XTEINK firmware

X3, X4 Classic, and X4 Pro have ESP-IDF ports based on documented pinouts and controller sequences. **None has been tested on physical hardware in this project.** Builds, CPU emulation, and simulated-bus tests are recorded in [validation](validation.md). The [original X4 installation guide](hardware.md) and release images remain specific to the original X4.

## Build a device and panel

Use ESP-IDF 5.5.2 with the appropriate toolchain, or the pinned Nix shells below. Run from the project root and use a separate build directory and SDK configuration for each device.

| `XTEINK_DEVICE` | MCU / Nix shell | `XTEINK_PANEL` choices | Default |
|---|---|---|---|
| `x4` | ESP32-C3 / `firmware` | `ssd1677` | `ssd1677` |
| `x3` | ESP32-C3 / `firmware` | `uc8253`, `uc8279` | `uc8253` |
| `x4-classic` | ESP32-S3 / `firmware-s3` | `ssd1677`, `uc8179`, `uc8279` | `ssd1677` |
| `x4-pro` | ESP32-S3 / `firmware-s3` | `ssd1677`, `uc8179`, `uc8279` | `ssd1677` |

Panel selection is explicit; there is no auto-detection. A default is a build choice, not evidence of the panel installed in a particular reader. Confirm the variant before attempting a hardware installation. Configuration rejects unknown devices, incompatible panel selections, and conflicting MCU targets.

```sh
nix develop path:.#firmware -c idf.py -C firmware -B "$PWD/build-x3" \
  -DXTEINK_DEVICE=x3 -DXTEINK_PANEL=uc8253 \
  -DSDKCONFIG="$PWD/build-x3/sdkconfig" build

nix develop path:.#firmware-s3 -c idf.py -C firmware -B "$PWD/build-x4-pro" \
  -DXTEINK_DEVICE=x4-pro -DXTEINK_PANEL=ssd1677 \
  -DSDKCONFIG="$PWD/build-x4-pro/sdkconfig" build
```

For Classic, replace `x4-pro` with `x4-classic` in the second command. Add `-DX4_APP_SOURCE="$PWD/examples/tetris.c"` to build Tetris in a separate application build directory. Both examples support Pro touch navigation. The board determines the MCU automatically; do not reuse a C3 SDK configuration for an S3 build.

The build directory contains `raylib_x4_firmware.bin`, the bootloader, partition table, and IDF's `flash_args`/`flasher_args.json`. These experimental binaries do not carry a hardware-tested claim. `scripts/package.py` and Docker firmware exports remain original-X4 release workflows; packaging rejects another device rather than labeling it as X4. QEMU smoke images replace the hardware HAL and must not be installed on readers.

## Implemented peripherals

| Function | X3 | X4 Classic | X4 Pro |
|---|---|---|---|
| Display | 792 × 528 UC8253 or UC8279 | 800 × 480 SSD1677, UC8179, or UC8279 | Same controller choices, Pro wiring |
| Input | Two ADC ladders and Power; debounced queued presses | Seven active-low GPIO keys | Up/Down/Power, GT911 first contact and Home → Back |
| Storage | Shared SPI SD, enable high | Native 1-bit SDMMC, enable low | Native 1-bit SDMMC, enable low |
| Battery | BQ27220 voltage, I2C `0x55` | CW2017 voltage, I2C `0x63` | CW2017 voltage, I2C `0x63` |
| Frontlight | Absent | Absent | Two 25 kHz PWM channels, brightness/warmth API |
| Sleep | Panel sleep, SD off, Power GPIO wake | Panel sleep, SD off, Power EXT0 wake | Also touch rail and frontlight off |

Missing SD storage is nonfatal and is never formatted. S3 mounting cycles the SD rail and verifies a sector read before reporting success. Fuel gauges are read for voltage only; unavailable or implausible readings return `battery_mv = -1`. Battery profiles and calibration registers are not changed. The input task performs I2C reads outside the timer service and latches taps across display updates.

`XteinkSetFrontlight(brightness, warmth)` accepts 0–100 for each argument. Brightness 0 is off; warmth 0 selects cool and 100 selects warm. The two channels share the brightness budget and start off. On the Pro, Paper's Device Status page provides Off/Cool/Warm controls. The desktop preview changes paper color without triggering an e-paper refresh; it is not photometrically calibrated.

## Board wiring used by the port

| Signal | X3 | X4 Classic | X4 Pro |
|---|---|---|---|
| EPD CLK / MOSI / CS / DC / RST / BUSY | 8 / 10 / 21 / 4 / 5 / 6 | 12 / 11 / 13 / 14 / 10 / 18 | 12 / 11 / 13 / 18 / 14 / 6 |
| SD | MISO 7, CS 12, enable 13 high | CLK 41, CMD 42, D0 40, enable 6 low | CLK 41, CMD 42, D0 40, enable 5 low |
| I2C SDA / SCL | 20 / 0 | 39 / 38 | 39 / 38 |
| Buttons | ADC 1/2, Power 3 | Back 9, Confirm 8, Left 5, Right 2, Up 0, Down 7, Power 3 | Up 0, Down 7, Power 3 |
| Other rails | — | Main enable 1 high | Main 1 high, touch 2 low, touch reset 4 / INT 10, cool 8 / warm 9 |

UltraChip BUSY is active-low; SSD1677 is active-high. The UltraChip drivers preserve OLD/NEW planes, use controller-specific row order and invisible-row padding, support asynchronous completion with a 15-second deadline, and power down after idle. X3 uses FreeInk's full/normal/half UC8253 LUTs, including a full-image conditioning pass and wake scrub, or the recovered stock UC8279 GC/DU banks. The S3 UltraChip variants use their built-in B/W waveforms. All still display the shared renderer's four gray levels through monochrome dithering; native panel grayscale is not implemented. SSD1677 supports changed-region transfers; UltraChip updates currently transfer the full visible frame.

## Remaining validation and support

The checklist in the [README](../README.md#compatibility-and-planned-support) tracks physical testing separately. Orientation, panel crop, waveform quality, SD reliability, touch accuracy, LED polarity/brightness, battery accuracy, sleep retention, and current draw remain unverified. RTC/IMU drivers, battery percentage, panel detection, native grayscale, and wireless APIs are not implemented. S3 builds use internal RAM without requiring PSRAM. Locked devices are not supported by an installation procedure here.

The S4 is an Android device, so it needs an Android application target, lifecycle/storage integration, and device-specific e-paper/frontlight APIs. Its Android runtime is not modeled by the C3/S3 emulators. It remains planned; no S4 support is implied by the Pro's touchscreen preview.

Hardware references are pinned to [FreeInk `252e9bb`](https://github.com/Free-Ink/freeink-sdk/tree/252e9bb05fb5e37d41cc506b359f8e62e9739665), especially its [board definitions](https://github.com/Free-Ink/freeink-sdk/blob/252e9bb05fb5e37d41cc506b359f8e62e9739665/libs/hardware/BoardConfig/include/BoardConfig.h), [Classic notes](https://github.com/Free-Ink/freeink-sdk/blob/252e9bb05fb5e37d41cc506b359f8e62e9739665/docs/xteink-x4c-support.md), and [Pro notes](https://github.com/Free-Ink/freeink-sdk/blob/252e9bb05fb5e37d41cc506b359f8e62e9739665/docs/xteink-x4pro-support.md). The adapted driver/LUT license is preserved in [THIRD_PARTY.md](../THIRD_PARTY.md). These references report other projects' hardware work and do not attest this implementation.
