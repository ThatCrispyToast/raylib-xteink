# Docker builds

Docker builds firmware and runs headless tests. The interactive desktop emulator uses the [native CMake workflow](emulator.md); USB backup and flashing run on the [host](hardware.md#back-up-and-install).

Use Docker Engine with Buildx/BuildKit, or Docker Desktop in Linux-container mode. Run commands from the project root. No local compiler, ESP-IDF installation, Nix, USB forwarding, or display forwarding is needed for these builds. The first build downloads several gigabytes of SDK tools; subsequent builds reuse image layers and a persistent compiler cache.

## Build firmware

Build Paper and export files into the checkout:

```sh
docker build --output type=local,dest=build-docker/paper .
```

Build Tetris into a separate directory:

```sh
docker build --build-arg X4_APP_SOURCE=examples/tetris.c \
  --output type=local,dest=build-docker/tetris .
```

The final Docker stage exports files rather than a runnable container. The local exporter creates files owned by the invoking user on Linux. The build does not mount or modify your checkout, existing native builds, or local `firmware/sdkconfig`.

Each output directory contains:

| File | Use |
|---|---|
| `raylib-x4.bin` | Complete installation at offset `0`: selected app, bootloader, partition table |
| `application.bin` | Selected app at `0x10000`, with this target's bootloader and partition table already installed |
| `bootloader.bin`, `partition-table.bin` | Individual components of the merged image |
| `SHA256SUMS`, `manifest.json` | Checksums, flash settings, and hardware-test status |
| `LICENSE`, `THIRD_PARTY.md` | License and dependency notices |

The filenames are the same for every application; keep separate output directories. Docker replaces matching exported files, so use a build directory rather than a backup or release directory. These exports do not include the release source archive and do not claim physical-device validation.

Follow [backup and installation](hardware.md#back-up-and-install) before flashing. Building alone never accesses a reader.

## Custom applications

`X4_APP_SOURCE` is relative to the project root:

```sh
docker build --build-arg X4_APP_SOURCE=examples/minimal.c \
  --output type=local,dest=build-docker/minimal .
```

Additional C files use CMake's semicolon-separated list and paths inside the container, for example `--build-arg 'X4_APP_EXTRA_SOURCES=/project/examples/helpers.c;/project/examples/model.c'`. Paper and Tetris automatically include their companion sources. See [application development](application.md) for the entry-point API.

[`.dockerignore`](../.dockerignore) limits the build context to source inputs. Put custom code under `examples/`, `src/`, or `include/`, or explicitly include its directory in that file. Git metadata, native build outputs, local SDK configuration, virtual environments, SD contents, and backups are excluded. Changes to docs alone do not rebuild firmware.

## Headless tests

```sh
docker build --target test --output type=local,dest=build-docker/tests .
```

This runs the native CTest suite through microreader/SDL, portable ASan/UBSan tests, and the ESP32-C3 QEMU smoke test. It exports `native-tests.xml`, `sanitizer-tests.xml`, and `qemu.log`. A failure stops the build; the QEMU binary is never exported as flashable firmware.

An unchanged successful build can be cached, including its test results. To execute the tests again while retaining dependency and compiler caches:

```sh
docker buildx build --no-cache-filter native-test,sanitizer-test,qemu-test \
  --target test --output type=local,dest=build-docker/tests .
```

The [Docker CI workflow](../.github/workflows/docker.yml) uses these targets and builds both examples. The existing Nix CI separately checks the locked native environment.

## Toolchain and scope

The [Dockerfile](../Dockerfile) pins Espressif's official ESP-IDF **5.5.2** image and Ubuntu **24.04** by image-index digest. The native image installs the distribution's CMake, GCC, and SDL2 packages; these package repositories can receive updates. microreader is pinned by commit. Compiler caches are disposable and are not required for a correct build. This pins the SDK environment, not byte-for-byte output: ESP-IDF embeds compilation timestamps.

The base images provide Linux x86-64 and ARM64 variants. Local validation uses Docker on x86-64 Linux; macOS, Windows/Docker Desktop, and ARM64 hosts have not been exercised. The build commands avoid host paths and shell-specific variables, but native Windows shells should put multiline commands on one line. Device-port and virtual-environment commands differ by host; see the hardware guide.

References: [Espressif's image](https://hub.docker.com/r/espressif/idf/), [Docker local exports](https://docs.docker.com/build/exporters/local-tar/), and [build caching](https://docs.docker.com/build/cache/optimize/).
