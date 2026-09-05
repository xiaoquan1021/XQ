#!/usr/bin/env bash
set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

packages=(
    git
    cmake
    wget
    rsync
    ninja-build
    pkg-config
    python3
    perl
    llvm
    gcc-11
    g++-11
    libpcre3
    libpcre3-dev
    automake
    build-essential
    zlib1g-dev
    libbz2-dev
    libreadline-dev
    libsqlite3-dev
    libffi-dev
    liblzma-dev
    uuid-dev
    libgl1-mesa-dev
    libglu1-mesa-dev
    libegl1-mesa-dev
    libgles2-mesa-dev
    libxcb-render0-dev
    libdbus-1-dev
    libnss3-dev
    libxcb-xfixes0-dev
    gperf
    bison
    flex
    byacc
    libxi-dev
    libx11-dev
    libxcomposite-dev
    libxcursor-dev
    libxext-dev
    libxtst-dev
    libatspi2.0-dev
    libx11-xcb-dev
    libxcb-util0-dev
    libxcb-keysyms1-dev
    libxcb-image0-dev
    libxcb-shm0-dev
    libxcb-icccm4-dev
    libxcb-sync-dev
    libxcb-render-util0-dev
    libxcb-xinerama0-dev
    libxcb-xkb-dev
    libxkbcommon-dev
    libxkbcommon-x11-dev
    libdrm-dev
    libxdamage-dev
    libxrandr-dev
    libxrender-dev
    libfontconfig1-dev
    libasound2-dev
    libcups2-dev
    libevent-dev
    libicu-dev
    libudev-dev
    libssl-dev
    python2
    python-is-python2
)

${SUDO} apt-get update
${SUDO} apt-get install -y "${packages[@]}"
