#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# 在 Ubuntu 20.04（focal）容器内准备麒麟桌面版的构建环境
#
# 为什么是 ubuntu:20.04：
#   银河麒麟桌面操作系统 V10 SP1 基于 Ubuntu 20.04 衍生，
#   glibc 2.31、libstdc++ 到 GLIBCXX_3.4.28 —— 与 focal 完全一致。
#   在与目标机同 glibc 的环境里编译，产物不会出现 "GLIBC_2.34 not found"。
#   （若错用 ubuntu-22.04，glibc 是 2.35，产物到麒麟上直接起不来。）
# ---------------------------------------------------------------------------
set -euo pipefail

# focal 已过标准支持期，主源偶有下线；失败时回退到 old-releases
if ! apt-get update; then
    echo "==> 主源不可用，切换 old-releases.ubuntu.com"
    sed -i \
        -e 's|http://archive.ubuntu.com/ubuntu|http://old-releases.ubuntu.com/ubuntu|g' \
        -e 's|http://security.ubuntu.com/ubuntu|http://old-releases.ubuntu.com/ubuntu|g' \
        /etc/apt/sources.list
    apt-get update
fi

export DEBIAN_FRONTEND=noninteractive
# --allow-downgrades：Debian bullseye 已 EOL，其镜像内预装的包（如 libc6 u14）
# 可能高于当前可用源中的版本；与 libc6-dev/perl 这类严格等版本依赖并存时，
# apt 会判定依赖冲突。允许降级即可把版本统一到源里，Ubuntu 上不会触发降级。
apt-get install -y --no-install-recommends --allow-downgrades \
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

echo "==> Ubuntu 20.04 构建环境就绪"
echo "    glibc : $(ldd --version | head -1 | awk '{print $NF}')"
echo "    gcc   : $(gcc --version | head -1)"
echo "    cmake : $(cmake --version | head -1 | awk '{print $3}')（低于 3.20 时脚本会用 pip 升级）"
