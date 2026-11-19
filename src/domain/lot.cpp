#include "domain/lot.h"

#include "domain/billing.h"

#include <QSet>

#include <algorithm>

namespace pl {

void Lot::reset(const Config& c)
{
    cfg = c;
    if (cfg.capacity < 1)
        cfg.capacity = 1;
    if (cfg.maxWaiting < 0)
        cfg.maxWaiting = 0;
    if (cfg.unitPriceCents < 0)
        cfg.unitPriceCents = 0;
    if (cfg.freeMinutes < 0)
        cfg.freeMinutes = 0;
    if (cfg.billingUnitMinutes <= 0)
        cfg.billingUnitMinutes = 60;

    active.clear();
    waiting.clear();
    slotPlate.assign(cfg.capacity, QString());
    freeSlots.clear();
    for (int i = 0; i < cfg.capacity; ++i)
        freeSlots.insert(i);
    totalEntered = totalExited = revenueCents = 0;
}

bool Lot::validate(QString* why) const
{
    auto fail = [why](const QString& msg) {
        if (why)
            *why = msg;
        return false;
    };

    if (cfg.capacity <= 0)
        return fail(QStringLiteral("容量非法"));
    if (slotPlate.size() != cfg.capacity)
        return fail(QStringLiteral("车位映射尺寸与容量不符"));
    if (active.size() + static_cast<int>(freeSlots.size()) != cfg.capacity)
        return fail(QStringLiteral("空闲车位集合与场内车辆数不互补"));

    QSet<int> occupied;
    for (auto it = active.cbegin(); it != active.cend(); ++it) {
        const ActiveEntry& e = it.value();
        if (it.key() != e.plate)
            return fail(QStringLiteral("车牌键值不一致"));
        if (e.slot < 0 || e.slot >= cfg.capacity)
            return fail(QStringLiteral("车位越界：%1").arg(e.plate));
        if (slotPlate.at(e.slot) != e.plate)
            return fail(QStringLiteral("车位映射不一致：%1").arg(e.plate));
        if (occupied.contains(e.slot))
            return fail(QStringLiteral("同一车位被重复占用"));
        occupied.insert(e.slot);
        if (e.enteredAtMs <= 0)
            return fail(QStringLiteral("入场时间非法：%1").arg(e.plate));
    }

    for (int s : freeSlots) {
        if (s < 0 || s >= cfg.capacity)
            return fail(QStringLiteral("空闲车位越界"));
        if (!slotPlate.at(s).isEmpty())
            return fail(QStringLiteral("空闲车位却登记了车辆"));
        if (occupied.contains(s))
            return fail(QStringLiteral("空闲集合包含已占车位"));
    }

    if (static_cast<int>(waiting.size()) > cfg.maxWaiting)
        return fail(QStringLiteral("等待队列超过上限"));
    QSet<QString> waitingPlates;
    for (const WaitEntry& w : waiting) {
        if (w.plate.isEmpty())
            return fail(QStringLiteral("队列中存在空车牌"));
        if (waitingPlates.contains(w.plate))
            return fail(QStringLiteral("队列内车牌重复：%1").arg(w.plate));
        waitingPlates.insert(w.plate);
        if (active.contains(w.plate))
            return fail(QStringLiteral("同一车辆既在场内又在队列：%1").arg(w.plate));
    }
    if (!waiting.isEmpty() && !freeSlots.empty())
        return fail(QStringLiteral("有车辆排队却仍存在空位"));

    return true;
}

EnterResult Lot::enter(const QString& plate, VehicleType t, qint64 atMs)
{
    EnterResult r;
    if (plate.isEmpty()) {
        r.status = EnterStatus::InvalidPlate;
        return r;
    }
    if (active.contains(plate)) {
        r.status = EnterStatus::DuplicateActive;
        return r;
    }
    for (const WaitEntry& w : waiting) {
        if (w.plate == plate) {
            r.status = EnterStatus::DuplicateWaiting;
            return r;
        }
    }

    const QString ticket = QStringLiteral("T-%1").arg(totalEntered + 1);

    if (!freeSlots.empty()) {
        const int slot = *freeSlots.begin();
        freeSlots.erase(freeSlots.begin());
        ActiveEntry e;
        e.ticket = ticket;
        e.plate = plate;
        e.type = t;
        e.slot = slot;
        e.enteredAtMs = atMs;
        active.insert(plate, e);
        slotPlate[slot] = plate;
        ++totalEntered;
        r.status = EnterStatus::Parked;
        r.slot = slot;
        r.ticket = ticket;
        return r;
    }

    if (static_cast<int>(waiting.size()) >= cfg.maxWaiting) {
        r.status = EnterStatus::QueueFull;
        return r;
    }

    WaitEntry w;
    w.ticket = ticket;
    w.plate = plate;
    w.type = t;
    w.queuedAtMs = atMs;
    waiting.append(w);
    ++totalEntered;
    r.status = EnterStatus::Queued;
    r.ticket = ticket;
    return r;
}

ExitResult Lot::leave(const QString& plate, qint64 atMs)
{
    ExitResult r;
    auto it = active.find(plate);
    if (it == active.end()) {
        for (const WaitEntry& w : waiting) {
            if (w.plate == plate) {
                r.status = ExitStatus::StillWaiting;
                r.plate = plate;
                r.ticket = w.ticket;
                return r;
            }
        }
        r.status = ExitStatus::NotFound;
        r.plate = plate;
        return r;
    }

    const ActiveEntry e = it.value();
    r.status = ExitStatus::Ok;
    r.ticket = e.ticket;
    r.plate = e.plate;
    r.type = e.type;
    r.slot = e.slot;
    r.enteredAtMs = e.enteredAtMs;
    r.exitedAtMs = atMs;
    r.stayMs = atMs - e.enteredAtMs;
    if (r.stayMs < 0)
        r.stayMs = 0;
    r.feeCents = feeCentsFor(e.enteredAtMs, atMs, cfg);

    active.erase(it);
    slotPlate[e.slot].clear();
    freeSlots.insert(e.slot);
    ++totalExited;
    revenueCents += r.feeCents;

    if (!waiting.isEmpty()) {
        const WaitEntry w = waiting.takeFirst();
        // 决策 2：排队时间不计费，计费起点重置为补位入位时刻。
        ActiveEntry promoted;
        promoted.ticket = w.ticket;
        promoted.plate = w.plate;
        promoted.type = w.type;
        promoted.slot = e.slot;
        promoted.enteredAtMs = atMs;
        active.insert(promoted.plate, promoted);
        slotPlate[e.slot] = promoted.plate;
        freeSlots.erase(e.slot);

        r.promoted = true;
        r.promotedTicket = w.ticket;
        r.promotedPlate = w.plate;
        r.promotedType = w.type;
        r.promotedEnteredAtMs = atMs;
    }
    return r;
}

void Lot::undoEnter(const EnterResult& r, const QString& plate)
{
    if (r.status == EnterStatus::Parked) {
        auto it = active.find(plate);
        if (it != active.end()) {
            slotPlate[it->slot].clear();
            freeSlots.insert(it->slot);
            active.erase(it);
        }
        --totalEntered;
    } else if (r.status == EnterStatus::Queued) {
        if (!waiting.isEmpty() && waiting.last().plate == plate)
            waiting.removeLast();
        --totalEntered;
    }
}

void Lot::undoLeave(const ExitResult& r, const WaitEntry& promotedBefore)
{
    if (r.status != ExitStatus::Ok)
        return;

    ActiveEntry e;
    e.ticket = r.ticket;
    e.plate = r.plate;
    e.type = r.type;
    e.slot = r.slot;
    e.enteredAtMs = r.enteredAtMs;
    active.insert(e.plate, e);
    slotPlate[e.slot] = e.plate;
    freeSlots.erase(e.slot);
    --totalExited;
    revenueCents -= r.feeCents;

    if (r.promoted) {
        auto it = active.find(r.promotedPlate);
        if (it != active.end()) {
            slotPlate[it->slot].clear();
            freeSlots.insert(it->slot);
            active.erase(it);
        }
        if (!promotedBefore.plate.isEmpty())
            waiting.prepend(promotedBefore);
    }
}

int Lot::waitingIndex(const QString& plate) const
{
    for (int i = 0; i < waiting.size(); ++i) {
        if (waiting.at(i).plate == plate)
            return i;
    }
    return -1;
}

QString Lot::slotLabel(int slot) const
{
    if (slot < 0)
        return QStringLiteral("—");
    return QStringLiteral("P%1").arg(slot + 1, 3, 10, QLatin1Char('0'));
}

QList<ActiveEntry> Lot::parkedBySlot() const
{
    QList<ActiveEntry> list;
    list.reserve(active.size());
    for (auto it = active.cbegin(); it != active.cend(); ++it)
        list.append(it.value());
    std::sort(list.begin(), list.end(),
              [](const ActiveEntry& a, const ActiveEntry& b) { return a.slot < b.slot; });
    return list;
}

int Lot::countByType(VehicleType t, bool includeWaiting) const
{
    int n = 0;
    for (auto it = active.cbegin(); it != active.cend(); ++it) {
        if (it.value().type == t)
            ++n;
    }
    if (includeWaiting) {
        for (const WaitEntry& w : waiting) {
            if (w.type == t)
                ++n;
        }
    }
    return n;
}

bool Lot::changeCapacity(int newCapacity, QString* err)
{
    if (newCapacity < 1 || newCapacity > 10000) {
        if (err)
            *err = QStringLiteral("容量必须在 1~10000 之间");
        return false;
    }
    if (!active.isEmpty() || !waiting.isEmpty()) {
        if (err)
            *err = QStringLiteral("场内仍有车辆或队列非空，不能调整容量");
        return false;
    }

    cfg.capacity = newCapacity;
    slotPlate.assign(newCapacity, QString());
    freeSlots.clear();
    for (int i = 0; i < newCapacity; ++i)
        freeSlots.insert(i);
    return true;
}

} // namespace pl
