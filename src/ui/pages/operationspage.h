#pragma once
// ---------------------------------------------------------------------------
// operationspage.h — 停车操作：入库 / 出场结算入口 + 业务规则摘要
// ---------------------------------------------------------------------------
#include "ui/page.h"

class QLabel;

namespace app {
class AppContext;
}

namespace ui {

class OperationsPage : public Page
{
    Q_OBJECT

public:
    explicit OperationsPage(app::AppContext* ctx, QWidget* parent = nullptr);

    void refresh() override;

private:
    void openEnterDialog();
    void openExitDialog();

    app::AppContext* m_ctx = nullptr;
    QLabel* m_insideLabel = nullptr;
    QLabel* m_queueLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_ruleLabel = nullptr;
    QLabel* m_ruleFree = nullptr;
    QLabel* m_ruleUnit = nullptr;
    QLabel* m_rulePrice = nullptr;
    QLabel* m_ruleMaxWaiting = nullptr;
};

} // namespace ui
