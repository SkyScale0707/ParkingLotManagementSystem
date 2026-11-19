#include "app/appcontext.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QTemporaryDir>
#include <QTest>

// UI 冒烟测试：构造主窗口、切换 5 个页面、确认渲染不崩溃。
// 通过 ctest 设置 QT_QPA_PLATFORM=offscreen 运行，无需真实显示器。
class TestUi : public QObject
{
    Q_OBJECT

private slots:
    void mainWindowShowsAllPages();
    void pagesRenderWithEmptyLot();
};

void TestUi::mainWindowShowsAllPages()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    app::AppContext ctx(dir.path());
    QString error;
    QVERIFY2(ctx.open(&error), qPrintable(error));
    QVERIFY2(ctx.seedDemo(&error), qPrintable(error));

    ui::MainWindow window(&ctx);
    window.resize(1280, 800);
    window.show();
    QTest::qWait(50);

    QCOMPARE(window.pageCount(), 5);
    for (int i = 0; i < window.pageCount(); ++i) {
        window.showPage(i);
        QTest::qWait(10);
        const QPixmap shot = window.grab();
        QVERIFY(!shot.isNull());
        QVERIFY(shot.width() > 800);
    }
}

void TestUi::pagesRenderWithEmptyLot()
{
    QTemporaryDir dir;
    app::AppContext ctx(dir.path());
    QString error;
    QVERIFY2(ctx.open(&error), qPrintable(error));

    ui::MainWindow window(&ctx);
    window.resize(1120, 720);
    window.show();
    QTest::qWait(50);

    for (int i = 0; i < window.pageCount(); ++i) {
        window.showPage(i);
        QTest::qWait(10);
        QVERIFY(!window.grab().isNull());
    }
}

QTEST_MAIN(TestUi)

#include "test_ui.moc"
