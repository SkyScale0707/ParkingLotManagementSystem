#include "domain/billing.h"
#include "domain/layout.h"
#include "domain/lot.h"

#include <QTest>

using namespace pl;

class TestDomain : public QObject
{
    Q_OBJECT

private slots:
    void enterParked();
    void duplicatePlate();
    void queueAndPromote();
    void queueLimit();
    void feeBoundaries_data();
    void feeBoundaries();
    void leaveNotFound();
    void invariantsHold();
    void invariantsDetectCorruption();
    void layoutGeometry();
    void capacityChange();
    void formatting();
};

void TestDomain::enterParked()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 3;
    lot.reset(cfg);

    QVERIFY(lot.validate());
    const EnterResult r = lot.enter(QStringLiteral("鄂A00001"), VehicleType::CarSmall, 1000);
    QCOMPARE(r.status, EnterStatus::Parked);
    QCOMPARE(r.slot, 0);
    QCOMPARE(r.ticket, QStringLiteral("T-1"));
    QCOMPARE(lot.occupyCount(), 1);
    QCOMPARE(lot.emptyCount(), 2);
    QVERIFY(lot.hasPlate(QStringLiteral("鄂A00001")));
    QVERIFY(!lot.freeSlots.count(0));
    QVERIFY(lot.slotPlate.at(0) == QStringLiteral("鄂A00001"));

    // 最小空位优先：释放 0 号位后应重新使用 0
    lot.leave(QStringLiteral("鄂A00001"), 2000);
    const EnterResult r2 = lot.enter(QStringLiteral("鄂A00002"), VehicleType::TruckLarge, 3000);
    QCOMPARE(r2.status, EnterStatus::Parked);
    QCOMPARE(r2.slot, 0);
    QVERIFY(lot.validate());
}

void TestDomain::duplicatePlate()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 1;
    lot.reset(cfg);

    QCOMPARE(lot.enter(QStringLiteral("A"), VehicleType::CarSmall, 1).status, EnterStatus::Parked);
    QCOMPARE(lot.enter(QStringLiteral("A"), VehicleType::CarSmall, 2).status,
             EnterStatus::DuplicateActive);
    QCOMPARE(lot.enter(QStringLiteral("B"), VehicleType::CarSmall, 3).status, EnterStatus::Queued);
    QCOMPARE(lot.enter(QStringLiteral("B"), VehicleType::CarSmall, 4).status,
             EnterStatus::DuplicateWaiting);
    QCOMPARE(lot.enter(QString(), VehicleType::CarSmall, 5).status, EnterStatus::InvalidPlate);
    QVERIFY(lot.validate());
}

void TestDomain::queueAndPromote()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 1;
    lot.reset(cfg);

    const qint64 t0 = 1'700'000'000'000;
    QCOMPARE(lot.enter(QStringLiteral("A"), VehicleType::TruckMedium, t0).status,
             EnterStatus::Parked);
    // B 排队 2 小时后才补位
    QCOMPARE(lot.enter(QStringLiteral("B"), VehicleType::CarSmall, t0 + 60'000).status,
             EnterStatus::Queued);
    QCOMPARE(lot.waiting.size(), 1);
    QVERIFY(lot.validate());

    const ExitResult r = lot.leave(QStringLiteral("A"), t0 + 2 * 3'600'000);
    QCOMPARE(r.status, ExitStatus::Ok);
    QCOMPARE(r.feeCents, qint64(2 * 500));  // 2 小时 × ¥5
    QVERIFY(r.promoted);
    QCOMPARE(r.promotedPlate, QStringLiteral("B"));
    QCOMPARE(r.promotedEnteredAtMs, t0 + 2 * 3'600'000);
    QCOMPARE(lot.occupyCount(), 1);
    QCOMPARE(lot.waiting.size(), 0);

    // 决策 2：排队时长不计费，B 从补位时刻起算 1 小时 → ¥5
    const ExitResult rb = lot.leave(QStringLiteral("B"), t0 + 3 * 3'600'000);
    QCOMPARE(rb.status, ExitStatus::Ok);
    QCOMPARE(rb.stayMs, qint64(3'600'000));
    QCOMPARE(rb.feeCents, qint64(500));
    QCOMPARE(lot.revenueCents, qint64(1500));
    QVERIFY(lot.validate());
}

void TestDomain::queueLimit()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 1;
    cfg.maxWaiting = 1;
    lot.reset(cfg);

    QCOMPARE(lot.enter(QStringLiteral("A"), VehicleType::CarSmall, 1).status, EnterStatus::Parked);
    QCOMPARE(lot.enter(QStringLiteral("B"), VehicleType::CarSmall, 2).status, EnterStatus::Queued);
    QCOMPARE(lot.enter(QStringLiteral("C"), VehicleType::CarSmall, 3).status, EnterStatus::QueueFull);
    QVERIFY(lot.validate());
}

void TestDomain::feeBoundaries_data()
{
    QTest::addColumn<qint64>("stayMs");
    QTest::addColumn<qint64>("expectedCents");

    QTest::newRow("0 分钟") << qint64(0) << qint64(0);
    QTest::newRow("5 分钟") << qint64(5 * 60'000) << qint64(0);
    QTest::newRow("15 分钟整（免费上限）") << qint64(15 * 60'000) << qint64(0);
    QTest::newRow("15 分 01 毫秒（刚超免费）") << qint64(15 * 60'000 + 1) << qint64(500);
    QTest::newRow("75 分钟（1 个计费单位）") << qint64(75 * 60'000) << qint64(500);
    QTest::newRow("75 分 01 毫秒（进位到 2 单位）") << qint64(75 * 60'000 + 1) << qint64(1000);
    QTest::newRow("135 分钟（2 单位）") << qint64(135 * 60'000) << qint64(1000);
    QTest::newRow("135 分 01 秒（进位到 3 单位）") << qint64(135 * 60'000 + 1000) << qint64(1500);
}

void TestDomain::feeBoundaries()
{
    QFETCH(qint64, stayMs);
    QFETCH(qint64, expectedCents);

    Config cfg;
    cfg.freeMinutes = 15;
    cfg.billingUnitMinutes = 60;
    cfg.unitPriceCents = 500;

    const qint64 entered = 1'000'000;
    qint64 billable = -1;
    const qint64 fee = feeCentsFor(entered, entered + stayMs, cfg, &billable);
    QCOMPARE(fee, expectedCents);
    QVERIFY(billable >= 0);

    // 免费时长内不产生金额
    if (stayMs <= 15 * 60'000)
        QCOMPARE(billable, qint64(0));
}

void TestDomain::leaveNotFound()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 1;
    lot.reset(cfg);

    QCOMPARE(lot.leave(QStringLiteral("X"), 1).status, ExitStatus::NotFound);
    lot.enter(QStringLiteral("A"), VehicleType::CarSmall, 1);
    lot.enter(QStringLiteral("B"), VehicleType::CarSmall, 2);
    QCOMPARE(lot.leave(QStringLiteral("B"), 3).status, ExitStatus::StillWaiting);
    // 排队车辆不能通过出场操作被移除
    QCOMPARE(lot.waiting.size(), 1);
}

void TestDomain::invariantsHold()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 20;
    cfg.maxWaiting = 5;
    lot.reset(cfg);

    qint64 t = 1'000'000;
    for (int i = 0; i < 30; ++i) {
        lot.enter(QStringLiteral("P%1").arg(i), allTypes().at(i % 4), t += 1000);
        QVERIFY(lot.validate());
    }
    QCOMPARE(lot.occupyCount(), 20);
    QCOMPARE(lot.waiting.size(), 5);

    for (int i = 0; i < 12; ++i) {
        const ExitResult r = lot.leave(QStringLiteral("P%1").arg(i), t += 3'600'000);
        QCOMPARE(r.status, ExitStatus::Ok);
        QVERIFY(r.promoted == (i < 5));
        QVERIFY(lot.validate());
    }
    // 前 5 次出场各补位 1 辆（队列清空）；后 7 次纯出场
    QCOMPARE(lot.occupyCount(), 13);
    QCOMPARE(lot.waiting.size(), 0);
    QCOMPARE(lot.totalEntered, qint64(25));  // 20 入位 + 5 入队（其余 5 辆被队列上限拒绝）
}

void TestDomain::invariantsDetectCorruption()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 2;
    lot.reset(cfg);
    lot.enter(QStringLiteral("A"), VehicleType::CarSmall, 1);

    lot.slotPlate[0] = QStringLiteral("B");  // 人为破坏派生索引
    QVERIFY(!lot.validate());
}

void TestDomain::layoutGeometry()
{
    const Layout l120 = makeLayout(120);
    QCOMPARE(l120.cols, 14);
    QCOMPARE(l120.rows, 9);
    QCOMPARE(l120.slotRects.size(), 120);

    const Layout l60 = makeLayout(60);
    QCOMPARE(l60.cols, 10);
    QCOMPARE(l60.rows, 6);

    const Layout l1000 = makeLayout(1000);
    QCOMPARE(l1000.cols, 30);
    QCOMPARE(l1000.rows, 34);
    QCOMPARE(l1000.slotRects.size(), 1000);
    QVERIFY(l1000.widthPx() > 0);
    QVERIFY(l1000.heightPx() > 0);

    // 车位矩形不重叠，且包含在画布内
    for (int i = 0; i < l120.slotRects.size(); ++i) {
        const QRect a = l120.slotRects.at(i).rect();
        QVERIFY(a.width() > 0 && a.height() > 0);
        QVERIFY(l120.slotRects.at(i).x + l120.slotRects.at(i).w <= l120.widthPx());
        QVERIFY(l120.slotRects.at(i).y + l120.slotRects.at(i).h <= l120.heightPx());
        for (int j = i + 1; j < l120.slotRects.size(); ++j)
            QVERIFY(!a.intersects(l120.slotRects.at(j).rect()));
    }

    // 容量 1 也能正常布局
    const Layout tiny = makeLayout(1);
    QCOMPARE(tiny.slotRects.size(), 1);
    QCOMPARE(tiny.cols, 1);
}

void TestDomain::capacityChange()
{
    Lot lot;
    Config cfg;
    cfg.capacity = 4;
    lot.reset(cfg);
    lot.enter(QStringLiteral("A"), VehicleType::CarSmall, 1);

    QString err;
    QVERIFY(!lot.changeCapacity(10, &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(lot.cfg.capacity, 4);

    lot.leave(QStringLiteral("A"), 2);
    QVERIFY(lot.changeCapacity(10, &err));
    QCOMPARE(lot.cfg.capacity, 10);
    QCOMPARE(lot.slotPlate.size(), 10);
    QCOMPARE(static_cast<int>(lot.freeSlots.size()), 10);
    QVERIFY(lot.validate());
}

void TestDomain::formatting()
{
    QCOMPARE(formatMoney(500), QStringLiteral("¥5.00"));
    QCOMPARE(formatMoney(0), QStringLiteral("¥0.00"));
    QCOMPARE(formatMoney(12345), QStringLiteral("¥123.45"));
    QCOMPARE(formatMoney(-500), QStringLiteral("-¥5.00"));

    QCOMPARE(formatDuration(30'000), QStringLiteral("不足 1 分钟"));
    QCOMPARE(formatDuration(120'000), QStringLiteral("2 分钟"));
    QCOMPARE(formatDuration(3'900'000), QStringLiteral("1 小时 05 分"));
    QCOMPARE(formatDuration(2 * 3'600'000), QStringLiteral("2 小时 00 分"));

    QCOMPARE(typeLabel(VehicleType::TruckLarge), QStringLiteral("大卡"));
    QCOMPARE(typeToken(VehicleType::TruckMedium), QStringLiteral("truck_medium"));
    VehicleType t = VehicleType::CarSmall;
    QVERIFY(typeFromToken(QStringLiteral("truck_small"), &t));
    QCOMPARE(t, VehicleType::TruckSmall);
    QVERIFY(!typeFromToken(QStringLiteral("rocket"), &t));
}

QTEST_GUILESS_MAIN(TestDomain)

#include "test_domain.moc"
