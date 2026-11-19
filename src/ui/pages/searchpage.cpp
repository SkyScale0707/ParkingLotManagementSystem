#include "ui/pages/searchpage.h"

#include "app/appcontext.h"
#include "domain/billing.h"
#include "ui/theme.h"
#include "ui/vehiclelistmodel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

namespace ui {

class VehicleFilterProxy : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setPlateFilter(const QString& text)
    {
        beginFilterChange();
        m_plate = text.trimmed().toUpper();
        endFilterChange();
    }

    void setTypeFilter(int type)
    {
        beginFilterChange();
        m_type = type;
        endFilterChange();
    }

    // 0 = 全部, 1 = 在场, 2 = 排队
    void setStatusFilter(int status)
    {
        beginFilterChange();
        m_status = status;
        endFilterChange();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        auto* model = qobject_cast<VehicleListModel*>(sourceModel());
        if (!model)
            return false;

        const QModelIndex sourceIndex = model->index(row, VehicleListModel::Plate, parent);
        if (!sourceIndex.isValid())
            return false;
        const VehicleRow& r = model->rowAt(sourceIndex.row());

        if (!m_plate.isEmpty() && !r.plate.contains(m_plate, Qt::CaseInsensitive))
            return false;
        if (m_type >= 0 && static_cast<int>(r.type) != m_type)
            return false;
        if (m_status == 1 && r.waiting)
            return false;
        if (m_status == 2 && !r.waiting)
            return false;
        return true;
    }

private:
    QString m_plate;
    int m_type = -1;
    int m_status = 0;
};

SearchPage::SearchPage(app::AppContext* ctx, QWidget* parent)
    : Page(parent)
    , m_ctx(ctx)
{
    setObjectName(QStringLiteral("pageBody"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    auto* filterCard = new Card(QStringLiteral("筛选条件"), this);
    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(10);

    m_search = new QLineEdit(filterCard);
    m_search->setPlaceholderText(QStringLiteral("车牌号（支持模糊匹配）"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(220);

    m_typeFilter = new QComboBox(filterCard);
    m_typeFilter->addItem(QStringLiteral("全部车型"), -1);
    for (pl::VehicleType t : pl::allTypes())
        m_typeFilter->addItem(pl::typeLabel(t), static_cast<int>(t));

    m_statusFilter = new QComboBox(filterCard);
    m_statusFilter->addItem(QStringLiteral("全部状态"), 0);
    m_statusFilter->addItem(QStringLiteral("在场"), 1);
    m_statusFilter->addItem(QStringLiteral("排队"), 2);

    auto* resetBtn = new QPushButton(QStringLiteral("重置"), filterCard);
    resetBtn->setObjectName(QStringLiteral("ghost"));

    m_count = new QLabel(filterCard);
    m_count->setObjectName(QStringLiteral("muted"));

    filterRow->addWidget(m_search);
    filterRow->addWidget(m_typeFilter);
    filterRow->addWidget(m_statusFilter);
    filterRow->addWidget(resetBtn);
    filterRow->addStretch(1);
    filterRow->addWidget(m_count);
    filterCard->body()->addLayout(filterRow);
    root->addWidget(filterCard);

    auto* tableCard = new Card(QStringLiteral("车辆列表"), this);
    m_table = new QTableView(tableCard);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSortingEnabled(false);

    m_model = new VehicleListModel(this);
    m_proxy = new VehicleFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_table->setModel(m_proxy);

    m_table->setColumnWidth(VehicleListModel::Plate, 150);
    m_table->setColumnWidth(VehicleListModel::Type, 90);
    m_table->setColumnWidth(VehicleListModel::Slot, 80);
    m_table->setColumnWidth(VehicleListModel::State, 100);
    m_table->setColumnWidth(VehicleListModel::Since, 170);
    m_table->setColumnWidth(VehicleListModel::Duration, 120);
    m_table->setColumnWidth(VehicleListModel::Fee, 110);

    tableCard->body()->addWidget(m_table, 1);
    root->addWidget(tableCard, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxy->setPlateFilter(text);
        updateCount();
    });
    connect(m_typeFilter, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_proxy->setTypeFilter(m_typeFilter->itemData(index).toInt());
        updateCount();
    });
    connect(m_statusFilter, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_proxy->setStatusFilter(index);
        updateCount();
    });
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_search->clear();
        m_typeFilter->setCurrentIndex(0);
        m_statusFilter->setCurrentIndex(0);
        updateCount();
    });
    connect(m_table, &QTableView::doubleClicked, this, &SearchPage::showDetail);

    refresh();
}

void SearchPage::refresh()
{
    if (!m_ctx)
        return;
    m_model->setLot(&m_ctx->lot(), pl::nowMs());
    updateCount();
}

void SearchPage::tick()
{
    if (!m_ctx)
        return;
    m_model->refresh(pl::nowMs());
    updateCount();
}

void SearchPage::updateCount()
{
    m_count->setText(QStringLiteral("共 %1 条记录").arg(m_proxy->rowCount()));
}

void SearchPage::showDetail(const QModelIndex& proxyIndex)
{
    if (!proxyIndex.isValid())
        return;

    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    if (!sourceIndex.isValid())
        return;

    const VehicleRow& r = m_model->rowAt(sourceIndex.row());
    QString text;
    text += QStringLiteral("车牌号：%1\n").arg(r.plate);
    text += QStringLiteral("车型：%1\n").arg(pl::typeLabel(r.type));
    text += QStringLiteral("状态：%1\n")
                .arg(r.waiting ? QStringLiteral("等待队列第 %1 位").arg(r.waitPos)
                               : QStringLiteral("在场"));
    if (!r.waiting)
        text += QStringLiteral("车位：P%1\n").arg(r.slot + 1, 3, 10, QLatin1Char('0'));
    text += QStringLiteral("%1：%2\n")
                .arg(r.waiting ? QStringLiteral("开始等待") : QStringLiteral("入场时间"),
                     pl::formatDateTime(r.sinceMs));
    text += QStringLiteral("%1：%2\n")
                .arg(r.waiting ? QStringLiteral("已等待") : QStringLiteral("已停时长"),
                     pl::formatDuration(r.durationMs));
    if (!r.waiting)
        text += QStringLiteral("预估费用：%1").arg(pl::formatMoney(r.estFeeCents));

    QMessageBox::information(this, QStringLiteral("车辆详情"), text);
}

} // namespace ui
