#pragma once
// ---------------------------------------------------------------------------
// statspage.h — 统计报表：车型分布、月度收入、历史流水导出
// ---------------------------------------------------------------------------
#include "ui/page.h"

class QLabel;
class QProgressBar;
class QTableWidget;

namespace app {
class AppContext;
}

namespace ui {

class KpiCard;
class RevenueChart;

class StatsPage : public Page
{
    Q_OBJECT

public:
    explicit StatsPage(app::AppContext* ctx, QWidget* parent = nullptr);

    void refresh() override;

private:
    void exportCsv();

    app::AppContext* m_ctx = nullptr;
    KpiCard* m_kpiEntered = nullptr;
    KpiCard* m_kpiExited = nullptr;
    KpiCard* m_kpiRevenue = nullptr;
    KpiCard* m_kpiHistory = nullptr;

    QProgressBar* m_bars[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel* m_counts[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel* m_totalLabel = nullptr;

    QTableWidget* m_recentTable = nullptr;
    RevenueChart* m_chart = nullptr;
    QLabel* m_monthTotal = nullptr;
};

} // namespace ui
