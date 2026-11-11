#pragma once

#include <QIcon>
#include <QString>

// 程序绘制的轻量图标，避免外部资源依赖，风格贴近参考图中的 Office 2007 图标
namespace CadIcons {

// key 见实现文件；传入空字符串表示无图标按钮
QIcon icon(const QString& key);

}  // namespace CadIcons
