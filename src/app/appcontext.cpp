#include "app/appcontext.h"

#include "domain/billing.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace app {
namespace {

QString demoPlate(int n)
{
    static const char* provinces[] = {"鄂", "京", "沪", "粤", "苏", "浙", "川", "鲁", "豫", "皖"};
    return QStringLiteral("%1A%2")
        .arg(QString::fromUtf8(provinces[static_cast<size_t>(n) % 10]))
        .arg(n % 100000, 5, 10, QLatin1Char('0'));
}

void clearError(QString* error)
{
    if (error)
        error->clear();
}

} // namespace

AppContext::AppContext(QString dataDir, QObject* parent)
    : QObject(parent)
    , dataDir_(std::move(dataDir))
    , store_(dataDir_)
{
}

AppContext::~AppContext()
{
    if (opened_ && !readOnly_) {
        QString ignored;
        store_.compact(lot_, &ignored);
    }
}

bool AppContext::open(QString* error)
{
    clearError(error);

    if (!QDir().mkpath(dataDir_)) {
        if (error)
            *error = QStringLiteral("无法创建数据目录：%1").arg(dataDir_);
        return false;
    }

    // 单实例锁：第二个实例只读打开，避免两个进程同时写同一目录
    lock_ = std::make_unique<QLockFile>(QDir(dataDir_).filePath(QStringLiteral(".app-lock")));
    if (!lock_->tryLock(200)) {
        readOnly_ = true;
        lock_.reset();
    }

    const pl::LotStore::OpenReport rep = store_.open(lot_);
    if (!rep.ok) {
        if (error)
            *error = rep.error;
        return false;
    }

    if (!readOnly_) {
        // 启动即压缩：把上次遗留的 journal 归档进 history 并落一代快照
        QString ignored;
        store_.compact(lot_, &ignored);
    }

    reloadHistory();
    opened_ = true;
    return true;
}

pl::EnterResult AppContext::enterVehicle(const QString& plate, pl::VehicleType t, QString* error)
{
    clearError(error);
    if (!opened_ || readOnly_) {
        if (error)
            *error = QStringLiteral("当前为只读模式，无法写入数据");
        pl::EnterResult r;
        r.status = pl::EnterStatus::InvalidPlate;
        return r;
    }

    const qint64 at = pl::nowMs();
    const pl::EnterResult r = lot_.enter(plate, t, at);
    if (r.status != pl::EnterStatus::Parked && r.status != pl::EnterStatus::Queued)
        return r;

    QString err;
    if (!store_.recordEnter(r, plate, t, at, &err)) {
        lot_.undoEnter(r, plate);
        if (error)
            *error = err;
        return r;
    }

    emit changed();
    return r;
}

pl::ExitResult AppContext::leaveVehicle(const QString& plate, QString* error)
{
    clearError(error);
    if (!opened_ || readOnly_) {
        if (error)
            *error = QStringLiteral("当前为只读模式，无法写入数据");
        pl::ExitResult r;
        r.plate = plate;
        return r;
    }

    const qint64 at = pl::nowMs();

    // 记录队首（若被补位，回滚时需要原样的 queuedAtMs）
    pl::WaitEntry promotedBefore;
    if (!lot_.waiting.isEmpty())
        promotedBefore = lot_.waiting.first();

    const pl::ExitResult r = lot_.leave(plate, at);
    if (r.status != pl::ExitStatus::Ok)
        return r;

    QString err;
    if (!store_.recordExit(r, &err)) {
        lot_.undoLeave(r, promotedBefore);
        if (error)
            *error = err;
        return r;
    }

    ExitRecord rec;
    rec.plate = r.plate;
    rec.ticket = r.ticket;
    rec.type = r.type;
    rec.slot = r.slot;
    rec.enteredAtMs = r.enteredAtMs;
    rec.exitedAtMs = r.exitedAtMs;
    rec.stayMs = r.stayMs;
    rec.feeCents = r.feeCents;
    session_.append(rec);

    emit changed();
    return r;
}

bool AppContext::applyConfig(const pl::Config& cfg, QString* error)
{
    clearError(error);
    if (!opened_ || readOnly_) {
        if (error)
            *error = QStringLiteral("当前为只读模式，无法修改设置");
        return false;
    }

    if (cfg.capacity < 1 || cfg.capacity > 10000) {
        if (error)
            *error = QStringLiteral("车位容量必须在 1~10000 之间");
        return false;
    }
    if (cfg.unitPriceCents < 0 || cfg.freeMinutes < 0 || cfg.billingUnitMinutes < 1
        || cfg.maxWaiting < 0) {
        if (error)
            *error = QStringLiteral("设置项不合法");
        return false;
    }

    const pl::Config old = lot_.cfg;

    if (cfg.capacity != old.capacity) {
        QString err;
        if (!lot_.changeCapacity(cfg.capacity, &err)) {
            if (error)
                *error = err;
            return false;
        }
    }
    lot_.cfg = cfg;

    QString err;
    if (!store_.recordConfig(lot_.cfg, pl::nowMs(), &err)) {
        // 回滚内存配置
        lot_.cfg = old;
        if (cfg.capacity != old.capacity)
            lot_.changeCapacity(old.capacity, nullptr);
        if (error)
            *error = err;
        return false;
    }

    emit changed();
    return true;
}

bool AppContext::compactNow(QString* error)
{
    clearError(error);
    if (!opened_ || readOnly_) {
        if (error)
            *error = QStringLiteral("当前为只读模式，无法压缩");
        return false;
    }

    QString err;
    if (!store_.compact(lot_, &err)) {
        if (error)
            *error = err;
        return false;
    }
    reloadHistory();
    emit changed();
    return true;
}

bool AppContext::exportHistoryCsv(const QString& path, QString* error) const
{
    clearError(error);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("无法写入：%1（%2）").arg(path, f.errorString());
        return false;
    }

    QString csv;
    csv += QStringLiteral("车牌号,车型,车位,入场时间,出场时间,停车时长,金额(元)\n");
    const QList<ExitRecord> exits = allExits();
    for (const ExitRecord& e : exits) {
        const QString slot = e.slot >= 0
            ? QStringLiteral("P%1").arg(e.slot + 1, 3, 10, QLatin1Char('0'))
            : QStringLiteral("—");
        csv += QStringLiteral("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\"\n")
                   .arg(e.plate, pl::typeLabel(e.type), slot,
                        QDateTime::fromMSecsSinceEpoch(e.enteredAtMs)
                            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                        QDateTime::fromMSecsSinceEpoch(e.exitedAtMs)
                            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                        pl::formatDuration(e.stayMs),
                        QString::number(e.feeCents / 100.0, 'f', 2));
    }

    const QByteArray bom("\xEF\xBB\xBF");
    const QByteArray body = csv.toUtf8();
    if (f.write(bom) != bom.size() || f.write(body) != body.size() || !f.commit()) {
        if (error)
            *error = QStringLiteral("写出失败：%1").arg(f.errorString());
        return false;
    }
    return true;
}

QList<ExitRecord> AppContext::readHistoryFromDisk() const
{
    QList<ExitRecord> out;
    const QDir hdir(QDir(dataDir_).filePath(QStringLiteral("history")));
    const QStringList files = hdir.entryList({QStringLiteral("*.ndjson")}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        QFile f(hdir.filePath(name));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> lines = f.readAll().split('\n');
        for (const QByteArray& raw : lines) {
            const QByteArray line = raw.trimmed();
            if (line.isEmpty())
                continue;
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject())
                continue;
            const QJsonObject o = doc.object();
            ExitRecord e;
            e.plate = o.value(QStringLiteral("plate")).toString();
            e.ticket = o.value(QStringLiteral("ticket")).toString();
            pl::VehicleType t = pl::VehicleType::CarSmall;
            if (!pl::typeFromToken(o.value(QStringLiteral("type")).toString(), &t))
                continue;
            e.type = t;
            e.slot = static_cast<int>(o.value(QStringLiteral("slot")).toInteger(-1));
            e.enteredAtMs = o.value(QStringLiteral("enteredAtMs")).toInteger(0);
            e.exitedAtMs = o.value(QStringLiteral("at")).toInteger(0);
            e.stayMs = o.value(QStringLiteral("stayMs")).toInteger(0);
            e.feeCents = o.value(QStringLiteral("feeCents")).toInteger(0);
            out.append(e);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const ExitRecord& a, const ExitRecord& b) { return a.exitedAtMs < b.exitedAtMs; });
    return out;
}

void AppContext::reloadHistory()
{
    history_ = readHistoryFromDisk();
    session_.clear();
}

QList<ExitRecord> AppContext::allExits() const
{
    QList<ExitRecord> all = history_;
    all += session_;
    std::sort(all.begin(), all.end(),
              [](const ExitRecord& a, const ExitRecord& b) { return a.exitedAtMs < b.exitedAtMs; });
    return all;
}

QList<ExitRecord> AppContext::recentExits(int limit) const
{
    QList<ExitRecord> all = allExits();
    std::reverse(all.begin(), all.end());
    if (all.size() > limit)
        all = all.mid(0, limit);
    return all;
}

QMap<QString, qint64> AppContext::monthlyRevenue() const
{
    QMap<QString, qint64> out;
    for (const ExitRecord& e : allExits()) {
        const QString month =
            QDateTime::fromMSecsSinceEpoch(e.exitedAtMs).toString(QStringLiteral("yyyy-MM"));
        out[month] += e.feeCents;
    }
    return out;
}

bool AppContext::seedDemo(QString* error)
{
    clearError(error);
    if (!opened_ || readOnly_) {
        if (error)
            *error = QStringLiteral("当前为只读模式，无法写入演示数据");
        return false;
    }
    if (!lot_.active.isEmpty() || !lot_.waiting.isEmpty()) {
        if (error)
            *error = QStringLiteral("数据目录非空，不能注入演示数据");
        return false;
    }

    pl::Config cfg = lot_.cfg;
    cfg.capacity = 72;
    cfg.unitPriceCents = 500;
    cfg.freeMinutes = 15;
    cfg.billingUnitMinutes = 60;
    cfg.maxWaiting = 200;
    lot_.reset(cfg);

    const qint64 now = pl::nowMs();
    const qint64 minute = 60'000;
    const QList<pl::VehicleType> types = pl::allTypes();

    auto addAt = [&](const QString& plate, pl::VehicleType t, qint64 at) {
        const pl::EnterResult r = lot_.enter(plate, t, at);
        if (r.status == pl::EnterStatus::Parked || r.status == pl::EnterStatus::Queued) {
            QString ignored;
            store_.recordEnter(r, plate, t, at, &ignored);
        }
    };
    auto removeAt = [&](const QString& plate, qint64 at) {
        const pl::ExitResult r = lot_.leave(plate, at);
        if (r.status == pl::ExitStatus::Ok) {
            QString ignored;
            store_.recordExit(r, &ignored);
        }
    };

    // 72 辆 6 小时前入场；8 辆 4 小时前开始排队
    for (int i = 0; i < 72; ++i)
        addAt(demoPlate(i), types.at(i % 4), now - 6 * 60 * minute);
    for (int i = 0; i < 8; ++i)
        addAt(demoPlate(100 + i), types.at(i % 4), now - 4 * 60 * minute);

    // 14 辆 2 小时前出场（前 8 次各补位 1 辆，后 6 次释放空位）
    for (int i = 0; i < 14; ++i)
        removeAt(demoPlate(i), now - 2 * 60 * minute);

    // 12 辆 1 小时前入场：6 辆入位、6 辆排队；再来 2 辆排队 → 队列 8 辆
    for (int i = 0; i < 12; ++i)
        addAt(demoPlate(200 + i), types.at(i % 4), now - 60 * minute);
    for (int i = 0; i < 2; ++i)
        addAt(demoPlate(300 + i), types.at(i % 4), now - 10 * minute);

    QString err;
    if (!store_.compact(lot_, &err)) {
        if (error)
            *error = err;
        return false;
    }
    reloadHistory();
    emit changed();
    return true;
}

} // namespace app
