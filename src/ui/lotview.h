#pragma once
// ---------------------------------------------------------------------------
// lotview.h — 2D 车位视图（QGraphicsView）
//
// 交互全部由框架提供：命中测试（itemAt）、滚轮缩放、拖拽平移、悬停高亮。
// 视图里没有任何布局逻辑——几何全部来自 pl::makeLayout。
// ---------------------------------------------------------------------------
#include "domain/layout.h"
#include "domain/lot.h"

#include <QGraphicsView>
#include <QHash>
#include <QPoint>
#include <QSet>

namespace ui {

class LotView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit LotView(QWidget* parent = nullptr);

    void setLot(const pl::Lot* lot);
    void rebuild();
    void fitToView();
    void zoomIn();
    void zoomOut();

signals:
    void slotActivated(int slotIndex);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    const pl::Lot* m_lot = nullptr;
    pl::Layout m_layout;
    bool m_fitted = false;
    bool m_needsFit = true;
    bool m_hasBuilt = false;
    QHash<int, QGraphicsItem*> m_items;
    QSet<int> m_prevOccupied;
    int m_layoutCapacity = 0;

    QPoint m_pressPos;
    QPoint m_lastPan;
    bool m_panning = false;
};

} // namespace ui
