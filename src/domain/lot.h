#pragma once
// ---------------------------------------------------------------------------
// lot.h — 停车场领域模型
//
// 单一真相源：active（车牌 → 场内记录）。slotPlate / freeSlots 都是由 active
// 推导出的索引，绝不单独落盘；因此不可能出现"车位空闲但车还在"的状态。
// ---------------------------------------------------------------------------
#include "domain/types.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <set>

namespace pl {

class Lot
{
public:
    Config cfg;

    QHash<QString, ActiveEntry> active;   // 车牌 → 场内记录
    QList<WaitEntry> waiting;             // FIFO，队首先补位
    QVector<QString> slotPlate;           // 车位 → 车牌（空串表示空闲）【派生】
    std::set<int> freeSlots;              // 空闲车位（有序，取最小号 O(log n)）【派生】

    qint64 totalEntered = 0;   // 入场次数（含入队）
    qint64 totalExited = 0;    // 出场次数
    qint64 revenueCents = 0;   // 累计收入（分）

    Lot() { reset(Config{}); }

    void reset(const Config& c);
    bool validate(QString* why = nullptr) const;

    EnterResult enter(const QString& plate, VehicleType t, qint64 atMs);
    ExitResult leave(const QString& plate, qint64 atMs);

    // 持久化失败时的精确回滚：journal 追加失败 → 内存状态必须回到操作前。
    void undoEnter(const EnterResult& r, const QString& plate);
    void undoLeave(const ExitResult& r, const WaitEntry& promotedBefore);

    int occupyCount() const { return active.size(); }
    int emptyCount() const { return cfg.capacity - occupyCount(); }
    bool hasPlate(const QString& plate) const { return active.contains(plate); }
    int waitingIndex(const QString& plate) const;

    QString slotLabel(int slot) const;             // 0 -> "P001"
    QList<ActiveEntry> parkedBySlot() const;       // 按车位号排序
    int countByType(VehicleType t, bool includeWaiting) const;

    // 调整容量：仅允许在场内/队列都为空时执行（不破坏既有业务数据）。
    bool changeCapacity(int newCapacity, QString* err);
};

} // namespace pl
