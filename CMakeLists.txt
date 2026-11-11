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

# Qt 6.7.3 (msvc2019_64)。可通过命令行 -DQT_ROOT=... 覆盖
if(NOT DEFINED QT_ROOT)
    set(QT_ROOT "C:/Qt/6.7.3/msvc2019_64" CACHE PATH "Qt 6.7.3 安装根目录")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets)

# ---------------------------------------------------------------------------
# 客户端目标（阶段 2：绘图命令状态机）
# 目录按 C/S 架构分层：
#   src/client/  —— 客户端界面、图元文档、渲染、命令状态机与交互
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
    src/client/CadIcons.cpp
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
    src/client/CadIcons.h
    src/common/CadProtocol.h
    src/common/CadGeometry.h
)

qt_add_executable(caddemo WIN32
    ${CADDEMO_SOURCES}
    ${CADDEMO_HEADERS}
)

target_include_directories(caddemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(caddemo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
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
