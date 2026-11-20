#include "entry_exit_simulator.h"

#include <QStringList>
#include <QTest>
#include <QVector>

using namespace pl::devices;

namespace {

// 把设备时序压到毫秒级，让测试在几十毫秒内跑完
EntryExitSimulator::Options fastOptions(bool requireExitApproval = false)
{
    EntryExitSimulator::Options options;
    options.entryProfile.recognitionDelayMs = 1;
    options.entryProfile.recognitionJitterMs = 0;
    options.entryProfile.recognitionFailureRate = 0.0;
    options.entryProfile.barrierOpenMs = 2;
    options.entryProfile.barrierCloseMs = 2;
    options.entryProfile.vehiclePassMs = 5;

    options.exitProfile = options.entryProfile;
    options.requireExitApproval = requireExitApproval;
    options.seed = 20251120;
    return options;
}

} // namespace

class TestInterface : public QObject
{
    Q_OBJECT

private slots:
    void plateNormalization();
    void entryFlow();
    void lowConfidenceRejected();
    void barrierJamReported();
    void exitRequiresApproval();
    void exitAutoApproval();
    void timestampsFollowInjectedClock();
    void counterTracksPassages();
    void duplicateEntryRejectedWhileBusy();
};

void TestInterface::plateNormalization()
{
    QCOMPARE(normalizePlate(QStringLiteral(" 鄂a·12345 ")), QStringLiteral("鄂A12345"));
    QCOMPARE(normalizePlate(QStringLiteral("京Ｂ－00001")), QStringLiteral("京B00001"));
    QVERIFY(isPlausiblePlate(QStringLiteral("鄂A12345")));
    QVERIFY(!isPlausiblePlate(QStringLiteral("A1")));
    QVERIFY(!isPlausiblePlate(QString()));
    QCOMPARE(makeDemoPlate(7), QStringLiteral("鲁A00007"));
    QCOMPARE(makeDemoPlate(7), makeDemoPlate(7));

    CarIOError error(DeviceError::BarrierJam, QStringLiteral("道闸卡死"), 1'700'000'000'000);
    QVERIFY(error.hasError());
    QCOMPARE(error.timeLog().size(), 6);
    QVERIFY(error.toString().contains(QStringLiteral("道闸卡死")));
}

void TestInterface::entryFlow()
{
    EntryExitSimulator sim(fastOptions());

    QVector<PassageEvent> entered;
    QStringList display;
    connect(&sim, &EntryExitSimulator::entered, this,
            [&entered](const PassageEvent& event) { entered.append(event); });
    connect(&sim, &EntryExitSimulator::displayChanged, this,
            [&display](LaneKind lane, const QString& text) {
                if (lane == LaneKind::Entry)
                    display.append(text);
            });

    sim.simulateEntry(QStringLiteral(" 鄂a-12345 "), 0.95, 0);

    QTRY_VERIFY_WITH_TIMEOUT(entered.size() == 1, 3000);
    const PassageEvent event = entered.first();
    QCOMPARE(event.plate, QStringLiteral("鄂A12345"));
    QVERIFY(event.lane == LaneKind::Entry);
    QCOMPARE(event.vehicleCategory, 0);
    QVERIFY(event.recognizedAtMs > 0);
    QVERIFY(event.passedAtMs >= event.recognizedAtMs);

    QTRY_VERIFY_WITH_TIMEOUT(sim.entryController()->state() == LaneState::Idle, 3000);
    QVERIFY(sim.entryBarrier()->state() == IBarrier::State::Closed);
    QVERIFY(display.join(QLatin1Char(' ')).contains(QStringLiteral("鄂A12345")));

    // 业务层拿到票据号后补打小票
    QVERIFY(sim.entryMonitor()->printTicket(QStringLiteral("T-1")));
    QCOMPARE(sim.entryPrinter()->printCount(), 1);
    QCOMPARE(sim.entryPrinter()->lastTicket(), QStringLiteral("T-1"));
    QCOMPARE(sim.entryPrinter()->lastPlate(), QStringLiteral("鄂A12345"));
}

void TestInterface::lowConfidenceRejected()
{
    EntryExitSimulator sim(fastOptions());

    QVector<DeviceError> errors;
    QVector<PassageEvent> entered;
    connect(&sim, &EntryExitSimulator::deviceError, this,
            [&errors](DeviceError error, const QString&) { errors.append(error); });
    connect(&sim, &EntryExitSimulator::entered, this,
            [&entered](const PassageEvent& event) { entered.append(event); });

    sim.simulateEntry(QStringLiteral("鄂A00001"), 0.10, 0);

    QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 3000);
    QVERIFY(errors.contains(DeviceError::LowConfidence));
    QVERIFY(entered.isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!sim.isEntryBusy(), 3000);
    QTest::qWait(100);
    QVERIFY(entered.isEmpty());
}

void TestInterface::barrierJamReported()
{
    EntryExitSimulator sim(fastOptions());
    sim.entryBarrier()->injectJam(true);

    QVector<DeviceError> errors;
    QVector<PassageEvent> entered;
    connect(&sim, &EntryExitSimulator::deviceError, this,
            [&errors](DeviceError error, const QString&) { errors.append(error); });
    connect(&sim, &EntryExitSimulator::entered, this,
            [&entered](const PassageEvent& event) { entered.append(event); });

    sim.simulateEntry(QStringLiteral("鄂A00002"), 0.99, 0);

    QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 3000);
    QVERIFY(errors.contains(DeviceError::BarrierJam));
    QVERIFY(entered.isEmpty());
}

void TestInterface::exitRequiresApproval()
{
    EntryExitSimulator sim(fastOptions(true));

    int waiting = 0;
    QVector<PassageEvent> exited;
    connect(&sim, &EntryExitSimulator::waitingForExitApproval, this, [&waiting]() { ++waiting; });
    connect(&sim, &EntryExitSimulator::exited, this,
            [&exited](const PassageEvent& event) { exited.append(event); });

    sim.simulateExit(QStringLiteral("京B00001"), 0.99, 0);

    QTRY_COMPARE_WITH_TIMEOUT(waiting, 1, 3000);
    QVERIFY(sim.exitController()->state() == LaneState::AwaitingApproval);
    QVERIFY(exited.isEmpty());

    QVERIFY(sim.approveExit());
    QTRY_VERIFY_WITH_TIMEOUT(exited.size() == 1, 3000);
    QCOMPARE(exited.first().plate, QStringLiteral("京B00001"));
    QVERIFY(exited.first().lane == LaneKind::Exit);
}

void TestInterface::exitAutoApproval()
{
    EntryExitSimulator sim(fastOptions(false));

    QVector<PassageEvent> exited;
    connect(&sim, &EntryExitSimulator::exited, this,
            [&exited](const PassageEvent& event) { exited.append(event); });

    sim.simulateExit(QStringLiteral("京B00002"), 0.99, 0);

    QTRY_VERIFY_WITH_TIMEOUT(exited.size() == 1, 3000);
    QCOMPARE(exited.first().plate, QStringLiteral("京B00002"));
}

void TestInterface::timestampsFollowInjectedClock()
{
    ManualClock clock(1'700'000'000'000);
    EntryExitSimulator sim(fastOptions());
    sim.setClock(&clock);

    QVector<PassageEvent> entered;
    connect(&sim, &EntryExitSimulator::entered, this,
            [&entered](const PassageEvent& event) { entered.append(event); });

    sim.simulateEntry(QStringLiteral("鄂A00003"), 0.99, 0);
    QTRY_VERIFY_WITH_TIMEOUT(entered.size() == 1, 3000);

    QCOMPARE(entered.first().recognizedAtMs, clock.nowMs());
    QCOMPARE(entered.first().passedAtMs, clock.nowMs());
}

void TestInterface::counterTracksPassages()
{
    EntryExitSimulator sim(fastOptions(false));

    QVector<PassageEvent> entered;
    QVector<PassageEvent> exited;
    connect(&sim, &EntryExitSimulator::entered, this,
            [&entered](const PassageEvent& event) { entered.append(event); });
    connect(&sim, &EntryExitSimulator::exited, this,
            [&exited](const PassageEvent& event) { exited.append(event); });

    sim.simulateEntry(QStringLiteral("鄂A00004"), 0.99, 0);
    QTRY_VERIFY_WITH_TIMEOUT(entered.size() == 1, 3000);

    sim.simulateExit(QStringLiteral("鄂A00004"), 0.99, 0);
    QTRY_VERIFY_WITH_TIMEOUT(exited.size() == 1, 3000);

    QCOMPARE(sim.counter()->inCount(), qint64(1));
    QCOMPARE(sim.counter()->outCount(), qint64(1));
    QCOMPARE(sim.counter()->netCount(), qint64(0));
    QVERIFY(sim.counter()->state() == CounterState::Fine);
}

void TestInterface::duplicateEntryRejectedWhileBusy()
{
    EntryExitSimulator sim(fastOptions());

    QVector<DeviceError> errors;
    QVector<PassageEvent> entered;
    connect(&sim, &EntryExitSimulator::deviceError, this,
            [&errors](DeviceError error, const QString&) { errors.append(error); });
    connect(&sim, &EntryExitSimulator::entered, this,
            [&entered](const PassageEvent& event) { entered.append(event); });

    sim.simulateEntry(QStringLiteral("鄂A00005"), 0.99, 0);
    sim.simulateEntry(QStringLiteral("鄂A00006"), 0.99, 0);   // 车道忙，应被拒绝

    QVERIFY(errors.contains(DeviceError::Busy));
    QTRY_VERIFY_WITH_TIMEOUT(entered.size() == 1, 3000);
    QCOMPARE(entered.first().plate, QStringLiteral("鄂A00005"));
}

QTEST_GUILESS_MAIN(TestInterface)

#include "test_interface.moc"
