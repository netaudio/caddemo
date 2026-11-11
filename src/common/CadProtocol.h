#pragma once

// ---------------------------------------------------------------------------
// C/S 架构共享协议定义（阶段 1：图元模型与文档数据）
//
//   客户端 (src/client) : 界面 / 交互 / 本地渲染与预览
//   服务端 (src/server) : 后续阶段实现，负责图元存储、DXF 解析、扫查计划计算
//   双方通过下面的命令请求 / 响应结构交换数据，便于直接序列化为 JSON/TCP 包
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>

namespace cad {

// 二维坐标：软件默认使用 X-Z 平面，原点为圆盘中心，Z 沿中轴线向上
struct Vec2 {
    double x = 0.0;
    double z = 0.0;
};

enum class EntityType : std::int32_t {
    Point       = 1,
    Line        = 2,
    Circle      = 3,
    Arc         = 4,
    Curve       = 5,
    Rectangle   = 6,
    Concentric  = 7,
    Text        = 8,
    Dimension   = 9,
};

// 「当前属性」中的颜色，与 Ribbon 下拉框一一对应
enum class ColorIndex : std::int32_t {
    Black  = 0,
    Red    = 1,
    Green  = 2,
    Blue   = 3,
    Yellow = 4,
    White  = 5,
};

// 「当前属性」中的线型
enum class LineType : std::int32_t {
    Solid      = 0,
    Dashed     = 1,
    DashDot    = 2,
    DashDotDot = 3,
};

// 图元：与界面上的「绘图」工具一一对应
struct Entity {
    std::int64_t        id = 0;
    EntityType          type = EntityType::Line;
    std::vector<Vec2>   points;       // 端点 / 圆心 / 控制点 / 矩形对角点
    double              radius = 0.0; // 圆 / 圆弧 / 同心圆外圈半径
    double              innerRadius = 0.0; // 同心圆内圈半径（0 = 取外圈半径的一半）
    double              startAngle = 0.0;  // 圆弧起始角（度，数学正向）
    // 圆弧终止角（度）；endAngle - startAngle 为带符号 span：
    // 正值逆时针、负值顺时针，360 表示整圆
    double              endAngle = 0.0;
    double              textHeight = 5.0;
    std::string         text;
    std::string         layer = "0";
    ColorIndex          color = ColorIndex::Black;
    LineType            lineType = LineType::Solid;
    double              lineWidth = 0.0;  // 0 = 使用默认细线
};

// 命令请求：例如客户端发出 "line" + 两个点，服务端校验后返回图元 id
struct CommandRequest {
    std::string         command;    // line / circle / trim / ...
    std::string         sessionId;
    std::vector<Vec2>   args;
    std::vector<std::int64_t> selection;   // 先拾取对象再执行命令
    std::string         keyboardInput;     // 状态条键盘输入，如 "x=*,y=*"
};

struct CommandReply {
    bool                ok = false;
    std::string         message;
    std::vector<Entity> entities;
};

}  // namespace cad
