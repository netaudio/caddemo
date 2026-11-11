cmake_minimum_required(VERSION 3.20)

project(CadDemo
    VERSION 0.1.0
    DESCRIPTION "ScanPro CAD 绘图模块 - C/S 架构客户端 UI"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# 全局设置
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ---------------------------------------------------------------------------
# Qt 版本策略：Windows 与麒麟刻意使用不同的 Qt
#
#   Windows —— Qt 6.7.3（msvc2019_64）
#              开发环境既定版本，界面与各绘图/编辑命令均在此验证。
#
#   麒麟 V10 —— Qt 5.15.2（LTS）
#              麒麟系统源自带、由麒麟官方适配维护的版本（qmake 3.1）。
#              不用 Qt 6.7 的原因：Qt 6.7 官方 Linux 包需要 GLIBCXX_3.4.28，
#              而麒麟 V10 自带 GCC 7.3 的 libstdc++ 只到 3.4.24，直接运行会报
#              "GLIBCXX_3.4.28 not found"；麒麟源里也没有 Qt6 包。
#
# 可用 -DQT_MAJOR=6 / -DQT_MAJOR=5 显式覆盖。
# 源码已做双版本兼容（见 CadInputParser.cpp 中 SkipEmptyParts 的处理）。
# ---------------------------------------------------------------------------
if(NOT DEFINED QT_MAJOR)
    if(WIN32)
        set(QT_MAJOR 6 CACHE STRING "Qt 主版本：6=Windows 开发环境，5=麒麟系统 Qt")
    else()
        set(QT_MAJOR 5 CACHE STRING "Qt 主版本：6=Windows 开发环境，5=麒麟系统 Qt")
    endif()
endif()
message(STATUS "CadDemo: 使用 Qt${QT_MAJOR}")

# Qt 安装根目录：仅 Windows + Qt6 需要显式指定；Linux 走系统路径由 find_package 自行解析
if(NOT DEFINED QT_ROOT AND WIN32 AND QT_MAJOR EQUAL 6)
    set(QT_ROOT "C:/Qt/6.7.3/msvc2019_64" CACHE PATH "Qt 安装根目录")
endif()
if(DEFINED QT_ROOT AND NOT "${QT_ROOT}" STREQUAL "")
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

if(QT_MAJOR EQUAL 6)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
else()
    # 5.12 是下限：Ubuntu 20.04 仓库即为此版本，麒麟源自带 5.15.2
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets)
endif()

# ---------------------------------------------------------------------------
# 客户端目标（阶段 4：DXF/Part2D 文件读写 + 右侧工件树联动）
# 目录按 C/S 架构分层：
#   src/client/  —— 客户端界面、图元文档、渲染、绘图与编辑命令状态机、交互
#   src/common/  —— 客户端/服务端共享的命令、图元模型与几何工具
#   src/server/  —— 服务端（后续阶段实现）
# ---------------------------------------------------------------------------
set(CADDEMO_SOURCES
    src/main.cpp
    src/client/MainWindow.cpp
    src/client/RibbonBar.cpp
    src/client/CadCanvas.cpp
    src/client/CadDocument.cpp
    src/client/CadEntityPainter.cpp
    src/client/CadCommands.cpp
    src/client/CadInputParser.cpp
    src/client/CadDrawController.cpp
    src/client/CadEditController.cpp
    src/client/CadIcons.cpp
    src/client/CadPartFile.cpp
    src/client/CadDxf.cpp
    src/client/CadEntityTree.cpp
)

set(CADDEMO_HEADERS
    src/client/MainWindow.h
    src/client/RibbonBar.h
    src/client/CadCanvas.h
    src/client/CadDocument.h
    src/client/CadViewTransform.h
    src/client/CadEntityPainter.h
    src/client/CadCommands.h
    src/client/CadInputParser.h
    src/client/CadDrawController.h
    src/client/CadEditController.h
    src/client/CadIcons.h
    src/client/CadPartFile.h
    src/client/CadDxf.h
    src/client/CadEntityTree.h
    src/common/CadProtocol.h
    src/common/CadGeometry.h
)

# qt_add_executable 是 Qt6 专有命令；Qt5 用普通 add_executable，
# moc/uic 由上面开启的 CMAKE_AUTOMOC / CMAKE_AUTOUIC 处理。
if(QT_MAJOR EQUAL 6)
    qt_add_executable(caddemo WIN32
        ${CADDEMO_SOURCES}
        ${CADDEMO_HEADERS}
    )
else()
    add_executable(caddemo WIN32
        ${CADDEMO_SOURCES}
        ${CADDEMO_HEADERS}
    )
endif()

target_include_directories(caddemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(caddemo PRIVATE
    Qt${QT_MAJOR}::Core
    Qt${QT_MAJOR}::Gui
    Qt${QT_MAJOR}::Widgets
)

if(MSVC)
    # 源码统一 UTF-8（含中文界面文字）
    target_compile_options(caddemo PRIVATE /utf-8 /W3 /EHsc /permissive-)
    target_compile_definitions(caddemo PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(caddemo PRIVATE -Wall -Wextra)
endif()

# 构建完成后自动部署 Qt 运行库，保证生成的 exe 可直接双击运行
find_program(WINDEPLOYQT_EXE NAMES windeployqt PATHS "${QT_ROOT}/bin" NO_DEFAULT_PATH)
if(WINDEPLOYQT_EXE AND WIN32)
    add_custom_command(TARGET caddemo POST_BUILD
        COMMAND "${WINDEPLOYQT_EXE}" --no-translations --no-system-d3d-compiler
                --no-opengl-sw "$<TARGET_FILE:caddemo>"
        COMMENT "Deploying Qt runtime libraries..."
        VERBATIM)
endif()

# ---------------------------------------------------------------------------
# 安装规则（Linux / 麒麟等信创环境出包用；Windows 仍由 windeployqt 处理）
#   cmake --install build/linux --prefix <dir>   →  <dir>/bin/caddemo
# ---------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS caddemo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # 附带示例文件，方便在目标机上验证文件读写链路
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/preview/"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/caddemo/samples
            OPTIONAL
            FILES_MATCHING PATTERN "*.dxf" PATTERN "*.part2d")

    # 把 Qt 依赖清单导出，出包脚本据此收集 .so 与插件
    install(CODE "
        file(WRITE \"\${CMAKE_INSTALL_PREFIX}/qt-deps.txt\" \"Qt${QT_MAJOR} ${QT_ROOT}\\n\")
    ")
endif()
