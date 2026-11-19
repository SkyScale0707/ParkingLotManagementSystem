#include "ui/vehiclelistmodel.h"

#include "domain/billing.h"

#include <QBrush>
#include <QColor>

namespace ui {

VehicleListModel::VehicleListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void VehicleListModel::setLot(const pl::Lot* lot, qint64 now)
{
    beginResetModel();
    m_lot = lot;
    m_now = now;
    m_rows.clear();

    if (m_lot) {
        for (const pl::ActiveEntry& e : m_lot->parkedBySlot()) {
            VehicleRow r;
            r.plate = e.plate;
            r.type = e.type;
            r.slot = e.slot;
            r.waiting = false;
            r.sinceMs = e.enteredAtMs;
            m_rows.append(r);
        }
        int pos = 1;
        for (const pl::WaitEntry& w : m_lot->waiting) {
            VehicleRow r;
            r.plate = w.plate;
            r.type = w.type;
            r.slot = -1;
            r.waiting = true;
            r.waitPos = pos++;
            r.sinceMs = w.queuedAtMs;
            m_rows.append(r);
        }
    }

    updateDurations();
    endResetModel();
}

void VehicleListModel::refresh(qint64 now)
{
    if (!m_lot)
        return;

    const int expected = m_lot->occupyCount() + static_cast<int>(m_lot->waiting.size());
    if (expected != m_rows.size()) {
        setLot(m_lot, now);
        return;
    }

    m_now = now;
    updateDurations();
    for (int i = 0; i < m_rows.size(); ++i) {
        const QModelIndex left = index(i, Duration);
        const QModelIndex right = index(i, Fee);
        emit dataChanged(left, right, {Qt::DisplayRole});
    }
}

void VehicleListModel::updateDurations()
{
    for (VehicleRow& r : m_rows) {
        r.durationMs = qMax<qint64>(0, m_now - r.sinceMs);
        r.estFeeCents = r.waiting ? -1 : pl::feeCentsFor(r.sinceMs, m_now, m_lot->cfg);
    }
}

int VehicleListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int VehicleListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant VehicleListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const VehicleRow& r = m_rows.at(index.row());
    const int column = index.column();

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case Plate:    return r.plate;
        case Type:     return pl::typeLabel(r.type);
        case Slot:     return r.waiting ? QStringLiteral("—")
                                        : QStringLiteral("P%1").arg(r.slot + 1, 3, 10, QLatin1Char('0'));
        case State:
            return r.waiting ? QStringLiteral("排队 #%1").arg(r.waitPos) : QStringLiteral("在场");
        case Since:    return pl::formatDateTime(r.sinceMs);
        case Duration: return pl::formatDuration(r.durationMs);
        case Fee:
            return r.waiting ? QStringLiteral("—") : pl::formatMoney(r.estFeeCents);
        default:       return {};
        }

    case Qt::TextAlignmentRole: {
        const int align = (column == Plate) ? Qt::AlignLeft : Qt::AlignHCenter;
        return static_cast<int>(align | Qt::AlignVCenter);
    }

    case Qt::ForegroundRole:
        if (r.waiting && column == State)
            return QBrush(QColor(0xB4, 0x53, 0x09));
        if (column == Plate)
            return QBrush(QColor(0x0F, 0x17, 0x2A));
        return {};

    case Qt::ToolTipRole:
        return QStringLiteral("%1（%2）").arg(r.plate, pl::typeLabel(r.type));

    case Qt::UserRole:
        return r.plate;

    default:
        return {};
    }
}

QVariant VehicleListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};

    if (orientation == Qt::Vertical)
        return {};

    switch (section) {
    case Plate:    return QStringLiteral("车牌号");
    case Type:     return QStringLiteral("车型");
    case Slot:     return QStringLiteral("车位");
    case State:    return QStringLiteral("状态");
    case Since:    return QStringLiteral("入场 / 排队时间");
    case Duration: return QStringLiteral("已停 / 已等");
    case Fee:      return QStringLiteral("预估费用");
    default:       return {};
    }
}

} // namespace ui
