#pragma once
// ---------------------------------------------------------------------------
// settingspage.h — 系统设置：容量 / 计费规则 / 数据目录与存储状态
// ---------------------------------------------------------------------------
#include "ui/page.h"

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace app {
class AppContext;
}

namespace ui {

class SettingsPage : public Page
{
    Q_OBJECT

public:
    explicit SettingsPage(app::AppContext* ctx, QWidget* parent = nullptr);

    void refresh() override;

private:
    void save();
    void compactNow();
    void exportCsv();
    void openDataDir();
    void updateStorageLabels();

    app::AppContext* m_ctx = nullptr;

    QSpinBox* m_capacity = nullptr;
    QDoubleSpinBox* m_price = nullptr;
    QSpinBox* m_free = nullptr;
    QSpinBox* m_unit = nullptr;
    QSpinBox* m_maxWaiting = nullptr;
    QLineEdit* m_dir = nullptr;
    QLabel* m_feedback = nullptr;
    QLabel* m_example = nullptr;
    QLabel* m_readOnly = nullptr;
    QPushButton* m_saveBtn = nullptr;

    QLabel* m_statSeq = nullptr;
    QLabel* m_statSnapshot = nullptr;
    QLabel* m_statGens = nullptr;
    QLabel* m_statJournal = nullptr;
    QLabel* m_statSize = nullptr;
    QLabel* m_statHistory = nullptr;
};

} // namespace ui
