#include "domain/layout.h"

#include <algorithm>
#include <cmath>

namespace pl {

int Layout::widthPx() const
{
    if (cols <= 0)
        return margin * 2;
    return margin * 2 + cols * (slotW + gapX) - gapX;
}

int Layout::heightPx() const
{
    if (rows <= 0)
        return margin * 2;
    const int aisles = (rows - 1) / std::max(1, aisleEvery);
    return margin * 2 + rows * (slotH + gapY) - gapY + aisles * aislePx;
}

Layout makeLayout(int capacity)
{
    Layout L;
    if (capacity < 1)
        capacity = 1;

    // 目标宽高比 ≈ 1.6（横向布局），列数限制在 4..30 以保证可读性
    int cols = static_cast<int>(std::lround(std::sqrt(static_cast<double>(capacity) * 1.6)));
    cols = std::clamp(cols, 4, 30);
    cols = std::min(cols, capacity);

    const int rows = (capacity + cols - 1) / cols;
    L.cols = cols;
    L.rows = rows;

    L.slotRects.reserve(capacity);
    for (int i = 0; i < capacity; ++i) {
        SlotGeom g;
        g.index = i;
        g.row = i / cols;
        g.col = i % cols;
        const int aisles = g.row / L.aisleEvery;
        g.x = L.margin + g.col * (L.slotW + L.gapX);
        g.y = L.margin + g.row * (L.slotH + L.gapY) + aisles * L.aislePx;
        g.w = L.slotW;
        g.h = L.slotH;
        L.slotRects.append(g);
    }
    return L;
}

} // namespace pl
