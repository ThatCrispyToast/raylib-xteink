# syntax=docker/dockerfile:1
FROM ubuntu:24.04@sha256:24737cef17f4e22b1977df7d9a05e77c4e28b7da5b8911f8d8ccda136bf3573c AS native-build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates ccache cmake git libsdl2-dev ninja-build pkg-config python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /project
COPY scripts/fetch-emulator.py scripts/fetch-emulator.py
RUN python3 scripts/fetch-emulator.py
COPY . .
ENV CCACHE_DIR=/ccache
RUN --mount=type=cache,id=x4-native-ccache,target=/ccache \
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build build --parallel 4

FROM native-build AS native-test
RUN ctest --test-dir build --output-on-failure --output-junit /native-tests.xml

FROM native-build AS sanitizer-test
RUN --mount=type=cache,id=x4-native-ccache,target=/ccache \
    cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DX4_SANITIZE=ON -DX4_BUILD_EMULATOR=OFF \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build build-sanitize --parallel 4 \
    && ctest --test-dir build-sanitize --output-on-failure --output-junit /sanitizer-tests.xml

FROM espressif/idf:v5.5.2@sha256:05cbfc42ed2e987b8026722c15bf1d8523d3e4fd1b4ac04d2e4056f5e0918b99 AS idf-build
WORKDIR /project
COPY . .
SHELL ["/opt/esp/entrypoint.sh", "/bin/bash", "-c"]
ENV CCACHE_DIR=/ccache

FROM idf-build AS qemu-test
RUN --mount=type=cache,id=x4-idf-ccache,target=/ccache,sharing=locked \
    idf.py -C firmware -B /project/build-qemu -DX4_QEMU_SMOKE=ON \
      -DSDKCONFIG=/project/build-qemu/sdkconfig build \
    && python3 scripts/test-qemu.py

FROM scratch AS test
COPY --from=native-test /native-tests.xml /
COPY --from=sanitizer-test /sanitizer-tests.xml /
COPY --from=qemu-test /project/build-qemu/qemu.log /

FROM idf-build AS firmware-build
ARG X4_APP_SOURCE=examples/paper.c
ARG X4_APP_EXTRA_SOURCES=
RUN --mount=type=cache,id=x4-idf-ccache,target=/ccache,sharing=locked \
    idf.py -C firmware -B /project/firmware/build \
      -DSDKCONFIG=/project/firmware/build/sdkconfig \
      -DX4_APP_SOURCE="/project/${X4_APP_SOURCE}" \
      -DX4_APP_EXTRA_SOURCES="${X4_APP_EXTRA_SOURCES}" build size \
    && python3 scripts/package.py --firmware-only

FROM scratch AS firmware
COPY --from=firmware-build /project/dist/ /
