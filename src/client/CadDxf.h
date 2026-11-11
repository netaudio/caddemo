#pragma once

#include <QString>

class CadDocument;

// DXF（R12 ASCII）互操作：支持常见几何图元的导出 / 导入。
// 坐标约定：本软件世界坐标为 X-Z（Z 向上），DXF 为 X-Y，
// 故导出时 Z -> DXF Y，导入时 DXF Y -> Z，角度方向一致（均逆时针）。
namespace cadfile {

// 导出整个文档为 DXF（R12 文本格式）；成功返回 true
bool exportDxf(const QString& path, const CadDocument* doc);

// 从 DXF 导入并替换 doc 内容；成功返回 true（无法解析时返回 false）
bool importDxf(const QString& path, CadDocument* doc);

}  // namespace cadfile
