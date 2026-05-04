#pragma comment(lib, "OpenGL32.lib")

#include "QtVsMainwindow_ParkingLotManagementSystem_b1.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QDateTime>
#include <QGroupBox>

#include <QFont>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QIntValidator>

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setStyle(QStyleFactory::create("Fusion"));  // 圆润核心样式
    app.setFont(QFont("Microsoft YaHei", 9));       // 全局统一字体

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& locale : uiLanguages) {
        const QString baseName = "QtProjectChange-Parking_Lot_Management_System_"
            + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }
    MainWindow window;
    window.show();
    return app.exec();
}
