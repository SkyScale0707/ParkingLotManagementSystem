#include "app/appcontext.h"
#include "ui/dialogs/enterdialog.h"
#include "ui/dialogs/exitdialog.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("HUST"));
    QApplication::setApplicationName(QStringLiteral("ParkingLotManagementSystem"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("停车场管理系统"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    app.setStyle(QStringLiteral("Fusion"));

    // 显式加载系统中文字体：即使使用 offscreen 等平台插件，也能正确渲染中文
    for (const QString& path : {QStringLiteral("C:/Windows/Fonts/msyh.ttc"),
                                QStringLiteral("C:/Windows/Fonts/msyhbd.ttc"),
                                QStringLiteral("C:/Windows/Fonts/simhei.ttf")}) {
        if (QFile::exists(path))
            QFontDatabase::addApplicationFont(path);
    }
    const QStringList families = QFontDatabase::families();
    QString family = QStringLiteral("Microsoft YaHei");
    if (!families.contains(family)) {
        family = families.contains(QStringLiteral("Microsoft YaHei UI"))
            ? QStringLiteral("Microsoft YaHei UI")
            : QStringLiteral("SimHei");
    }
    app.setFont(QFont(family, 9));
    app.setStyleSheet(ui::theme::styleSheet());

    // Qt 自带控件文案（如对话框按钮）跟随系统语言
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("停车场管理系统（Qt 6）"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dataDirOpt(
        QStringList{QStringLiteral("d"), QStringLiteral("data-dir")},
        QStringLiteral("指定数据目录（默认：可执行文件旁的 data/）"), QStringLiteral("dir"));
    QCommandLineOption demoOpt(QStringLiteral("demo"),
                               QStringLiteral("向空库注入演示数据"));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                               QStringLiteral("把各页面渲染为 PNG 到指定目录后退出"),
                               QStringLiteral("dir"));
    QCommandLineOption smokeOpt(QStringLiteral("smoke"),
                                QStringLiteral("启动后自动退出（冒烟测试）"));
    parser.addOptions({dataDirOpt, demoOpt, shotOpt, smokeOpt});
    parser.process(app);

    QString dataDir = parser.value(dataDirOpt);
    const QString shotDir = parser.value(shotOpt);
    if (dataDir.isEmpty()) {
        if (!shotDir.isEmpty()) {
            // 截图使用固定名字的临时目录，避免状态栏出现随机路径
            const QString demoDir = QDir(QDir::tempPath()).filePath(QStringLiteral("ParkingLotDemo"));
            QDir(demoDir).removeRecursively();
            dataDir = demoDir;
        } else {
            // 优先使用「便携模式」：数据放在 exe 旁；安装目录不可写时退回 AppData
            const QString portable = QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QStringLiteral("data"));
            const bool writable = [&portable]() {
                if (!QDir().mkpath(portable))
                    return false;
                QFile probe(QDir(portable).filePath(QStringLiteral(".write-probe")));
                if (!probe.open(QIODevice::WriteOnly))
                    return false;
                probe.close();
                probe.remove();
                return true;
            }();
            dataDir = writable
                ? portable
                : QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                      .filePath(QStringLiteral("data"));
        }
    }

    app::AppContext ctx(dataDir);
    QString error;
    if (!ctx.open(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("无法打开数据目录"), error);
        return 1;
    }

    if (parser.isSet(demoOpt) || !shotDir.isEmpty()) {
        QString seedError;
        if (!ctx.seedDemo(&seedError) && !seedError.contains(QStringLiteral("非空")))
            qWarning("演示数据注入失败：%s", qPrintable(seedError));
    }

    if (!shotDir.isEmpty()) {
        QDir().mkpath(shotDir);
        ui::MainWindow window(&ctx);
        window.resize(1440, 900);
        window.show();
        QApplication::processEvents();

        const QStringList names{QStringLiteral("overview"), QStringLiteral("operations"),
                                QStringLiteral("search"), QStringLiteral("stats"),
                                QStringLiteral("settings")};
        for (int i = 0; i < window.pageCount() && i < names.size(); ++i) {
            window.showPage(i);
            QApplication::processEvents();
            QApplication::processEvents();
            window.grab().save(QDir(shotDir).filePath(names.at(i) + QStringLiteral(".png")));
        }

        {
            ui::EnterDialog dialog(&ctx);
            dialog.show();
            QApplication::processEvents();
            dialog.grab().save(QDir(shotDir).filePath(QStringLiteral("dialog-enter.png")));
            dialog.close();
        }
        {
            ui::ExitDialog dialog(&ctx);
            dialog.show();
            QApplication::processEvents();
            if (!ctx.lot().active.isEmpty())
                dialog.setPlateAndLookup(ctx.lot().parkedBySlot().first().plate);
            QApplication::processEvents();
            dialog.grab().save(QDir(shotDir).filePath(QStringLiteral("dialog-exit.png")));
            dialog.close();
        }
        return 0;
    }

    ui::MainWindow window(&ctx);
    window.show();

    if (parser.isSet(smokeOpt))
        QTimer::singleShot(1200, &app, &QCoreApplication::quit);

    return app.exec();
}
