#pragma once
// ---------------------------------------------------------------------------
// devicepage.h — 自动识别页：入口/出口设备状态 + 模拟来车 + 识别流水
//
// 自动识别与人工登记共用 AppContext：本页只负责驱动模拟设备并展示结果，
// 真正的入库/出场仍走 AppContext，因此两套入口可以同时工作。
// ---------------------------------------------------------------------------
#include "interface/device_types.h"
#include "ui/page.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace app {
class AppContext;
class DeviceBridge;
}

namespace ui {

class Card;

class DevicePage : public Page
{
    Q_OBJECT

public:
    explicit DevicePage(app::AppContext* ctx, app::DeviceBridge* bridge,
                        QWidget* parent = nullptr);

    void refresh() override;
    void tick() override;

private:
    void buildLaneCards();
    void appendLog(const QString& lane, const QString& plate, const QString& result,
                   const QString& amount, const QString& detail);
    void updateLaneUi();
    void updateListeningUi();
    void updateCountersUi();

    app::AppContext* m_ctx = nullptr;
    app::DeviceBridge* m_bridge = nullptr;

    Card* m_entryCard = nullptr;
    Card* m_exitCard = nullptr;

    QLabel* m_entryState = nullptr;
    QLabel* m_entryDisplay = nullptr;
    QLabel* m_entryLast = nullptr;
    QLineEdit* m_entryPlate = nullptr;
    QComboBox* m_entryType = nullptr;
    QPushButton* m_entryButton = nullptr;

    QLabel* m_exitState = nullptr;
    QLabel* m_exitDisplay = nullptr;
    QLabel* m_exitLast = nullptr;
    QLineEdit* m_exitPlate = nullptr;
    QPushButton* m_exitButton = nullptr;

    QTableWidget* m_logTable = nullptr;
    QLabel* m_counterLabel = nullptr;
    QLabel* m_statsLabel = nullptr;
    QPushButton* m_listenButton = nullptr;

    int m_autoIn = 0;
    int m_autoOut = 0;
    int m_autoErrors = 0;
};

} // namespace ui
