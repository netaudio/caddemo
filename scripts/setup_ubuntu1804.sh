#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# 在 Ubuntu 18.04（bionic）容器内准备麒麟 V10 SP1 的构建环境
#
# 为什么是 ubuntu:18.04（2026-11 修正，此前误用 20.04）：
#   麒麟桌面版 V10 SP1 多架构统一 glibc 2.28（arm64 真机实测报
#   "GLIBC_2.29 not found" 证实，麒麟底层是 RHEL8/openEuler 系而非
#   Ubuntu 系）、系统 GCC 7.3 → GLIBCXX 上限 3.4.24。
#   bionic：glibc 2.27（≤2.28，留一版余量）、gcc 7.5（产物最高
#   GLIBCXX_3.4.24，与麒麟精确匹配）。此前 20.04 构建的产物在真机上报
#   "GLIBC_2.29 not found"。
#   bionic 的 cmake 仅 3.10（项目要求 3.20+），build_linux.sh 会用
#   pip 自动升级；bionic python3 是 3.6，因此不要在此装 aqtinstall。
#   （若错用 ubuntu-22.04，glibc 是 2.35，产物到麒麟上直接起不来。）
# ---------------------------------------------------------------------------
set -euo pipefail

# bionic 已 EOL，源已迁至 old-releases.ubuntu.com；直连失败时切换
if ! apt-get update; then
    echo "==> 主源不可用，切换 old-releases.ubuntu.com"
    sed -i \
        -e 's|http://archive.ubuntu.com/ubuntu|http://old-releases.ubuntu.com/ubuntu|g' \
        -e 's|http://security.ubuntu.com/ubuntu|http://old-releases.ubuntu.com/ubuntu|g' \
        /etc/apt/sources.list
    apt-get update
fi

export DEBIAN_FRONTEND=noninteractive
apt-get install -y --no-install-recommends \
    build-essential gcc g++ make cmake ninja-build \
    python3 python3-dev python3-pip \
    p7zip-full file patchelf ca-certificates \
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxkbcommon-dev libxkbcommon-x11-dev \
    libxcb1-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
    libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev \
    libxcb-xinerama0-dev libxcb-xkb-dev libxcb-sync-dev libxcb-xfixes0-dev \
    libfontconfig1-dev libfreetype6-dev libdbus-1-dev

rm -rf /var/lib/apt/lists/*

echo "==> Ubuntu 18.04 构建环境就绪"
echo "    glibc : $(ldd --version | head -1 | awk '{print $NF}')（要求 ≤ 2.28，麒麟 V10 SP1 上限）"
echo "    gcc   : $(gcc --version | head -1)（GCC7 → 产物 GLIBCXX ≤ 3.4.24，匹配麒麟）"
echo "    cmake : $(cmake --version | head -1 | awk '{print $3}')（低于 3.20 时脚本会用 pip 升级）"
