#pragma once
// ---------------------------------------------------------------------------
// vehiclelistmodel.h — 车辆列表 Model（在场 + 排队），供 QTableView 增量刷新
// ---------------------------------------------------------------------------
#include "domain/lot.h"

#include <QAbstractTableModel>
#include <QVector>

namespace ui {

struct VehicleRow {
    QString plate;
    pl::VehicleType type = pl::VehicleType::CarSmall;
    int slot = -1;
    bool waiting = false;
    int waitPos = 0;
    qint64 sinceMs = 0;
    qint64 durationMs = 0;
    qint64 estFeeCents = -1;   // 排队车辆不计费
};

class VehicleListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Plate = 0,
        Type,
        Slot,
        State,
        Since,
        Duration,
        Fee,
        ColumnCount,
    };

    explicit VehicleListModel(QObject* parent = nullptr);

    void setLot(const pl::Lot* lot, qint64 now);
    void refresh(qint64 now);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    const VehicleRow& rowAt(int row) const { return m_rows.at(row); }

private:
    void updateDurations();

    const pl::Lot* m_lot = nullptr;
    QVector<VehicleRow> m_rows;
    qint64 m_now = 0;
};

} // namespace ui
