#pragma once
// ---------------------------------------------------------------------------
// exitdialog.h — 出场结算对话框
// ---------------------------------------------------------------------------
#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

namespace app {
class AppContext;
}

namespace ui {

class ExitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExitDialog(app::AppContext* ctx, QWidget* parent = nullptr);

    // 供总览页/快捷入口预填车牌并立即查询
    void setPlateAndLookup(const QString& plate);

private:
    void lookup();
    void settle();
    void resetDetails();
    void setFeedback(const QString& text, bool warn);

    app::AppContext* m_ctx = nullptr;
    QLineEdit* m_plate = nullptr;
    QPushButton* m_lookupBtn = nullptr;
    QPushButton* m_settleBtn = nullptr;
    QLabel* m_plateVal = nullptr;
    QLabel* m_typeVal = nullptr;
    QLabel* m_slotVal = nullptr;
    QLabel* m_enterVal = nullptr;
    QLabel* m_stayVal = nullptr;
    QLabel* m_feeVal = nullptr;
    QLabel* m_feedback = nullptr;
    bool m_found = false;
};

} // namespace ui
