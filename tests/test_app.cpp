#include "app/appcontext.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace pl;

namespace {

// 让 AppContext 的默认 50 位容量收敛成便于验证的小容量
bool shrinkTo(app::AppContext& ctx, int capacity, QString* err)
{
    Config cfg = ctx.config();
    cfg.capacity = capacity;
    return ctx.applyConfig(cfg, err);
}

} // namespace

class TestApp : public QObject
{
    Q_OBJECT

private slots:
    void enterExitAndReload();
    void configPersistedAcrossRestart();
    void capacityGuard();
    void exportCsv();
    void duplicateRejected();
};

void TestApp::enterExitAndReload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    {
        app::AppContext ctx(dir.path());
        QString err;
        QVERIFY2(ctx.open(&err), qPrintable(err));
        QVERIFY(!ctx.isReadOnly());
        QVERIFY(shrinkTo(ctx, 2, &err));

        const EnterResult a = ctx.enterVehicle(QStringLiteral("鄂A00001"), VehicleType::CarSmall, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(a.status, EnterStatus::Parked);

        const EnterResult b = ctx.enterVehicle(QStringLiteral("鄂A00002"), VehicleType::TruckSmall, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(b.status, EnterStatus::Parked);

        const EnterResult c =
            ctx.enterVehicle(QStringLiteral("鄂A00003"), VehicleType::TruckLarge, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.status, EnterStatus::Queued);
        QCOMPARE(ctx.lot().occupyCount(), 2);
        QCOMPARE(static_cast<int>(ctx.lot().waiting.size()), 1);

        const ExitResult r = ctx.leaveVehicle(QStringLiteral("鄂A00001"), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(r.status, ExitStatus::Ok);
        QVERIFY(r.promoted);
        QCOMPARE(r.promotedPlate, QStringLiteral("鄂A00003"));
        QCOMPARE(ctx.lot().occupyCount(), 2);
        QCOMPARE(static_cast<int>(ctx.lot().waiting.size()), 0);
        QCOMPARE(ctx.allExits().size(), 1);
        QCOMPARE(ctx.recentExits(5).size(), 1);
    }

    // 重新打开：析构时已压缩，状态应从快照恢复
    {
        app::AppContext ctx(dir.path());
        QString err;
        QVERIFY2(ctx.open(&err), qPrintable(err));
        QCOMPARE(ctx.config().capacity, 2);
        QCOMPARE(ctx.lot().occupyCount(), 2);
        QVERIFY(ctx.lot().hasPlate(QStringLiteral("鄂A00002")));
        QVERIFY(ctx.lot().hasPlate(QStringLiteral("鄂A00003")));
        QCOMPARE(ctx.allExits().size(), 1);
        QCOMPARE(ctx.lot().totalEntered, qint64(3));
        QCOMPARE(ctx.lot().totalExited, qint64(1));
        QVERIFY(ctx.lot().validate());
    }
}

void TestApp::configPersistedAcrossRestart()
{
    QTemporaryDir dir;
    {
        app::AppContext ctx(dir.path());
        QString err;
        QVERIFY2(ctx.open(&err), qPrintable(err));

        Config cfg = ctx.config();
        cfg.capacity = 8;
        cfg.unitPriceCents = 700;
        cfg.freeMinutes = 5;
        cfg.billingUnitMinutes = 30;
        cfg.maxWaiting = 12;
        QVERIFY2(ctx.applyConfig(cfg, &err), qPrintable(err));
        QCOMPARE(ctx.config().unitPriceCents, 700);
    }

    {
        app::AppContext ctx(dir.path());
        QString err;
        QVERIFY2(ctx.open(&err), qPrintable(err));
        QCOMPARE(ctx.config().capacity, 8);
        QCOMPARE(ctx.config().unitPriceCents, 700);
        QCOMPARE(ctx.config().freeMinutes, 5);
        QCOMPARE(ctx.config().billingUnitMinutes, 30);
        QCOMPARE(ctx.config().maxWaiting, 12);
        QCOMPARE(static_cast<int>(ctx.lot().freeSlots.size()), 8);
    }
}

void TestApp::capacityGuard()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString err;
    QVERIFY2(ctx.open(&err), qPrintable(err));
    QVERIFY2(ctx.enterVehicle(QStringLiteral("鄂A00001"), VehicleType::CarSmall, &err).status
                 == EnterStatus::Parked,
             qPrintable(err));

    Config cfg = ctx.config();
    cfg.capacity = 20;
    QVERIFY(!ctx.applyConfig(cfg, &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(ctx.config().capacity, 50);   // 默认容量未被修改
}

void TestApp::exportCsv()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString err;
    QVERIFY2(ctx.open(&err), qPrintable(err));

    QVERIFY2(ctx.enterVehicle(QStringLiteral("鄂A00001"), VehicleType::CarSmall, &err).status
                 == EnterStatus::Parked,
             qPrintable(err));
    QVERIFY2(ctx.leaveVehicle(QStringLiteral("鄂A00001"), &err).status == ExitStatus::Ok,
             qPrintable(err));

    const QString path = dir.filePath(QStringLiteral("out.csv"));
    QVERIFY2(ctx.exportHistoryCsv(path, &err), qPrintable(err));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray data = f.readAll();
    QVERIFY(data.startsWith("\xEF\xBB\xBF"));
    const QString text = QString::fromUtf8(data);
    QVERIFY(text.contains(QStringLiteral("车牌号")));
    QVERIFY(text.contains(QStringLiteral("鄂A00001")));
}

void TestApp::duplicateRejected()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString err;
    QVERIFY2(ctx.open(&err), qPrintable(err));

    QCOMPARE(ctx.enterVehicle(QStringLiteral("鄂A00001"), VehicleType::CarSmall, &err).status,
             EnterStatus::Parked);
    QCOMPARE(ctx.enterVehicle(QStringLiteral("鄂A00001"), VehicleType::CarSmall, &err).status,
             EnterStatus::DuplicateActive);
    QCOMPARE(ctx.enterVehicle(QString(), VehicleType::CarSmall, &err).status,
             EnterStatus::InvalidPlate);
}

QTEST_GUILESS_MAIN(TestApp)

#include "test_app.moc"
