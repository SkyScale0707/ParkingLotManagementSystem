#include "domain/lot.h"
#include "persistence/lotstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace pl;

namespace {

bool enterAndRecord(LotStore& store, Lot& lot, const QString& plate, VehicleType t, qint64 at,
                    QString* err)
{
    const EnterResult r = lot.enter(plate, t, at);
    if (r.status != EnterStatus::Parked && r.status != EnterStatus::Queued) {
        if (err)
            *err = QStringLiteral("入场失败");
        return false;
    }
    return store.recordEnter(r, plate, t, at, err);
}

bool leaveAndRecord(LotStore& store, Lot& lot, const QString& plate, qint64 at, QString* err)
{
    const ExitResult r = lot.leave(plate, at);
    if (r.status != ExitStatus::Ok) {
        if (err)
            *err = QStringLiteral("出场失败");
        return false;
    }
    return store.recordExit(r, err);
}

int countGenerations(const QString& dir)
{
    return QDir(dir).entryList({QStringLiteral("gen-*")}, QDir::Dirs | QDir::NoDotAndDotDot).size();
}

int countLines(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    return static_cast<int>(f.readAll().count('\n'));
}

} // namespace

class TestPersistence : public QObject
{
    Q_OBJECT

private slots:
    void snapshotRoundTrip();
    void journalReplayAfterCrash();
    void tornTailDropped();
    void rebuildFromJournalOnly();
    void archiveIsIdempotent();
    void generationCleanup();
    void configPersisted();
    void corruptSnapshotRejected();
    void missingCurrentFallsBack();
    void duplicatedJournalRecordsIgnored();
};

void TestPersistence::snapshotRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 4;
    const auto rep = store.open(lot, cfg);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QVERIFY(rep.created);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("鄂A00001"), VehicleType::CarSmall, 1000, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("鄂A00002"), VehicleType::TruckLarge, 2000, &err));
    QVERIFY2(store.compact(lot, &err), qPrintable(err));

    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("CURRENT"))));
    QCOMPARE(countGenerations(dir.path()), 1);

    Lot reopened;
    LotStore store2(dir.path());
    const auto rep2 = store2.open(reopened);
    QVERIFY2(rep2.ok, qPrintable(rep2.error));
    QVERIFY(!rep2.created);
    QCOMPARE(rep2.replayedOps, 0);
    QCOMPARE(rep2.tornLines, 0);
    QCOMPARE(rep2.snapshotSeq, qint64(2));
    QCOMPARE(reopened.cfg.capacity, 4);
    QCOMPARE(reopened.occupyCount(), 2);
    QVERIFY(reopened.hasPlate(QStringLiteral("鄂A00001")));
    QCOMPARE(reopened.totalEntered, qint64(2));
    QVERIFY(reopened.validate());
}

void TestPersistence::journalReplayAfterCrash()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 4;
    QVERIFY(store.open(lot, cfg).ok);

    const qint64 t0 = QDateTime(QDate(2025, 11, 10), QTime(12, 0)).toMSecsSinceEpoch();
    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, t0, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("B2"), VehicleType::TruckSmall, t0 + 1000, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("C3"), VehicleType::TruckMedium, t0 + 2000, &err));
    QVERIFY(leaveAndRecord(store, lot, QStringLiteral("A1"), t0 + 3'600'000 + 1000, &err));

    // 不压缩，直接模拟崩溃重启
    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(rep.replayedOps, 4);
    QCOMPARE(rep.tornLines, 0);
    QCOMPARE(reopened.occupyCount(), 2);
    QVERIFY(!reopened.hasPlate(QStringLiteral("A1")));
    QCOMPARE(reopened.totalEntered, qint64(3));
    QCOMPARE(reopened.totalExited, qint64(1));
    QCOMPARE(reopened.revenueCents, qint64(500));
    QVERIFY(reopened.validate());
}

void TestPersistence::tornTailDropped()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 4;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, 1000, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("B2"), VehicleType::CarSmall, 2000, &err));

    // 人为追加半条记录（模拟写到一半被杀）
    QFile f(dir.filePath(QStringLiteral("journal.ndjson")));
    QVERIFY(f.open(QIODevice::Append | QIODevice::WriteOnly));
    f.write("{\"seq\":99,\"op\":\"enter\",\"plate\":\"XX");
    f.close();

    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(rep.tornLines, 1);
    QCOMPARE(rep.replayedOps, 2);
    QCOMPARE(reopened.occupyCount(), 2);
    QVERIFY(reopened.validate());
}

void TestPersistence::rebuildFromJournalOnly()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 3;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, 1000, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("B2"), VehicleType::CarSmall, 2000, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("C3"), VehicleType::CarSmall, 3000, &err));
    QCOMPARE(countGenerations(dir.path()), 0);

    // 容量只能从日志反推（fallback 故意给 1）
    Config tiny;
    tiny.capacity = 1;
    Lot rebuilt;
    LotStore store2(dir.path());
    const auto rep = store2.open(rebuilt, tiny);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(rep.replayedOps, 3);
    QCOMPARE(rebuilt.cfg.capacity, 3);
    QCOMPARE(rebuilt.occupyCount(), 3);
    QVERIFY(rebuilt.validate());
}

void TestPersistence::archiveIsIdempotent()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 2;
    QVERIFY(store.open(lot, cfg).ok);

    const qint64 t0 = QDateTime(QDate(2025, 11, 10), QTime(12, 0)).toMSecsSinceEpoch();
    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, t0, &err));
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("B2"), VehicleType::CarSmall, t0 + 1000, &err));
    QVERIFY(leaveAndRecord(store, lot, QStringLiteral("A1"), t0 + 3'600'000, &err));
    QVERIFY2(store.compact(lot, &err), qPrintable(err));

    const QString history = dir.filePath(QStringLiteral("history/2025-11.ndjson"));
    QVERIFY(QFile::exists(history));
    QCOMPARE(countLines(history), 1);

    // 再次归档：按 seq 幂等，不应重复写入
    QVERIFY2(store.archiveExits(&err), qPrintable(err));
    QCOMPARE(countLines(history), 1);
}

void TestPersistence::generationCleanup()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 10;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    for (int i = 0; i < 4; ++i) {
        QVERIFY(enterAndRecord(store, lot, QStringLiteral("P%1").arg(i), VehicleType::CarSmall,
                               static_cast<qint64>(1000 + i), &err));
        QVERIFY2(store.compact(lot, &err), qPrintable(err));
    }
    QCOMPARE(lot.occupyCount(), 4);
    QVERIFY(countGenerations(dir.path()) <= 2);

    // 保留的两代中最新一代能被正确加载
    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(reopened.occupyCount(), 4);
}

void TestPersistence::configPersisted()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 4;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, 1000, &err));
    QVERIFY(leaveAndRecord(store, lot, QStringLiteral("A1"), 4000, &err));

    Config newCfg = lot.cfg;
    newCfg.unitPriceCents = 800;
    newCfg.freeMinutes = 30;
    newCfg.billingUnitMinutes = 30;
    newCfg.capacity = 8;
    QVERIFY(lot.changeCapacity(newCfg.capacity, &err));
    lot.cfg = newCfg;
    QVERIFY2(store.recordConfig(newCfg, 5000, &err), qPrintable(err));
    QVERIFY2(store.compact(lot, &err), qPrintable(err));

    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(reopened.cfg.capacity, 8);
    QCOMPARE(reopened.cfg.unitPriceCents, 800);
    QCOMPARE(reopened.cfg.freeMinutes, 30);
    QCOMPARE(reopened.cfg.billingUnitMinutes, 30);
    QCOMPARE(static_cast<int>(reopened.freeSlots.size()), 8);
    QVERIFY(reopened.validate());
}

void TestPersistence::corruptSnapshotRejected()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 2;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, 1000, &err));
    QVERIFY2(store.compact(lot, &err), qPrintable(err));

    const QString genDir = QDir(dir.path()).entryList({QStringLiteral("gen-*")},
                                                      QDir::Dirs | QDir::NoDotAndDotDot).value(0);
    QVERIFY(!genDir.isEmpty());

    QFile f(dir.filePath(genDir + QStringLiteral("/active.json")));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("{ not json");
    f.close();

    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY(!rep.ok);
    QVERIFY(!rep.error.isEmpty());
}

void TestPersistence::missingCurrentFallsBack()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 3;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, 1000, &err));
    QVERIFY2(store.compact(lot, &err), qPrintable(err));

    // CURRENT 损坏：应回退到目录中最新的一代，而不是直接失败
    {
        QFile f(dir.filePath(QStringLiteral("CURRENT")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("999");
        f.close();
    }

    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(reopened.occupyCount(), 1);
    QVERIFY(reopened.validate());
}

void TestPersistence::duplicatedJournalRecordsIgnored()
{
    QTemporaryDir dir;
    LotStore store(dir.path());
    Lot lot;
    Config cfg;
    cfg.capacity = 2;
    QVERIFY(store.open(lot, cfg).ok);

    QString err;
    QVERIFY(enterAndRecord(store, lot, QStringLiteral("A1"), VehicleType::CarSmall, 1000, &err));
    QVERIFY2(store.compact(lot, &err), qPrintable(err));

    // 模拟"切换 CURRENT 后、清空 journal 前"崩溃留下的旧记录
    QFile f(dir.filePath(QStringLiteral("journal.ndjson")));
    QVERIFY(f.open(QIODevice::Append | QIODevice::WriteOnly));
    f.write("{\"seq\":1,\"op\":\"enter\",\"ticket\":\"T-1\",\"plate\":\"A1\",\"type\":\"car_small\",\"slot\":0,\"enteredAtMs\":1000,\"at\":1000}\n");
    f.close();

    Lot reopened;
    LotStore store2(dir.path());
    const auto rep = store2.open(reopened);
    QVERIFY2(rep.ok, qPrintable(rep.error));
    QCOMPARE(rep.replayedOps, 0);   // seq <= snapshotSeq，跳过
    QCOMPARE(reopened.occupyCount(), 1);
    QCOMPARE(reopened.totalEntered, qint64(1));
    QVERIFY(reopened.validate());
}

QTEST_GUILESS_MAIN(TestPersistence)

#include "test_persistence.moc"
