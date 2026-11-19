#include "persistence/lotstore.h"

#include "domain/billing.h"
#include "domain/layout.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QSaveFile>

#include <algorithm>

namespace pl {
namespace {

constexpr int kSchemaVersion = 1;

QString join(const QString& dir, const QString& name)
{
    return QDir(dir).filePath(name);
}

void setError(QString* err, const QString& msg)
{
    if (err)
        *err = msg;
}

bool readAll(const QString& path, QByteArray* out)
{
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return false;
    *out = f.readAll();
    return true;
}

bool writeNewFile(const QString& path, const QByteArray& data, QString* err)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        setError(err, QStringLiteral("无法写入文件：%1（%2）").arg(path, f.errorString()));
        return false;
    }
    if (f.write(data) != data.size()) {
        setError(err, QStringLiteral("写入不完整：%1（%2）").arg(path, f.errorString()));
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        setError(err, QStringLiteral("提交失败：%1（%2）").arg(path, f.errorString()));
        return false;
    }
    return true;
}

QString monthKey(qint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM"));
}

QList<QByteArray> splitLines(const QByteArray& text)
{
    return text.split('\n');
}

qint64 maxSeqInFile(const QString& path)
{
    QByteArray text;
    if (!readAll(path, &text))
        return 0;
    qint64 best = 0;
    for (const QByteArray& raw : splitLines(text)) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty())
            continue;
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        best = std::max(best, doc.object().value(QStringLiteral("seq")).toInteger(0));
    }
    return best;
}

// 把一条 journal 记录应用到内存模型（崩溃后重放）
bool applyOp(Lot& lot, const QJsonObject& rec, QString* err)
{
    const QString op = rec.value(QStringLiteral("op")).toString();
    const qint64 at = rec.value(QStringLiteral("at")).toInteger(0);

    if (op == QLatin1String("enter")) {
        ActiveEntry e;
        e.ticket = rec.value(QStringLiteral("ticket")).toString();
        e.plate = rec.value(QStringLiteral("plate")).toString();
        VehicleType t = VehicleType::CarSmall;
        if (!typeFromToken(rec.value(QStringLiteral("type")).toString(), &t)) {
            setError(err, QStringLiteral("enter：车型非法"));
            return false;
        }
        e.type = t;
        e.slot = static_cast<int>(rec.value(QStringLiteral("slot")).toInteger(-1));
        e.enteredAtMs = rec.value(QStringLiteral("enteredAtMs")).toInteger(at);
        if (e.plate.isEmpty()) {
            setError(err, QStringLiteral("enter：车牌为空"));
            return false;
        }
        if (e.slot < 0 || e.slot >= lot.cfg.capacity) {
            setError(err, QStringLiteral("enter：车位越界"));
            return false;
        }
        if (lot.active.contains(e.plate)) {
            setError(err, QStringLiteral("enter：重复车牌 %1").arg(e.plate));
            return false;
        }
        lot.active.insert(e.plate, e);
        lot.slotPlate[e.slot] = e.plate;
        lot.freeSlots.erase(e.slot);
        ++lot.totalEntered;
        return true;
    }

    if (op == QLatin1String("queue")) {
        WaitEntry w;
        w.ticket = rec.value(QStringLiteral("ticket")).toString();
        w.plate = rec.value(QStringLiteral("plate")).toString();
        VehicleType t = VehicleType::CarSmall;
        if (!typeFromToken(rec.value(QStringLiteral("type")).toString(), &t)) {
            setError(err, QStringLiteral("queue：车型非法"));
            return false;
        }
        w.type = t;
        w.queuedAtMs = rec.value(QStringLiteral("queuedAtMs")).toInteger(at);
        if (w.plate.isEmpty()) {
            setError(err, QStringLiteral("queue：车牌为空"));
            return false;
        }
        lot.waiting.append(w);
        ++lot.totalEntered;
        return true;
    }

    if (op == QLatin1String("exit")) {
        const QString plate = rec.value(QStringLiteral("plate")).toString();
        auto it = lot.active.find(plate);
        if (it == lot.active.end()) {
            setError(err, QStringLiteral("exit：车辆不在场内 %1").arg(plate));
            return false;
        }
        const int slot = it->slot;
        lot.active.erase(it);
        lot.slotPlate[slot].clear();
        lot.freeSlots.insert(slot);
        ++lot.totalExited;
        lot.revenueCents += rec.value(QStringLiteral("feeCents")).toInteger(0);

        const QJsonValue promoted = rec.value(QStringLiteral("promoted"));
        if (promoted.isObject()) {
            const QJsonObject p = promoted.toObject();
            const QString ticket = p.value(QStringLiteral("ticket")).toString();
            bool found = false;
            for (int i = 0; i < lot.waiting.size(); ++i) {
                if (lot.waiting.at(i).ticket == ticket) {
                    lot.waiting.removeAt(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                setError(err, QStringLiteral("exit.promoted：凭证不在队列 %1").arg(ticket));
                return false;
            }
            ActiveEntry e;
            e.ticket = ticket;
            e.plate = p.value(QStringLiteral("plate")).toString();
            VehicleType t = VehicleType::CarSmall;
            if (!typeFromToken(p.value(QStringLiteral("type")).toString(), &t)) {
                setError(err, QStringLiteral("exit.promoted：车型非法"));
                return false;
            }
            e.type = t;
            e.slot = static_cast<int>(p.value(QStringLiteral("slot")).toInteger(slot));
            e.enteredAtMs = p.value(QStringLiteral("enteredAtMs")).toInteger(at);
            if (e.slot < 0 || e.slot >= lot.cfg.capacity) {
                setError(err, QStringLiteral("exit.promoted：车位越界"));
                return false;
            }
            lot.active.insert(e.plate, e);
            lot.slotPlate[e.slot] = e.plate;
            lot.freeSlots.erase(e.slot);
        }
        return true;
    }

    // 兼容原型早期只记录单价的日志
    if (op == QLatin1String("price")) {
        lot.cfg.unitPriceCents = static_cast<int>(
            rec.value(QStringLiteral("unitPriceCents")).toInteger(lot.cfg.unitPriceCents));
        return true;
    }

    if (op == QLatin1String("config")) {
        Config c = lot.cfg;
        c.capacity = static_cast<int>(rec.value(QStringLiteral("capacity")).toInteger(c.capacity));
        c.unitPriceCents =
            static_cast<int>(rec.value(QStringLiteral("unitPriceCents")).toInteger(c.unitPriceCents));
        c.freeMinutes = static_cast<int>(rec.value(QStringLiteral("freeMinutes")).toInteger(c.freeMinutes));
        c.billingUnitMinutes =
            static_cast<int>(rec.value(QStringLiteral("billingUnitMinutes")).toInteger(c.billingUnitMinutes));
        c.maxWaiting = static_cast<int>(rec.value(QStringLiteral("maxWaiting")).toInteger(c.maxWaiting));
        if (c.capacity < 1) {
            setError(err, QStringLiteral("config：容量非法"));
            return false;
        }
        if (c.capacity != lot.cfg.capacity) {
            if (!lot.active.isEmpty() || !lot.waiting.isEmpty()) {
                setError(err, QStringLiteral("config：场内非空时不能改容量"));
                return false;
            }
            lot.changeCapacity(c.capacity, nullptr);
        }
        lot.cfg = c;
        return true;
    }

    setError(err, QStringLiteral("未知操作：%1").arg(op));
    return false;
}

} // namespace

LotStore::LotStore(QString dir)
    : dir_(std::move(dir))
{
}

bool LotStore::writeAtomic(const QString& path, const QByteArray& data, QString* err)
{
    return writeNewFile(path, data, err);
}

bool LotStore::appendRecord(QJsonObject rec, QString* err)
{
    const qint64 next = seq_ + 1;
    rec.insert(QStringLiteral("seq"), next);

    QByteArray line = QJsonDocument(rec).toJson(QJsonDocument::Compact);
    line += '\n';

    QFile f(join(dir_, QStringLiteral("journal.ndjson")));
    if (!f.open(QIODevice::Append | QIODevice::WriteOnly)) {
        setError(err, QStringLiteral("无法追加日志（%1）").arg(f.errorString()));
        return false;
    }
    if (f.write(line) != line.size()) {
        setError(err, QStringLiteral("日志写入不完整（%1）").arg(f.errorString()));
        return false;
    }
    if (!f.flush()) {
        setError(err, QStringLiteral("日志落盘失败（%1）").arg(f.errorString()));
        return false;
    }

    seq_ = next;
    ++journalRecords_;
    ++pendingOps_;
    return true;
}

bool LotStore::recordEnter(const EnterResult& r, const QString& plate, VehicleType t, qint64 atMs,
                           QString* err)
{
    if (r.status != EnterStatus::Parked && r.status != EnterStatus::Queued)
        return true;

    QJsonObject rec;
    rec.insert(QStringLiteral("op"), r.status == EnterStatus::Parked ? QStringLiteral("enter")
                                                                    : QStringLiteral("queue"));
    rec.insert(QStringLiteral("ticket"), r.ticket);
    rec.insert(QStringLiteral("plate"), plate);
    rec.insert(QStringLiteral("type"), typeToken(t));
    rec.insert(QStringLiteral("at"), atMs);
    if (r.status == EnterStatus::Parked) {
        rec.insert(QStringLiteral("slot"), r.slot);
        rec.insert(QStringLiteral("enteredAtMs"), atMs);
    } else {
        rec.insert(QStringLiteral("queuedAtMs"), atMs);
    }
    return appendRecord(rec, err);
}

bool LotStore::recordExit(const ExitResult& r, QString* err)
{
    if (r.status != ExitStatus::Ok)
        return true;

    QJsonObject rec;
    rec.insert(QStringLiteral("op"), QStringLiteral("exit"));
    rec.insert(QStringLiteral("ticket"), r.ticket);
    rec.insert(QStringLiteral("plate"), r.plate);
    rec.insert(QStringLiteral("type"), typeToken(r.type));
    rec.insert(QStringLiteral("slot"), r.slot);
    rec.insert(QStringLiteral("enteredAtMs"), r.enteredAtMs);
    rec.insert(QStringLiteral("stayMs"), r.stayMs);
    rec.insert(QStringLiteral("feeCents"), r.feeCents);
    rec.insert(QStringLiteral("at"), r.exitedAtMs);

    if (r.promoted) {
        QJsonObject p;
        p.insert(QStringLiteral("ticket"), r.promotedTicket);
        p.insert(QStringLiteral("plate"), r.promotedPlate);
        p.insert(QStringLiteral("type"), typeToken(r.promotedType));
        p.insert(QStringLiteral("slot"), r.slot);
        p.insert(QStringLiteral("enteredAtMs"), r.promotedEnteredAtMs);
        rec.insert(QStringLiteral("promoted"), p);
    }
    return appendRecord(rec, err);
}

bool LotStore::recordConfig(const Config& cfg, qint64 atMs, QString* err)
{
    QJsonObject rec;
    rec.insert(QStringLiteral("op"), QStringLiteral("config"));
    rec.insert(QStringLiteral("capacity"), cfg.capacity);
    rec.insert(QStringLiteral("unitPriceCents"), cfg.unitPriceCents);
    rec.insert(QStringLiteral("freeMinutes"), cfg.freeMinutes);
    rec.insert(QStringLiteral("billingUnitMinutes"), cfg.billingUnitMinutes);
    rec.insert(QStringLiteral("maxWaiting"), cfg.maxWaiting);
    rec.insert(QStringLiteral("at"), atMs);
    return appendRecord(rec, err);
}

bool LotStore::writeGeneration(const Lot& lot, qint64 gen, QString* err)
{
    const QString gdir = join(dir_, QStringLiteral("gen-%1").arg(gen));
    if (!QDir().mkpath(gdir)) {
        setError(err, QStringLiteral("无法创建快照目录：%1").arg(gdir));
        return false;
    }

    // config.json —— 权威配置
    QJsonObject cfg;
    cfg.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    cfg.insert(QStringLiteral("capacity"), lot.cfg.capacity);
    cfg.insert(QStringLiteral("unitPriceCents"), lot.cfg.unitPriceCents);
    cfg.insert(QStringLiteral("freeMinutes"), lot.cfg.freeMinutes);
    cfg.insert(QStringLiteral("billingUnitMinutes"), lot.cfg.billingUnitMinutes);
    cfg.insert(QStringLiteral("maxWaiting"), lot.cfg.maxWaiting);
    if (!writeNewFile(join(gdir, QStringLiteral("config.json")),
                      QJsonDocument(cfg).toJson(QJsonDocument::Indented), err))
        return false;

    // layout.json —— 派生几何，删掉也能按容量重建
    const Layout L = makeLayout(lot.cfg.capacity);
    QJsonObject layout;
    layout.insert(QStringLiteral("capacity"), lot.cfg.capacity);
    layout.insert(QStringLiteral("rows"), L.rows);
    layout.insert(QStringLiteral("cols"), L.cols);
    layout.insert(QStringLiteral("aisleEvery"), L.aisleEvery);
    layout.insert(QStringLiteral("aislePx"), L.aislePx);
    layout.insert(QStringLiteral("margin"), L.margin);
    layout.insert(QStringLiteral("slotW"), L.slotW);
    layout.insert(QStringLiteral("slotH"), L.slotH);
    layout.insert(QStringLiteral("gapX"), L.gapX);
    layout.insert(QStringLiteral("gapY"), L.gapY);
    QJsonArray slotArr;
    for (const SlotGeom& g : L.slotRects) {
        QJsonObject o;
        o.insert(QStringLiteral("i"), g.index);
        o.insert(QStringLiteral("r"), g.row);
        o.insert(QStringLiteral("c"), g.col);
        o.insert(QStringLiteral("x"), g.x);
        o.insert(QStringLiteral("y"), g.y);
        o.insert(QStringLiteral("w"), g.w);
        o.insert(QStringLiteral("h"), g.h);
        slotArr.append(o);
    }
    layout.insert(QStringLiteral("slots"), slotArr);
    if (!writeNewFile(join(gdir, QStringLiteral("layout.json")),
                      QJsonDocument(layout).toJson(QJsonDocument::Indented), err))
        return false;

    // active.json —— 权威车辆状态（按车位排序，保证同一状态产出同一字节流）
    QJsonObject activeObj;
    activeObj.insert(QStringLiteral("seq"), gen);
    activeObj.insert(QStringLiteral("totalEntered"), lot.totalEntered);
    activeObj.insert(QStringLiteral("totalExited"), lot.totalExited);
    activeObj.insert(QStringLiteral("revenueCents"), lot.revenueCents);
    QJsonArray activeArr;
    for (const ActiveEntry& e : lot.parkedBySlot()) {
        QJsonObject o;
        o.insert(QStringLiteral("ticket"), e.ticket);
        o.insert(QStringLiteral("plate"), e.plate);
        o.insert(QStringLiteral("type"), typeToken(e.type));
        o.insert(QStringLiteral("slot"), e.slot);
        o.insert(QStringLiteral("enteredAtMs"), e.enteredAtMs);
        activeArr.append(o);
    }
    activeObj.insert(QStringLiteral("entries"), activeArr);
    if (!writeNewFile(join(gdir, QStringLiteral("active.json")),
                      QJsonDocument(activeObj).toJson(QJsonDocument::Indented), err))
        return false;

    // waiting.json —— 权威队列顺序（数组顺序即队列顺序）
    QJsonObject waitObj;
    waitObj.insert(QStringLiteral("seq"), gen);
    QJsonArray waitArr;
    for (const WaitEntry& w : lot.waiting) {
        QJsonObject o;
        o.insert(QStringLiteral("ticket"), w.ticket);
        o.insert(QStringLiteral("plate"), w.plate);
        o.insert(QStringLiteral("type"), typeToken(w.type));
        o.insert(QStringLiteral("queuedAtMs"), w.queuedAtMs);
        waitArr.append(o);
    }
    waitObj.insert(QStringLiteral("entries"), waitArr);
    if (!writeNewFile(join(gdir, QStringLiteral("waiting.json")),
                      QJsonDocument(waitObj).toJson(QJsonDocument::Indented), err))
        return false;

    // manifest.json —— 元信息（不承担原子性职责）
    QJsonObject man;
    man.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    man.insert(QStringLiteral("lotId"), lotId_);
    man.insert(QStringLiteral("snapshotSeq"), gen);
    man.insert(QStringLiteral("writtenAtMs"), nowMs());
    man.insert(QStringLiteral("capacity"), lot.cfg.capacity);
    man.insert(QStringLiteral("occupy"), lot.occupyCount());
    man.insert(QStringLiteral("waiting"), static_cast<int>(lot.waiting.size()));
    if (!writeNewFile(join(gdir, QStringLiteral("manifest.json")),
                      QJsonDocument(man).toJson(QJsonDocument::Indented), err))
        return false;

    return true;
}

bool LotStore::archiveExits(QString* err)
{
    QByteArray text;
    if (!readAll(join(dir_, QStringLiteral("journal.ndjson")), &text))
        return true;  // 没有日志即无事可做

    QMap<QString, QVector<QJsonObject>> byMonth;
    for (const QByteArray& raw : splitLines(text)) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty())
            continue;
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject())
            break;  // 残尾：与压缩语义一致，不再继续
        const QJsonObject rec = doc.object();
        if (rec.value(QStringLiteral("op")).toString() != QLatin1String("exit"))
            continue;
        byMonth[monthKey(rec.value(QStringLiteral("at")).toInteger(0))].append(rec);
    }
    if (byMonth.isEmpty())
        return true;

    const QString hdir = join(dir_, QStringLiteral("history"));
    if (!QDir().mkpath(hdir)) {
        setError(err, QStringLiteral("无法创建归档目录：%1").arg(hdir));
        return false;
    }

    for (auto it = byMonth.cbegin(); it != byMonth.cend(); ++it) {
        const QString hpath = join(hdir, it.key() + QStringLiteral(".ndjson"));
        const qint64 lastSeq = maxSeqInFile(hpath);

        QFile f(hpath);
        if (!f.open(QIODevice::Append | QIODevice::WriteOnly)) {
            setError(err, QStringLiteral("无法追加归档：%1（%2）").arg(hpath, f.errorString()));
            return false;
        }
        for (const QJsonObject& rec : it.value()) {
            if (rec.value(QStringLiteral("seq")).toInteger(0) <= lastSeq)
                continue;  // 幂等：跳过已归档的记录
            QByteArray line = QJsonDocument(rec).toJson(QJsonDocument::Compact);
            line += '\n';
            if (f.write(line) != line.size()) {
                setError(err, QStringLiteral("归档写入失败：%1").arg(hpath));
                return false;
            }
        }
        if (!f.flush()) {
            setError(err, QStringLiteral("归档落盘失败：%1").arg(hpath));
            return false;
        }
    }
    return true;
}

bool LotStore::compact(const Lot& lot, QString* err)
{
    // 没有待压缩记录、且当前代已存在 → 无需重写（避免覆盖正在使用的快照）
    const QString existing = join(dir_, QStringLiteral("gen-%1").arg(seq_));
    if (pendingOps_ == 0 && QDir(existing).exists())
        return true;

    // 1) 先归档流水（journal 尚未截断，失败可重来）
    if (!archiveExits(err))
        return false;

    // 2) 写完整的一代快照
    if (!writeGeneration(lot, seq_, err))
        return false;

    // 3) 原子切换 CURRENT（唯一的原子点）
    if (!writeAtomic(join(dir_, QStringLiteral("CURRENT")),
                     QString::number(seq_).toUtf8(), err))
        return false;

    // 4) 截断 journal（若此前崩溃，重放会按 seq 跳过，不会重复应用）
    if (!writeAtomic(join(dir_, QStringLiteral("journal.ndjson")), QByteArray(), err))
        return false;
    journalRecords_ = 0;
    pendingOps_ = 0;
    snapshotSeq_ = seq_;

    // 5) 清理旧代（保留最近 2 代以便回退；删除失败不影响一致性）
    QList<qint64> gens;
    const QStringList names = QDir(dir_).entryList({QStringLiteral("gen-*")},
                                                   QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : names) {
        bool ok = false;
        const qint64 g = name.mid(4).toLongLong(&ok);
        if (ok)
            gens.append(g);
    }
    std::sort(gens.begin(), gens.end());
    while (gens.size() > 2) {
        const QString victim = join(dir_, QStringLiteral("gen-%1").arg(gens.first()));
        QDir(victim).removeRecursively();
        gens.removeFirst();
    }
    return true;
}

LotStore::OpenReport LotStore::open(Lot& lot, const Config& fallback)
{
    OpenReport rep;
    seq_ = 0;
    snapshotSeq_ = 0;
    journalRecords_ = 0;
    pendingOps_ = 0;
    lotId_.clear();

    if (!QDir().mkpath(dir_)) {
        rep.error = QStringLiteral("无法创建数据目录：%1").arg(dir_);
        return rep;
    }

    const QDir d(dir_);

    // 收集可用的快照代号
    QList<qint64> gens;
    const QStringList genNames = d.entryList({QStringLiteral("gen-*")},
                                             QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : genNames) {
        bool ok = false;
        const qint64 g = name.mid(4).toLongLong(&ok);
        if (ok)
            gens.append(g);
    }
    std::sort(gens.begin(), gens.end(), std::greater<qint64>());
    rep.generationsSeen = static_cast<int>(gens.size());

    qint64 cur = -1;
    {
        QByteArray text;
        if (readAll(join(dir_, QStringLiteral("CURRENT")), &text)) {
            bool ok = false;
            const qint64 v = QString::fromUtf8(text).trimmed().toLongLong(&ok);
            if (ok)
                cur = v;
        }
    }

    // 候选：CURRENT 指向的代优先，其次按代号降序
    QList<qint64> candidates;
    if (cur >= 0)
        candidates.append(cur);
    for (qint64 g : gens) {
        if (!candidates.contains(g))
            candidates.append(g);
    }

    Config cfg = fallback;
    qint64 snapshotSeq = 0;
    int schemaVersion = 0;
    QString loadedLotId;
    bool loaded = false;

    for (qint64 g : candidates) {
        const QString gdir = join(dir_, QStringLiteral("gen-%1").arg(g));
        if (!QFile::exists(join(gdir, QStringLiteral("active.json")))
            || !QFile::exists(join(gdir, QStringLiteral("waiting.json"))))
            continue;

        QByteArray text;
        if (!readAll(join(gdir, QStringLiteral("config.json")), &text))
            continue;
        QJsonParseError pe{};
        const QJsonDocument cfgDoc = QJsonDocument::fromJson(text, &pe);
        if (pe.error != QJsonParseError::NoError || !cfgDoc.isObject())
            continue;
        const QJsonObject co = cfgDoc.object();
        Config c = cfg;
        c.capacity = co.value(QStringLiteral("capacity")).toInt(c.capacity);
        c.unitPriceCents = co.value(QStringLiteral("unitPriceCents")).toInt(c.unitPriceCents);
        c.freeMinutes = co.value(QStringLiteral("freeMinutes")).toInt(c.freeMinutes);
        c.billingUnitMinutes = co.value(QStringLiteral("billingUnitMinutes")).toInt(c.billingUnitMinutes);
        c.maxWaiting = co.value(QStringLiteral("maxWaiting")).toInt(c.maxWaiting);
        if (c.capacity < 1)
            continue;

        lot.reset(c);
        cfg = c;
        bool ok = true;
        qint64 candSeq = 0;   // 该候选代的快照序号：只有整代加载成功才采纳

        // active.json
        if (!readAll(join(gdir, QStringLiteral("active.json")), &text)) {
            ok = false;
        } else {
            const QJsonDocument doc = QJsonDocument::fromJson(text, &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                ok = false;
            } else {
                const QJsonObject a = doc.object();
                candSeq = std::max(candSeq, a.value(QStringLiteral("seq")).toInteger(g));
                lot.totalEntered = a.value(QStringLiteral("totalEntered")).toInteger(0);
                lot.totalExited = a.value(QStringLiteral("totalExited")).toInteger(0);
                lot.revenueCents = a.value(QStringLiteral("revenueCents")).toInteger(0);
                const QJsonArray arr = a.value(QStringLiteral("entries")).toArray();
                for (const QJsonValue& v : arr) {
                    const QJsonObject e = v.toObject();
                    ActiveEntry ae;
                    ae.ticket = e.value(QStringLiteral("ticket")).toString();
                    ae.plate = e.value(QStringLiteral("plate")).toString();
                    VehicleType t = VehicleType::CarSmall;
                    if (!typeFromToken(e.value(QStringLiteral("type")).toString(), &t)
                        || ae.plate.isEmpty()) {
                        ok = false;
                        break;
                    }
                    ae.type = t;
                    ae.slot = static_cast<int>(e.value(QStringLiteral("slot")).toInteger(-1));
                    ae.enteredAtMs = e.value(QStringLiteral("enteredAtMs")).toInteger(0);
                    if (ae.slot < 0 || ae.slot >= lot.cfg.capacity || ae.enteredAtMs <= 0
                        || lot.active.contains(ae.plate)) {
                        ok = false;
                        break;
                    }
                    lot.active.insert(ae.plate, ae);
                    lot.slotPlate[ae.slot] = ae.plate;
                    lot.freeSlots.erase(ae.slot);
                }
            }
        }
        if (!ok)
            continue;

        // waiting.json
        if (!readAll(join(gdir, QStringLiteral("waiting.json")), &text)) {
            ok = false;
        } else {
            const QJsonDocument doc = QJsonDocument::fromJson(text, &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                ok = false;
            } else {
                const QJsonObject w = doc.object();
                candSeq = std::max(candSeq, w.value(QStringLiteral("seq")).toInteger(g));
                const QJsonArray arr = w.value(QStringLiteral("entries")).toArray();
                for (const QJsonValue& v : arr) {
                    const QJsonObject e = v.toObject();
                    WaitEntry we;
                    we.ticket = e.value(QStringLiteral("ticket")).toString();
                    we.plate = e.value(QStringLiteral("plate")).toString();
                    VehicleType t = VehicleType::CarSmall;
                    if (!typeFromToken(e.value(QStringLiteral("type")).toString(), &t)
                        || we.plate.isEmpty()) {
                        ok = false;
                        break;
                    }
                    we.type = t;
                    we.queuedAtMs = e.value(QStringLiteral("queuedAtMs")).toInteger(0);
                    lot.waiting.append(we);
                }
            }
        }
        if (!ok)
            continue;

        // manifest.json（缺失不算错误）
        if (readAll(join(gdir, QStringLiteral("manifest.json")), &text)) {
            const QJsonDocument doc = QJsonDocument::fromJson(text, &pe);
            if (pe.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject m = doc.object();
                schemaVersion = m.value(QStringLiteral("schemaVersion")).toInt(0);
                loadedLotId = m.value(QStringLiteral("lotId")).toString();
            }
        }

        loaded = true;
        snapshotSeq = candSeq;
        break;
    }

    if (!loaded && !gens.isEmpty()) {
        rep.error = QStringLiteral("所有快照代均不可用，数据目录可能已损坏：%1").arg(dir_);
        return rep;
    }

    QByteArray journalText;
    QByteArray text;

    if (!loaded) {
        // 全新目录：允许根目录保留一份旧式 config.json（兼容原型/手工配置）
        if (readAll(join(dir_, QStringLiteral("config.json")), &text)) {
            const QJsonDocument doc = QJsonDocument::fromJson(text);
            if (doc.isObject()) {
                const QJsonObject v = doc.object();
                cfg.capacity = v.value(QStringLiteral("capacity")).toInt(cfg.capacity);
                cfg.unitPriceCents = v.value(QStringLiteral("unitPriceCents")).toInt(cfg.unitPriceCents);
                cfg.freeMinutes = v.value(QStringLiteral("freeMinutes")).toInt(cfg.freeMinutes);
                cfg.billingUnitMinutes =
                    v.value(QStringLiteral("billingUnitMinutes")).toInt(cfg.billingUnitMinutes);
                cfg.maxWaiting = v.value(QStringLiteral("maxWaiting")).toInt(cfg.maxWaiting);
            }
        }
        lot.reset(cfg);
        rep.created = true;
    }

    if (lotId_.isEmpty())
        lotId_ = loadedLotId;
    if (lotId_.isEmpty())
        lotId_ = QStringLiteral("lot-%1").arg(nowMs());

    // 重放 journal
    const bool hasJournal = readAll(join(dir_, QStringLiteral("journal.ndjson")), &journalText);
    if (hasJournal && !loaded) {
        // 快照全丢时，容量只能从日志里的最大车位号反推（至少不小于默认容量）
        int maxSlot = -1;
        for (const QByteArray& raw : splitLines(journalText)) {
            const QByteArray line = raw.trimmed();
            if (line.isEmpty())
                continue;
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject())
                break;
            const QJsonObject rec = doc.object();
            if (rec.value(QStringLiteral("op")).toString() == QLatin1String("enter"))
                maxSlot = std::max(maxSlot,
                                   static_cast<int>(rec.value(QStringLiteral("slot")).toInteger(-1)));
        }
        if (maxSlot + 1 > cfg.capacity) {
            cfg.capacity = maxSlot + 1;
            lot.reset(cfg);
        }
    }

    if (hasJournal) {
        for (const QByteArray& raw : splitLines(journalText)) {
            const QByteArray line = raw.trimmed();
            if (line.isEmpty())
                continue;

            QJsonParseError pe{};
            const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                ++rep.tornLines;  // 残尾：最后一行写了一半
                break;
            }
            const QJsonObject rec = doc.object();
            const qint64 s = rec.value(QStringLiteral("seq")).toInteger(0);
            seq_ = std::max(seq_, s);
            ++journalRecords_;
            if (s <= snapshotSeq)
                continue;
            QString aerr;
            if (!applyOp(lot, rec, &aerr)) {
                rep.error = QStringLiteral("重放失败（seq=%1）：%2").arg(s).arg(aerr);
                return rep;
            }
            ++rep.replayedOps;
            ++pendingOps_;
        }
    }

    seq_ = std::max(seq_, snapshotSeq);
    snapshotSeq_ = snapshotSeq;

    QString why;
    if (!lot.validate(&why)) {
        rep.error = QStringLiteral("不变量被破坏：%1").arg(why);
        return rep;
    }

    rep.schemaVersion = schemaVersion;
    rep.snapshotSeq = snapshotSeq;
    rep.ok = true;
    return rep;
}

LotStore::Stats LotStore::stats() const
{
    Stats s;
    s.seq = seq_;
    s.snapshotSeq = snapshotSeq_;
    s.journalRecords = journalRecords_;

    const QFileInfo jf(join(dir_, QStringLiteral("journal.ndjson")));
    if (jf.exists())
        s.journalBytes = jf.size();

    const QDir d(dir_);
    const QString historyDir = d.filePath(QStringLiteral("history"));

    QDirIterator it(dir_, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fi(it.next());
        if (!fi.isFile())
            continue;
        s.totalBytes += fi.size();
        if (fi.absolutePath() == historyDir) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray data = f.readAll();
                s.historyRecords += static_cast<int>(data.count('\n'));
            }
        }
    }

    const QStringList names = d.entryList({QStringLiteral("gen-*")},
                                          QDir::Dirs | QDir::NoDotAndDotDot);
    s.generations = static_cast<int>(names.size());
    return s;
}

} // namespace pl
