#include "app/appcontext.h"
#include "app/devicebridge.h"

#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

using namespace pl;

namespace {

// 把设备时序压到毫秒级，让集成测试快速跑完
devices::EntryExitSimulator::Options fastOptions()
{
    devices::EntryExitSimulator::Options options;
    options.entryProfile.recognitionDelayMs = 1;
    options.entryProfile.recognitionJitterMs = 0;
    options.entryProfile.recognitionFailureRate = 0.0;
    options.entryProfile.barrierOpenMs = 2;
    options.entryProfile.barrierCloseMs = 2;
    options.entryProfile.vehiclePassMs = 5;

    options.exitProfile = options.entryProfile;
    options.seed = 20251120;
    return options;
}

} // namespace

class TestBridge : public QObject
{
    Q_OBJECT

private slots:
    void autoEntryThenAutoExit();
    void autoExitUnknownPlateRejected();
    void manualAndAutoShareData();
};

void TestBridge::autoEntryThenAutoExit()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString error;
    QVERIFY2(ctx.open(&error), qPrintable(error));

    app::DeviceBridge bridge(&ctx, fastOptions());

    QString enteredPlate;
    QString ticket;
    QStringList errors;
    connect(&bridge, &app::DeviceBridge::autoEntered, this,
            [&enteredPlate, &ticket](const QString& plate, const QString& issued,
                                     const QString&) {
                enteredPlate = plate;
                ticket = issued;
            });
    connect(&bridge, &app::DeviceBridge::autoError, this,
            [&errors](const QString& message) { errors.append(message); });

    bridge.requestEntry(QStringLiteral(" 鄂a-12345 "), 0);

    QTRY_VERIFY_WITH_TIMEOUT(!enteredPlate.isEmpty(), 3000);
    QCOMPARE(enteredPlate, QStringLiteral("鄂A12345"));
    QVERIFY(!ticket.isEmpty());
    QVERIFY(ctx.lot().hasPlate(QStringLiteral("鄂A12345")));
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QStringLiteral(" / "))));
    QCOMPARE(bridge.simulator()->entryPrinter()->printCount(), 1);
    QCOMPARE(bridge.simulator()->entryPrinter()->lastPlate(), QStringLiteral("鄂A12345"));

    QString exitedPlate;
    qint64 feeCents = -1;
    connect(&bridge, &app::DeviceBridge::autoExited, this,
            [&exitedPlate, &feeCents](const QString& plate, qint64 fee, const QString&) {
                exitedPlate = plate;
                feeCents = fee;
            });

    bridge.requestExit(QStringLiteral("鄂A12345"));

    QTRY_VERIFY_WITH_TIMEOUT(!exitedPlate.isEmpty(), 3000);
    QCOMPARE(exitedPlate, QStringLiteral("鄂A12345"));
    QVERIFY(feeCents >= 0);
    QVERIFY(!ctx.lot().hasPlate(QStringLiteral("鄂A12345")));
    QCOMPARE(ctx.lot().totalExited, qint64(1));
}

void TestBridge::autoExitUnknownPlateRejected()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString error;
    QVERIFY2(ctx.open(&error), qPrintable(error));

    app::DeviceBridge bridge(&ctx, fastOptions());

    QString rejectedPlate;
    QString reason;
    connect(&bridge, &app::DeviceBridge::autoRejected, this,
            [&rejectedPlate, &reason](const QString& plate, const QString& why) {
                rejectedPlate = plate;
                reason = why;
            });

    bridge.requestExit(QStringLiteral("京C99999"));

    QTRY_VERIFY_WITH_TIMEOUT(!rejectedPlate.isEmpty(), 3000);
    QCOMPARE(rejectedPlate, QStringLiteral("京C99999"));
    QVERIFY(!reason.isEmpty());
    QVERIFY(ctx.lot().active.isEmpty());
    QVERIFY(!ctx.lot().hasPlate(QStringLiteral("京C99999")));
}

void TestBridge::manualAndAutoShareData()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString error;
    QVERIFY2(ctx.open(&error), qPrintable(error));

    app::DeviceBridge bridge(&ctx, fastOptions());

    // 人工入库 → 自动识别出场
    const EnterResult manual =
        ctx.enterVehicle(QStringLiteral("鄂A22222"), VehicleType::CarSmall, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(manual.status == EnterStatus::Parked);

    QString exitedPlate;
    connect(&bridge, &app::DeviceBridge::autoExited, this,
            [&exitedPlate](const QString& plate, qint64, const QString&) {
                exitedPlate = plate;
            });

    bridge.requestExit(QStringLiteral("鄂A22222"));
    QTRY_VERIFY_WITH_TIMEOUT(!exitedPlate.isEmpty(), 3000);
    QCOMPARE(exitedPlate, QStringLiteral("鄂A22222"));
    QVERIFY(!ctx.lot().hasPlate(QStringLiteral("鄂A22222")));

    // 自动识别入场 → 人工结算出场
    bridge.requestEntry(QStringLiteral("鄂A33333"), 1);
    QTRY_VERIFY_WITH_TIMEOUT(ctx.lot().hasPlate(QStringLiteral("鄂A33333")), 3000);

    const ExitResult manualExit = ctx.leaveVehicle(QStringLiteral("鄂A33333"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(manualExit.status == ExitStatus::Ok);
    QVERIFY(!ctx.lot().hasPlate(QStringLiteral("鄂A33333")));
}

QTEST_GUILESS_MAIN(TestBridge)

#include "test_bridge.moc"
