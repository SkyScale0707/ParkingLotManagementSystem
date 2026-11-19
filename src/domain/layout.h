#pragma once
// ---------------------------------------------------------------------------
// layout.h — 2D 车位几何（纯计算，视图只负责把矩形画出来）
//
// 算法与原型一致：cols = clamp(round(sqrt(capacity × 1.6)), 4, 30)，
// 每 aisleEvery 行插入一条 aislePx 宽的行车道。
// ---------------------------------------------------------------------------
#include <QRect>
#include <QVector>

namespace pl {

struct SlotGeom {
    int index = 0;
    int row = 0;
    int col = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    QRect rect() const { return QRect(x, y, w, h); }
};

struct Layout {
    int rows = 0;
    int cols = 0;
    int aisleEvery = 2;
    int aislePx = 22;
    int margin = 12;
    int slotW = 26;
    int slotH = 52;
    int gapX = 6;
    int gapY = 8;
    QVector<SlotGeom> slotRects;   // 成员名避开 Qt 的 slots 宏

    int widthPx() const;
    int heightPx() const;
};

Layout makeLayout(int capacity);

} // namespace pl
