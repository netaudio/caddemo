#pragma once

// 状态条键盘输入解析（阶段 2）
// 支持 AutoCAD 风格：
//   x=10,y=20 / x=10,z=20   绝对坐标
//   dx=30,dy=10             相对上一点
//   L=30,A=45               长度 + 角度（度）
//   10,20                   绝对坐标简写
//   @30,10                  相对坐标简写
//   R=50 / N=5 / D=8 / A=90 / H=6   数值参数
//   T=文字 / 任意普通文字    文本内容
#include <QString>

#include "CadProtocol.h"

namespace cadinput {

enum class Kind { None, Point, Number, Text };

struct Result {
    Kind        kind = Kind::None;
    cad::Vec2   point;
    double      number = 0.0;
    QString     text;
    QString     description;   // 回显用的人类可读描述
};

// cursor 为鼠标当前世界坐标（解析失败时可回退），base 为上一个确认点
Result parse(const QString& input, const cad::Vec2& cursor, const cad::Vec2& base);

}  // namespace cadinput
