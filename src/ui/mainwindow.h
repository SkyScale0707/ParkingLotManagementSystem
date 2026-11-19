#pragma once
// ---------------------------------------------------------------------------
// mainwindow.h — 左侧导航 + QStackedWidget 主窗口
// ---------------------------------------------------------------------------
#include <QList>
#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;
class QTimer;

namespace app {
class AppContext;
}

namespace ui {

class Page;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(app::AppContext* ctx, QWidget* parent = nullptr);

    void showPage(int index);
    int pageCount() const { return m_pages.size(); }

private:
    void buildUi();
    void refreshAll();
    void updateStatus();
    void updateHeader(int index);
    void compactNow();

    app::AppContext* m_ctx = nullptr;
    QListWidget* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_statusDir = nullptr;
    QLabel* m_statusJournal = nullptr;
    QLabel* m_statusSaved = nullptr;
    QList<Page*> m_pages;
    QTimer* m_tick = nullptr;
    qint64 m_lastSaveAt = 0;
};

} // namespace ui
