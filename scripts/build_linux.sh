#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# ScanPro CAD — Linux 构建脚本（麒麟 V10 / openEuler / RHEL8 系）
#
# 与 Windows 的差异：Windows 用 Qt 6.7.3，麒麟侧用 Qt 5（麒麟系统源自带 5.15.2）。
# 由 QT_MAJOR 控制，默认 5。
#
# 用法：
#   ./scripts/build_linux.sh                     # 用系统 Qt5（麒麟源自带 5.15.2）
#   QT_MAJOR=6 ./scripts/build_linux.sh          # 改用下载的 Qt 6.7.3
#   QT_ROOT=/opt/Qt/5.15.2/gcc_64 ./scripts/build_linux.sh
#   QT_ARCH=linux_arm64 QT_MAJOR=6 ./scripts/build_linux.sh
#
# 环境变量：
#   QT_MAJOR       5（默认，麒麟系统 Qt）或 6（下载官方包）
#   QT_ROOT        指定后跳过探测/下载
#   QT_VERSION     Qt6 下载版本，默认 6.7.3
#   QT_ARCH        Qt6 架构，linux_gcc_64 / linux_arm64
#   BUILD_TYPE     默认 Release
#   BUILD_DIR      默认 build/linux-<arch>
#   PREFIX         默认 dist/linux-<arch>
#   SHIP_LIBSTDCXX 1 = 强制随包携带 libstdc++.so.6
#   SHIP_QT        1（默认）= 随包携带 Qt 运行库/插件，目标机零依赖；
#                  0 = 不携带，运行时用目标机系统 Qt。
#                  麒麟 V10 SP1 arm64 内核是 64K 内存页（getconf PAGE_SIZE=65536），
#                  发行版 Qt 库（4K 对齐）在其上无法加载，必须用目标机系统 Qt。
#   PAGE_ALIGN_64K 1 = 可执行文件按 64K 页对齐链接（-z max-page-size=65536）。
#                  64K 对齐产物可同时跑在 64K 页（麒麟 arm64 真机/QEMU 虚拟机）
#                  与 4K 页（x86_64、部分 arm64 系统）环境 —— 一包两用。
#
# 兼容性硬约束（决定产物能否在麒麟上跑起来）：
#   · glibc  —— 构建机必须 ≤ 目标机。麒麟 V10 SP1 多架构统一 glibc 2.28
#               （2026-11 arm64 真机实测修正：并非 20.04 的 2.31）。构建环境
#               选错会报 "GLIBC_2.29 not found"。
#   · libstdc++ —— 麒麟 V10 SP1 自带 GCC 7.3，GLIBCXX 上限 3.4.24；构建机
#               GCC 更新时需 SHIP_LIBSTDCXX=1，或换 GCC7 构建。
#   · 内存页 —— arm64 目标机 64K 页时构建必须 PAGE_ALIGN_64K=1，且不能
#               随包携带 4K 对齐的发行版 Qt 库（SHIP_QT=0）。
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
QT_MAJOR="${QT_MAJOR:-5}"
if [ "${QT_MAJOR}" = "6" ]; then
    QT_VERSION="${QT_VERSION:-6.7.3}"
else
    # 与麒麟桌面版 V10 SP1 系统源中的 Qt 保持一致
    QT_VERSION="${QT_VERSION:-5.15.2}"
fi
QT_ARCH="${QT_ARCH:-linux_gcc_64}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-${SRC_DIR}/build/linux-${QT_ARCH}}"
PREFIX="${PREFIX:-${SRC_DIR}/dist/linux-${QT_ARCH}}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
SHIP_LIBSTDCXX="${SHIP_LIBSTDCXX:-0}"
SHIP_QT="${SHIP_QT:-1}"
PAGE_ALIGN_64K="${PAGE_ALIGN_64K:-0}"

echo "==> 源码目录 : ${SRC_DIR}"
echo "==> 构建目录 : ${BUILD_DIR}"
echo "==> 输出目录 : ${PREFIX}"
echo "==> Qt 主版本: ${QT_MAJOR}"

# ---------------------------------------------------------------------------
# 0) 工具链
#    gcc-toolset（RHEL8 容器里存在）优先入 PATH；麒麟本机通常直接用系统 gcc
# ---------------------------------------------------------------------------
for d in /opt/rh/gcc-toolset-11/root/usr /opt/rh/gcc-toolset-*/root/usr; do
    if [ -x "${d}/bin/gcc" ]; then
        export PATH="${d}/bin:${PATH}"
        GTS_LIBDIR="${d}/lib64"
        echo "==> 工具链 : ${d} (gcc $(gcc -dumpversion))"
        break
    fi
done

pick_python() {
    # 放宽到 3.6：ubuntu:18.04(bionic) 的系统 python3 就是 3.6，
    # 这里主要用于 pip 升级 cmake。需要 3.7+ 的 aqtinstall 调用前
    # 用 ensure_py37 单独检查。
    for c in python3.11 python3.10 python3.9 python3.8 python3; do
        command -v "${c}" >/dev/null 2>&1 || continue
        if "${c}" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)' 2>/dev/null; then
            echo "${c}"
            return 0
        fi
    done
    return 1
}

# aqtinstall 官方要求 Python >= 3.7（bionic 的 3.6 装不上）
ensure_py37() {
    local py="$1"
    [ -n "${py}" ] || return 1
    "${py}" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 7) else 1)' 2>/dev/null
}

cmake_new_enough() {
    command -v cmake >/dev/null 2>&1 || return 1
    local v
    v="$(cmake --version 2>/dev/null | head -1 | awk '{print $3}')"
    [ -n "${v}" ] || return 1
    [ "$(printf '%s\n3.20.0\n' "${v}" | sort -V | head -1)" = "3.20.0" ]
}

# ---------------------------------------------------------------------------
# 1) Qt
# ---------------------------------------------------------------------------
QT_LIB_DIR=""
QT_PLUGIN_DIR=""

if [ "${QT_MAJOR}" = "6" ]; then
    case "${QT_ARCH}" in
        linux_gcc_64) QT_SUBDIR="gcc_64"    ;;
        linux_arm64)  QT_SUBDIR="gcc_arm64" ;;
        *)            QT_SUBDIR="gcc_64"    ;;
    esac

    if [ -z "${QT_ROOT:-}" ]; then
        QT_INSTALL_DIR="${QT_INSTALL_DIR:-/opt/Qt}"
        QT_ROOT="${QT_INSTALL_DIR}/${QT_VERSION}/${QT_SUBDIR}"
        if [ ! -d "${QT_ROOT}" ]; then
            PYTHON="$(pick_python || true)"
            ensure_py37 "${PYTHON}" || { echo "!! aqtinstall 需要 Python >= 3.7" >&2; exit 1; }
            echo "==> 安装 Qt ${QT_VERSION} (${QT_ARCH})"
            "${PYTHON}" -m pip install --quiet --upgrade pip setuptools wheel || true
            "${PYTHON}" -m pip install --quiet "aqtinstall>=2.3.0"
            "${PYTHON}" -m aqt install-qt linux desktop "${QT_VERSION}" "${QT_ARCH}" \
                -O "${QT_INSTALL_DIR}"
        fi
    fi
    [ -d "${QT_ROOT}" ] || { echo "!! 找不到 Qt 目录：${QT_ROOT}" >&2; exit 1; }
    QT_LIB_DIR="${QT_ROOT}/lib"
    QT_PLUGIN_DIR="${QT_ROOT}/plugins"
    echo "==> 使用 Qt6 : ${QT_ROOT}"
else
    # Qt5：优先用系统安装的 Qt（麒麟源 qtbase5-dev = 5.15.2，或发行版包）
    pick_qmake5() {
        for q in qmake-qt5 qmake5 qmake; do
            command -v "${q}" >/dev/null 2>&1 || continue
            case "$("${q}" -query QT_VERSION 2>/dev/null)" in
                5.*) echo "${q}"; return 0 ;;
            esac
        done
        return 1
    }

    if [ -z "${QT_ROOT:-}" ]; then
        QMAKE="$(pick_qmake5 || true)"
        if [ -n "${QMAKE}" ]; then
            QT_ROOT="$("${QMAKE}" -query QT_INSTALL_PREFIX)"
            QT_LIB_DIR="$("${QMAKE}" -query QT_INSTALL_LIBS)"
            QT_PLUGIN_DIR="$("${QMAKE}" -query QT_INSTALL_PLUGINS)"
            echo "==> 使用系统 Qt5 : ${QT_ROOT} ($("${QMAKE}" -query QT_VERSION))"
        else
            # 没有系统 Qt5：下载官方包，版本与麒麟系统源一致（5.15.2）
            QT_INSTALL_DIR="${QT_INSTALL_DIR:-/opt/Qt}"
            QT_ROOT="${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64"
            if [ ! -d "${QT_ROOT}" ]; then
                PYTHON="$(pick_python || true)"
                ensure_py37 "${PYTHON}" || { echo "!! aqtinstall 需要 Python >= 3.7" >&2; exit 1; }
                echo "==> 下载 Qt ${QT_VERSION}（与麒麟系统源同版本）"
                "${PYTHON}" -m pip install --quiet --upgrade pip setuptools wheel || true
                "${PYTHON}" -m pip install --quiet "aqtinstall>=2.3.0"
                "${PYTHON}" -m aqt install-qt linux desktop "${QT_VERSION}" gcc_64 \
                    -O "${QT_INSTALL_DIR}"
            fi
            QT_LIB_DIR="${QT_ROOT}/lib"
            QT_PLUGIN_DIR="${QT_ROOT}/plugins"
            echo "==> 使用 Qt5（官方包）: ${QT_ROOT}"
        fi
    else
        echo "==> 使用 Qt5 : ${QT_ROOT}"
        QT_LIB_DIR="${QT_ROOT}/lib"
        QT_PLUGIN_DIR="${QT_ROOT}/plugins"
    fi
fi

# CMake 版本（Qt6 的 CMake 包需要 3.16+，本项目要求 3.20+）
# bionic 的 apt cmake 只有 3.10，必须走 pip；pip 也先升到 py3.6 支持的最高版，
# 否则老 pip（9.x）对新版 PyPI 的 TLS/元数据支持不全
if ! cmake_new_enough; then
    PYTHON="$(pick_python || true)"
    if [ -n "${PYTHON}" ]; then
        echo "==> CMake 版本不足 3.20，通过 pip 安装"
        "${PYTHON}" -m pip install --quiet --upgrade pip setuptools wheel || true
        "${PYTHON}" -m pip install --quiet --upgrade "cmake>=3.20,<4"
    else
        echo "!! CMake 版本低于 3.20 且无法自动升级" >&2
        exit 1
    fi
fi

# ---------------------------------------------------------------------------
# 2) 构建
# ---------------------------------------------------------------------------
GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS=(-G Ninja)
fi

# 64K 页对齐：麒麟 V10 SP1 arm64 内核页 64K，LOAD 段对齐必须 ≥ 页大小。
# 64K 对齐的产物在 4K 页环境同样合法 —— 一包两用。
EXE_LINKER_FLAGS=""
if [ "${PAGE_ALIGN_64K}" = "1" ]; then
    EXE_LINKER_FLAGS="-Wl,-z,max-page-size=65536 -Wl,-z,common-page-size=65536"
    echo "==> 64K 页对齐链接 : ${EXE_LINKER_FLAGS}"
fi

cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" \
    "${GENERATOR_ARGS[@]}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DQT_MAJOR="${QT_MAJOR}" \
    -DQT_ROOT="${QT_ROOT:-}" \
    -DCMAKE_EXE_LINKER_FLAGS="${EXE_LINKER_FLAGS}" \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}"

cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
cmake --install "${BUILD_DIR}"

# ---------------------------------------------------------------------------
# 3) 收集运行库与插件
# ---------------------------------------------------------------------------
BIN_DIR="${PREFIX}/bin"

if [ "${SHIP_QT}" = "1" ]; then
    echo "==> 收集 Qt 运行库 -> ${BIN_DIR}"
    if [ -n "${QT_LIB_DIR}" ] && [ -d "${QT_LIB_DIR}" ]; then
        # 1) ldd 找直接依赖的 Qt 库（只拷落在 Qt 目录内的）
        ldd "${BIN_DIR}/caddemo" 2>/dev/null | awk '/=>/ {print $3}' | while read -r so; do
            case "${so}" in
                "${QT_LIB_DIR}"/*) cp -Lf "${so}" "${BIN_DIR}/" 2>/dev/null || true ;;
            esac
        done

        # 2) 兜底按文件名拷（xcb / icu 这类间接依赖 ldd 未必覆盖全）
        if [ "${QT_MAJOR}" = "6" ]; then
            PATS="libQt6Core.so.6 libQt6Gui.so.6 libQt6Widgets.so.6 libQt6DBus.so.6
                  libQt6XcbQpa.so.6 libicui18n.so.* libicuuc.so.* libicudata.so.*"
        else
            PATS="libQt5Core.so.5 libQt5Gui.so.5 libQt5Widgets.so.5 libQt5DBus.so.5
                  libQt5XcbQpa.so.5 libicui18n.so.* libicuuc.so.* libicudata.so.*"
        fi
        for pat in ${PATS}; do
            find "${QT_LIB_DIR}" -maxdepth 1 -name "${pat}" -exec cp -Lf {} "${BIN_DIR}/" \; 2>/dev/null || true
        done
    fi

    # 平台插件
    mkdir -p "${BIN_DIR}/platforms" "${BIN_DIR}/imageformats" "${BIN_DIR}/iconengines"
    cp -Lf "${QT_PLUGIN_DIR}"/platforms/libqxcb.so       "${BIN_DIR}/platforms/" 2>/dev/null || true
    cp -Lf "${QT_PLUGIN_DIR}"/platforms/libqoffscreen.so "${BIN_DIR}/platforms/" 2>/dev/null || true
    cp -Lf "${QT_PLUGIN_DIR}"/imageformats/*.so          "${BIN_DIR}/imageformats/" 2>/dev/null || true
    cp -Lf "${QT_PLUGIN_DIR}"/iconengines/*.so           "${BIN_DIR}/iconengines/"  2>/dev/null || true

    # RPATH：让可执行文件与各 .so 从自身目录找依赖。
    # 用 --force-rpath 生成 DT_RPATH（DT_RUNPATH 不作用于间接依赖）。
    if command -v patchelf >/dev/null 2>&1; then
        patchelf --force-rpath --set-rpath '$ORIGIN' "${BIN_DIR}/caddemo" 2>/dev/null \
            && echo "==> RPATH(DT_RPATH)=\$ORIGIN 已写入 caddemo"
        for so in "${BIN_DIR}"/*.so.*; do
            [ -e "${so}" ] && patchelf --set-rpath '$ORIGIN' "${so}" 2>/dev/null || true
        done
        for so in "${BIN_DIR}"/platforms/*.so "${BIN_DIR}"/imageformats/*.so \
                  "${BIN_DIR}"/iconengines/*.so; do
            [ -e "${so}" ] && patchelf --set-rpath '$ORIGIN/..' "${so}" 2>/dev/null || true
        done
    fi
else
    echo "==> SHIP_QT=0：不随包携带 Qt/插件，运行时使用目标机系统 Qt"
    echo "    （麒麟 V10 SP1 arm64 为 64K 内存页，发行版 4K 对齐 Qt 库无法加载）"
fi

# libstdc++：目标机 GLIBCXX 较老时随包携带（麒麟服务器版只到 3.4.24）
if [ "${SHIP_LIBSTDCXX}" = "1" ]; then
    if [ -z "${GTS_LIBDIR:-}" ] || [ ! -e "${GTS_LIBDIR}/libstdc++.so.6" ]; then
        GTS_LIBDIR="$(dirname "$(g++ -print-file-name=libstdc++.so.6 2>/dev/null \
            || echo /usr/lib64/libstdc++.so.6)")"
    fi
    if [ -e "${GTS_LIBDIR}/libstdc++.so.6" ]; then
        cp -Lf "${GTS_LIBDIR}"/libstdc++.so.6* "${BIN_DIR}/" 2>/dev/null || true
        echo "==> 已随包携带 libstdc++ : ${GTS_LIBDIR}/libstdc++.so.6"
    fi
fi

# 兜底启动脚本：SHIP_QT=1 时把 bin/ 放进搜索路径（自带的 Qt/插件）；
# SHIP_QT=0 时 bin/ 里没有库，直接执行，全部依赖走目标机系统路径
if [ "${SHIP_QT}" = "1" ]; then
    cat > "${BIN_DIR}/run-caddemo.sh" <<'LAUNCHER'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${HERE}:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}:${QT_PLUGIN_PATH:-}"
exec "${HERE}/caddemo" "$@"
LAUNCHER
else
    cat > "${BIN_DIR}/run-caddemo.sh" <<'LAUNCHER'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${HERE}/caddemo" "$@"
LAUNCHER
fi
chmod +x "${BIN_DIR}/run-caddemo.sh" "${BIN_DIR}/caddemo" 2>/dev/null || true

# ---------------------------------------------------------------------------
# 4) 打包
# ---------------------------------------------------------------------------
ARCHIVE="${PREFIX}.tar.gz"
echo "==> 打包 -> ${ARCHIVE}"
tar -czf "${ARCHIVE}" -C "${PREFIX}" .

# ---------------------------------------------------------------------------
# 5) 兼容性报告
# ---------------------------------------------------------------------------
report_syms() {
    local f="$1"
    [ -e "${f}" ] || return 0
    local g c
    g="$(objdump -T "${f}" 2>/dev/null | grep -o 'GLIBC_[0-9.]*'   | sort -uV | tail -1)"
    c="$(objdump -T "${f}" 2>/dev/null | grep -o 'GLIBCXX_[0-9.]*' | sort -uV | tail -1)"
    printf '    %-26s GLIBC %-10s GLIBCXX %s\n' "$(basename "${f}")" "${g:-none}" "${c:-none}"
}

echo ""
echo "==> 完成"
echo "    可执行文件 : ${BIN_DIR}/caddemo"
echo "    发布包     : ${ARCHIVE}"
echo ""
echo "==> 符号版本（对照：麒麟 V10 SP1 多架构 = GLIBC 2.28 / GLIBCXX 3.4.24）"
report_syms "${BIN_DIR}/caddemo"
if [ "${SHIP_QT}" = "1" ]; then
    if [ "${QT_MAJOR}" = "6" ]; then
        report_syms "${BIN_DIR}/libQt6Core.so.6"
    else
        report_syms "${BIN_DIR}/libQt5Core.so.5"
    fi
    report_syms "${BIN_DIR}/libstdc++.so.6"
fi
if [ "${PAGE_ALIGN_64K}" = "1" ]; then
    echo "==> LOAD 段对齐（readelf -lW）："
    readelf -lW "${BIN_DIR}/caddemo" 2>/dev/null | awk '$1=="LOAD" {print "    LOAD Align = " $NF}'
fi
