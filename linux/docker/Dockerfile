# SPDX-License-Identifier: GPL-3.0-or-later
# The CardputerZero build environment: Debian trixie arm64, the OS family the
# device runs, so the binary links against the same glibc. Runs natively on
# Apple Silicon and GitHub's ubuntu-24.04-arm runners; no cross toolchain.
# Used by scripts/build-cardputer-zero.sh.
FROM debian:trixie@sha256:9c6e381421d0f48d1b8133e573f33c185ee16cbf505eda9c4a8a4791b25ea8a6

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential ca-certificates cmake ninja-build pkg-config file \
      libcurl4-openssl-dev libasound2-dev libsdl2-dev python3 \
 && rm -rf /var/lib/apt/lists/*
