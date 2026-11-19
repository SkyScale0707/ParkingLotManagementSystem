#pragma once
// ---------------------------------------------------------------------------
// overviewpage.h — 总览：KPI + 2D 车位视图 + 等待队列 + 最近出场
// ---------------------------------------------------------------------------
#include "ui/page.h"

#include <QWidget>

class QLabel;
class QTableView;
class QTableWidget;

namespace app {
class AppContext;
}

namespace ui {

class KpiCard;
class LotView;
class VehicleListModel;

class OverviewPage : public Page
{
    Q_OBJECT

public:
    explicit OverviewPage(app::AppContext* ctx, QWidget* parent = nullptr);

    void refresh() override;
    void tick() override;

private:
    void onSlotActivated(int slot);
    void updateRecent();

    app::AppContext* m_ctx = nullptr;
    KpiCard* m_kpiInside = nullptr;
    KpiCard* m_kpiEmpty = nullptr;
    KpiCard* m_kpiQueue = nullptr;
    KpiCard* m_kpiRevenue = nullptr;
    LotView* m_view = nullptr;
    QLabel* m_slotDetail = nullptr;
    QLabel* m_queueHint = nullptr;
    VehicleListModel* m_queueModel = nullptr;
    QTableView* m_queueTable = nullptr;
    QTableWidget* m_recentTable = nullptr;
    int m_selectedSlot = -1;
};

} // namespace ui
