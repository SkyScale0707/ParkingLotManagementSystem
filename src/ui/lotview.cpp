#include "ui/lotview.h"

#include "ui/theme.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <cmath>

namespace ui {
namespace {

class SlotItem : public QGraphicsItem
{
public:
    SlotItem(int index, const QRectF& rect)
        : m_index(index)
        , m_rect(rect)
    {
        setAcceptHoverEvents(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("车位 %1").arg(index + 1));
    }

    int index() const { return m_index; }

    void setOccupied(bool occupied, pl::VehicleType type, const QString& plate)
    {
        m_occupied = occupied;
        m_type = type;
        m_plate = plate;
        setToolTip(occupied ? QStringLiteral("P%1 · %2 · %3")
                                  .arg(m_index + 1, 3, 10, QLatin1Char('0'))
                                  .arg(plate, pl::typeLabel(type))
                            : QStringLiteral("P%1 · 空闲")
                                  .arg(m_index + 1, 3, 10, QLatin1Char('0')));
        update();
    }

    QRectF boundingRect() const override { return m_rect.adjusted(-1.5, -1.5, 1.5, 1.5); }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QColor fill = m_occupied ? theme::typeSoftColor(m_type) : QColor(0xF1, 0xF5, 0xF9);
        QColor edge = m_occupied ? theme::typeColor(m_type) : QColor(0xCB, 0xD5, 0xE1);
        if (m_hover)
            edge = theme::accent();

        painter->setPen(QPen(edge, m_hover ? 2.0 : 1.0));
        painter->setBrush(fill);
        painter->drawRoundedRect(m_rect, 4, 4);

        const qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
        if (lod < 0.45)
            return;

        QFont font = painter->font();
        if (m_occupied) {
            font.setPointSizeF(7.5);
            font.setBold(true);
            painter->setFont(font);
            painter->setPen(theme::typeColor(m_type).darker(115));
            painter->drawText(m_rect.adjusted(1, 4, -1, 0), Qt::AlignHCenter | Qt::AlignTop,
                              pl::typeShortLabel(m_type));

            font.setPointSizeF(5.6);
            font.setBold(false);
            painter->setFont(font);
            painter->setPen(QColor(0x64, 0x74, 0x8B));
            painter->drawText(m_rect.adjusted(1, 0, -1, -4), Qt::AlignHCenter | Qt::AlignBottom,
                              QStringLiteral("P%1").arg(m_index + 1, 3, 10, QLatin1Char('0')));
        } else if (lod >= 0.7) {
            font.setPointSizeF(5.6);
            painter->setFont(font);
            painter->setPen(QColor(0x94, 0xA3, 0xB8));
            painter->drawText(m_rect.adjusted(1, 0, -1, -4), Qt::AlignHCenter | Qt::AlignBottom,
                              QStringLiteral("P%1").arg(m_index + 1, 3, 10, QLatin1Char('0')));
        }
    }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
        m_hover = true;
        update();
        QGraphicsItem::hoverEnterEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
    {
        m_hover = false;
        update();
        QGraphicsItem::hoverLeaveEvent(event);
    }

private:
    int m_index = 0;
    QRectF m_rect;
    bool m_occupied = false;
    pl::VehicleType m_type = pl::VehicleType::CarSmall;
    QString m_plate;
    bool m_hover = false;
};

} // namespace

LotView::LotView(QWidget* parent)
    : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(0xF8, 0xFA, 0xFC));
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMinimumHeight(300);
    viewport()->setCursor(Qt::ArrowCursor);
}

void LotView::setLot(const pl::Lot* lot)
{
    m_lot = lot;
    if (m_lot && m_lot->cfg.capacity != m_layoutCapacity)
        m_fitted = false;
    rebuild();
    if (!m_fitted) {
        m_needsFit = true;
        if (isVisible()) {
            fitToView();
            m_needsFit = false;
        }
    }
}

void LotView::rebuild()
{
    QGraphicsScene* s = scene();
    if (!s)
        return;

    s->clear();
    m_items.clear();

    m_layout = pl::makeLayout(m_lot ? m_lot->cfg.capacity : 1);
    m_layoutCapacity = m_layout.slotRects.size() > 0 ? static_cast<int>(m_layout.slotRects.size()) : 0;
    s->setSceneRect(0, 0, m_layout.widthPx(), m_layout.heightPx());

    if (!m_lot)
        return;

    QSet<int> occupiedNow;
    for (const pl::SlotGeom& g : m_layout.slotRects) {
        auto* item = new SlotItem(g.index, QRectF(g.rect()));
        bool occupied = false;
        if (g.index < m_lot->slotPlate.size()) {
            const QString plate = m_lot->slotPlate.at(g.index);
            if (!plate.isEmpty()) {
                const auto it = m_lot->active.constFind(plate);
                if (it != m_lot->active.constEnd()) {
                    item->setOccupied(true, it->type, plate);
                    occupied = true;
                }
            }
        }
        s->addItem(item);
        m_items.insert(g.index, item);
        if (occupied)
            occupiedNow.insert(g.index);

        // 入/出场动画：只有状态发生变化的车位做一次淡入
        const bool changed = m_hasBuilt && (m_prevOccupied.contains(g.index) != occupied);
        if (changed) {
            item->setOpacity(0.0);
            auto* anim = new QVariantAnimation(this);
            anim->setStartValue(0.0);
            anim->setEndValue(1.0);
            anim->setDuration(240);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &QVariantAnimation::valueChanged, this, [this, index = g.index](const QVariant& v) {
                if (QGraphicsItem* target = m_items.value(index, nullptr))
                    target->setOpacity(v.toDouble());
            });
            connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);
            anim->start();
        }
    }

    m_prevOccupied = occupiedNow;
    m_hasBuilt = true;
}

void LotView::fitToView()
{
    QGraphicsScene* s = scene();
    if (!s)
        return;
    const QRectF r = s->sceneRect();
    if (r.isEmpty())
        return;

    resetTransform();
    fitInView(r.adjusted(-6, -6, 6, 6), Qt::KeepAspectRatio);

    // 小规模停车场不要把车位放大到失真
    if (transform().m11() > 2.2) {
        resetTransform();
        scale(2.2, 2.2);
        centerOn(r.center());
    }
    m_fitted = true;
}

void LotView::showEvent(QShowEvent* event)
{
    QGraphicsView::showEvent(event);
    if (m_needsFit) {
        fitToView();
        m_needsFit = false;
    }
}

void LotView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (m_needsFit) {
        fitToView();
        m_needsFit = false;
    }
}

void LotView::zoomIn()
{
    if (transform().m11() < 3.2)
        scale(1.2, 1.2);
}

void LotView::zoomOut()
{
    if (transform().m11() > 0.3)
        scale(1.0 / 1.2, 1.0 / 1.2);
}

void LotView::wheelEvent(QWheelEvent* event)
{
    const qreal factor = std::pow(1.0015, static_cast<double>(event->angleDelta().y()));
    const qreal target = transform().m11() * factor;
    if (target >= 0.25 && target <= 4.0)
        scale(factor, factor);
    event->accept();
}

void LotView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->pos();
        m_lastPan = event->pos();
        m_panning = false;
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void LotView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        if (!m_panning && (event->pos() - m_pressPos).manhattanLength() > 4) {
            m_panning = true;
            viewport()->setCursor(Qt::ClosedHandCursor);
        }
        if (m_panning) {
            const QPoint delta = event->pos() - m_lastPan;
            m_lastPan = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void LotView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!m_panning) {
            if (QGraphicsItem* item = itemAt(event->pos())) {
                if (auto* slot = dynamic_cast<SlotItem*>(item))
                    emit slotActivated(slot->index());
            }
        }
        m_panning = false;
        viewport()->setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

} // namespace ui
