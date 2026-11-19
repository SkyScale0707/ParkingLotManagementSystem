#include "ui/pages/overviewpage.h"

#include "app/appcontext.h"
#include "domain/billing.h"
#include "ui/lotview.h"
#include "ui/theme.h"
#include "ui/vehiclelistmodel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ui {
namespace {

// 只显示排队车辆
class WaitingProxy : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        auto* model = qobject_cast<VehicleListModel*>(sourceModel());
        if (!model)
            return false;
        const QModelIndex idx = model->index(row, 0, parent);
        return model->rowAt(idx.row()).waiting;
    }
};

QWidget* makeLegend(pl::VehicleType type, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* dot = new QLabel(w);
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(QStringLiteral("background:%1; border-radius:5px;")
                           .arg(theme::typeColor(type).name()));
    auto* text = new QLabel(pl::typeLabel(type), w);
    text->setObjectName(QStringLiteral("muted"));

    layout->addWidget(dot);
    layout->addWidget(text);
    return w;
}

void setupTable(QTableView* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(30);
    table->horizontalHeader()->setHighlightSections(false);
    table->setFocusPolicy(Qt::NoFocus);
}

} // namespace

OverviewPage::OverviewPage(app::AppContext* ctx, QWidget* parent)
    : Page(parent)
    , m_ctx(ctx)
{
    setObjectName(QStringLiteral("pageBody"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    // ---- KPI 行 ----
    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(14);
    m_kpiInside = new KpiCard(QStringLiteral("在场车辆"), this);
    m_kpiEmpty = new KpiCard(QStringLiteral("空余车位"), this);
    m_kpiQueue = new KpiCard(QStringLiteral("等待队列"), this);
    m_kpiRevenue = new KpiCard(QStringLiteral("累计收入"), this);
    kpiRow->addWidget(m_kpiInside, 1);
    kpiRow->addWidget(m_kpiEmpty, 1);
    kpiRow->addWidget(m_kpiQueue, 1);
    kpiRow->addWidget(m_kpiRevenue, 1);
    root->addLayout(kpiRow);

    // ---- 中部：2D 视图 + 右侧列表 ----
    auto* mid = new QHBoxLayout();
    mid->setSpacing(14);

    auto* viewCard = new Card(QStringLiteral("车位分布"), this);
    m_view = new LotView(viewCard);

    auto* viewTools = new QHBoxLayout();
    viewTools->setSpacing(8);
    for (pl::VehicleType t : pl::allTypes())
        viewTools->addWidget(makeLegend(t, viewCard));
    viewTools->addStretch(1);

    auto* zoomOutBtn = new QPushButton(QStringLiteral("−"), viewCard);
    zoomOutBtn->setObjectName(QStringLiteral("ghost"));
    zoomOutBtn->setFixedWidth(34);
    zoomOutBtn->setToolTip(QStringLiteral("缩小"));
    auto* zoomInBtn = new QPushButton(QStringLiteral("+"), viewCard);
    zoomInBtn->setObjectName(QStringLiteral("ghost"));
    zoomInBtn->setFixedWidth(34);
    zoomInBtn->setToolTip(QStringLiteral("放大"));
    auto* fitBtn = new QPushButton(QStringLiteral("复位视图"), viewCard);
    fitBtn->setObjectName(QStringLiteral("ghost"));
    fitBtn->setToolTip(QStringLiteral("缩放到合适大小"));
    for (QPushButton* b : {zoomOutBtn, zoomInBtn, fitBtn})
        b->setCursor(Qt::PointingHandCursor);

    viewTools->addWidget(zoomOutBtn);
    viewTools->addWidget(zoomInBtn);
    viewTools->addWidget(fitBtn);
    viewCard->body()->addLayout(viewTools);
    viewCard->body()->addWidget(m_view, 1);

    m_slotDetail = new QLabel(
        QStringLiteral("点击车位查看详情 · 滚轮缩放 · 按住左键拖动平移"), viewCard);
    m_slotDetail->setObjectName(QStringLiteral("muted"));
    viewCard->body()->addWidget(m_slotDetail);

    connect(zoomInBtn, &QPushButton::clicked, m_view, &LotView::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, m_view, &LotView::zoomOut);
    connect(fitBtn, &QPushButton::clicked, m_view, &LotView::fitToView);
    connect(m_view, &LotView::slotActivated, this, &OverviewPage::onSlotActivated);

    mid->addWidget(viewCard, 1);

    auto* rightWrap = new QWidget(this);
    rightWrap->setMinimumWidth(350);
    rightWrap->setMaximumWidth(410);
    auto* right = new QVBoxLayout(rightWrap);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(14);

    auto* queueCard = new Card(QStringLiteral("等待队列"), rightWrap);
    m_queueHint = new QLabel(QStringLiteral("FIFO · 队首优先补位"), queueCard);
    m_queueHint->setObjectName(QStringLiteral("muted"));
    m_queueTable = new QTableView(queueCard);
    setupTable(m_queueTable);
    m_queueModel = new VehicleListModel(this);
    auto* proxy = new WaitingProxy(this);
    proxy->setSourceModel(m_queueModel);
    m_queueTable->setModel(proxy);
    m_queueTable->setColumnHidden(VehicleListModel::Slot, true);
    m_queueTable->setColumnHidden(VehicleListModel::Since, true);
    m_queueTable->setColumnHidden(VehicleListModel::Duration, true);
    m_queueTable->setColumnHidden(VehicleListModel::Fee, true);
    m_queueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_queueTable->setMinimumHeight(220);
    queueCard->body()->addWidget(m_queueHint);
    queueCard->body()->addWidget(m_queueTable, 1);
    right->addWidget(queueCard, 3);

    auto* recentCard = new Card(QStringLiteral("最近出场"), rightWrap);
    m_recentTable = new QTableWidget(0, 3, recentCard);
    m_recentTable->setHorizontalHeaderLabels(
        {QStringLiteral("车牌号"), QStringLiteral("时长"), QStringLiteral("金额")});
    setupTable(m_recentTable);
    m_recentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_recentTable->setSelectionMode(QAbstractItemView::NoSelection);
    recentCard->body()->addWidget(m_recentTable, 1);
    right->addWidget(recentCard, 2);

    mid->addWidget(rightWrap);
    root->addLayout(mid, 1);

    refresh();
}

void OverviewPage::refresh()
{
    if (!m_ctx)
        return;

    const pl::Lot& lot = m_ctx->lot();
    const qint64 now = pl::nowMs();
    const int capacity = lot.cfg.capacity;
    const int inside = lot.occupyCount();
    const int empty = capacity - inside;
    const int waiting = static_cast<int>(lot.waiting.size());
    const int utilization = capacity > 0 ? inside * 100 / capacity : 0;

    m_kpiInside->setValue(QStringLiteral("%1 / %2").arg(inside).arg(capacity));
    m_kpiInside->setHint(QStringLiteral("利用率 %1%").arg(utilization));
    m_kpiInside->setAccent(theme::accent());

    m_kpiEmpty->setValue(QString::number(empty));
    m_kpiEmpty->setHint(QStringLiteral("总容量 %1 个车位").arg(capacity));
    m_kpiEmpty->setAccent(empty > 0 ? QColor(0x10, 0xB9, 0x81) : QColor(0xDC, 0x26, 0x26));

    m_kpiQueue->setValue(QString::number(waiting));
    m_kpiQueue->setHint(waiting > 0 ? QStringLiteral("队首 %1").arg(lot.waiting.first().plate)
                                    : QStringLiteral("暂无排队车辆"));
    m_kpiQueue->setAccent(waiting > 0 ? QColor(0xF5, 0x9E, 0x0B) : theme::textMuted());

    m_kpiRevenue->setValue(pl::formatMoney(lot.revenueCents));
    m_kpiRevenue->setHint(QStringLiteral("%1 笔出场流水").arg(m_ctx->allExits().size()));
    m_kpiRevenue->setAccent(theme::accent());

    m_view->setLot(&lot);
    m_queueModel->setLot(&lot, now);
    m_queueHint->setText(waiting > 0
                             ? QStringLiteral("FIFO · 共 %1 辆，队首优先补位").arg(waiting)
                             : QStringLiteral("当前无排队车辆"));

    updateRecent();

    if (m_selectedSlot >= 0)
        onSlotActivated(m_selectedSlot);
    else
        m_slotDetail->setText(QStringLiteral("点击车位查看详情 · 滚轮缩放 · 按住左键拖动平移"));
}

void OverviewPage::tick()
{
    if (!m_ctx)
        return;
    m_queueModel->refresh(pl::nowMs());
    if (m_selectedSlot >= 0)
        onSlotActivated(m_selectedSlot);
}

void OverviewPage::onSlotActivated(int slot)
{
    if (!m_ctx)
        return;

    const pl::Lot& lot = m_ctx->lot();
    if (slot < 0 || slot >= lot.slotPlate.size()) {
        m_selectedSlot = -1;
        return;
    }
    m_selectedSlot = slot;

    const QString label = QStringLiteral("P%1").arg(slot + 1, 3, 10, QLatin1Char('0'));
    const QString plate = lot.slotPlate.at(slot);
    if (plate.isEmpty()) {
        m_slotDetail->setText(QStringLiteral("%1 · 空闲车位").arg(label));
        return;
    }

    const auto it = lot.active.constFind(plate);
    if (it == lot.active.constEnd()) {
        m_slotDetail->setText(QStringLiteral("%1 · 数据异常（映射缺失）").arg(label));
        return;
    }

    const pl::ActiveEntry& e = it.value();
    const qint64 now = pl::nowMs();
    const qint64 stay = qMax<qint64>(0, now - e.enteredAtMs);
    const qint64 fee = pl::feeCentsFor(e.enteredAtMs, now, lot.cfg);

    m_slotDetail->setText(QStringLiteral("%1 · %2 · %3 · 入场 %4 · 已停 %5 · 预估 %6")
                              .arg(label, e.plate, pl::typeLabel(e.type),
                                   pl::formatDateTime(e.enteredAtMs), pl::formatDuration(stay),
                                   pl::formatMoney(fee)));
}

void OverviewPage::updateRecent()
{
    if (!m_ctx)
        return;
    const QList<app::ExitRecord> exits = m_ctx->recentExits(5);
    m_recentTable->setRowCount(exits.size());
    for (int i = 0; i < exits.size(); ++i) {
        const app::ExitRecord& e = exits.at(i);
        auto* plateItem = new QTableWidgetItem(e.plate);
        auto* stayItem = new QTableWidgetItem(pl::formatDuration(e.stayMs));
        auto* feeItem = new QTableWidgetItem(pl::formatMoney(e.feeCents));

        plateItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        plateItem->setToolTip(QStringLiteral("%1 · %2").arg(e.plate, pl::typeLabel(e.type)));
        stayItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        feeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_recentTable->setItem(i, 0, plateItem);
        m_recentTable->setItem(i, 1, stayItem);
        m_recentTable->setItem(i, 2, feeItem);
    }
}

} // namespace ui
