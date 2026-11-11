#pragma once

// 图元渲染：CadCanvas（正式图元）与 CadDrawController（橡皮筋预览）共用
#include <QPen>
#include <QSizeF>

#include "CadGeometry.h"
#include "CadProtocol.h"
#include "CadViewTransform.h"

class QPainter;

namespace cadpainter {

QColor       colorOf(cad::ColorIndex color);
Qt::PenStyle penStyleOf(cad::LineType type);
QPen         entityPen(const cad::Entity& entity);

// 把单个图元按视图变换绘制到 painter；调用方负责 setBrush(Qt::NoBrush)
void drawEntity(QPainter& painter, const cad::Entity& entity,
                const CadViewTransform& view, const QSizeF& viewport);

}  // namespace cadpainter
