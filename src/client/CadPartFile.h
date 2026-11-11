#pragma once

#include <QString>

#include "CadProtocol.h"

class CadDocument;

// Part2D：本应用原生工程文件格式（JSON），可完整往返保存 / 加载整个文档。
namespace cadfile {

// 图元类型中文名（工件树标签与状态提示复用）
QString entityTypeName(cad::EntityType type);

// 保存整个文档（含 nextId、当前样式、全部图元）到 path；成功返回 true
bool savePart(const QString& path, const CadDocument* doc);

// 从 path 载入并替换 doc 内容（保留原图元 id 与 nextId）；成功返回 true
bool loadPart(const QString& path, CadDocument* doc);

// 单个图元的 JSON 序列化（供 Part2D 文件与服务端 TCP/JSON 协议共用）
QJsonObject entityToJson(const cad::Entity& e);
cad::Entity entityFromJson(const QJsonObject& o);

}  // namespace cadfile
