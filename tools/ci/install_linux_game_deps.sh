#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  libglew-dev \
  libglfw3-dev \
  libglm-dev \
  libassimp-dev \
  libgl1-mesa-dev \
  xorg-dev
