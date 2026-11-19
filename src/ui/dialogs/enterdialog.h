#pragma once
// ---------------------------------------------------------------------------
// enterdialog.h — 车辆入库对话框
// ---------------------------------------------------------------------------
#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace app {
class AppContext;
}

namespace ui {

class EnterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnterDialog(app::AppContext* ctx, QWidget* parent = nullptr);

private:
    void submit();
    void setFeedback(const QString& text, bool warn);

    app::AppContext* m_ctx = nullptr;
    QLineEdit* m_plate = nullptr;
    QComboBox* m_type = nullptr;
    QLabel* m_feedback = nullptr;
    QPushButton* m_submit = nullptr;
};

} // namespace ui
