#pragma once
// ---------------------------------------------------------------------------
// searchpage.h — 车辆查询（Model/View + 代理过滤）
// ---------------------------------------------------------------------------
#include "ui/page.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;

namespace app {
class AppContext;
}

namespace ui {

class VehicleFilterProxy;
class VehicleListModel;

class SearchPage : public Page
{
    Q_OBJECT

public:
    explicit SearchPage(app::AppContext* ctx, QWidget* parent = nullptr);

    void refresh() override;
    void tick() override;

private:
    void updateCount();
    void showDetail(const QModelIndex& proxyIndex);

    app::AppContext* m_ctx = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_typeFilter = nullptr;
    QComboBox* m_statusFilter = nullptr;
    QTableView* m_table = nullptr;
    VehicleListModel* m_model = nullptr;
    VehicleFilterProxy* m_proxy = nullptr;
    QLabel* m_count = nullptr;
};

} // namespace ui
