#include "ui/pages/statspage.h"

#include "app/appcontext.h"
#include "domain/billing.h"
#include "ui/theme.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVector>

namespace ui {
namespace {

// 月度收入柱状图（自绘，避免为一个图表引入 Qt Charts 依赖）
class RevenueChartImpl : public QWidget
{
public:
    using QWidget::QWidget;

    struct Item {
        QString month;
        qint64 cents = 0;
    };

    void setData(const QVector<Item>& items)
    {
        m_data = items;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF area = rect().adjusted(12, 20, -12, -26);
        if (m_data.isEmpty()) {
            painter.setPen(theme::textMuted());
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无出场流水"));
            return;
        }

        qint64 maxValue = 1;
        for (const Item& item : m_data)
            maxValue = qMax(maxValue, item.cents);

        const int count = m_data.size();
        const qreal gap = 14;
        const qreal maxBarWidth = 72;
        qreal barWidth = (area.width() - gap * (count - 1)) / count;
        barWidth = qMin(barWidth, maxBarWidth);
        const qreal totalWidth = count * barWidth + gap * (count - 1);
        const qreal startX = area.left() + (area.width() - totalWidth) / 2;

        painter.setPen(QPen(QColor(0xE6, 0xEA, 0xF2), 1));
        painter.drawLine(area.bottomLeft(), area.bottomRight());

        QFont font = painter.font();
        font.setPointSizeF(8.0);
        painter.setFont(font);

        for (int i = 0; i < count; ++i) {
            const Item& item = m_data.at(i);
            const qreal height = area.height() * static_cast<double>(item.cents) / maxValue;
            const QRectF bar(startX + i * (barWidth + gap), area.bottom() - height,
                             barWidth, height);

            painter.setPen(Qt::NoPen);
            painter.setBrush(theme::accent());
            painter.drawRoundedRect(bar, qMin<qreal>(6, barWidth / 2), 6);

            painter.setPen(theme::textMuted());
            painter.drawText(QRectF(bar.left() - 12, bar.top() - 20, barWidth + 24, 16),
                             Qt::AlignHCenter | Qt::AlignBottom, pl::formatMoney(item.cents));
            painter.drawText(QRectF(bar.left() - 12, area.bottom() + 6, barWidth + 24, 16),
                             Qt::AlignHCenter | Qt::AlignTop, item.month);
        }
    }

private:
    QVector<Item> m_data;
};

} // namespace

// 供头文件前向声明使用的包装类
class RevenueChart : public RevenueChartImpl
{
public:
    using RevenueChartImpl::RevenueChartImpl;
};

namespace {

void setupGridTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(30);
    table->horizontalHeader()->setHighlightSections(false);
}

QWidget* makeTypeRow(pl::VehicleType type, QProgressBar*& bar, QLabel*& count, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* dot = new QLabel(row);
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(QStringLiteral("background:%1; border-radius:5px;")
                           .arg(theme::typeColor(type).name()));
    auto* name = new QLabel(pl::typeLabel(type), row);
    name->setMinimumWidth(56);

    bar = new QProgressBar(row);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    bar->setStyleSheet(QStringLiteral("QProgressBar{background:#EEF2F7;border:none;border-radius:4px;}"
                                      "QProgressBar::chunk{background:%1;border-radius:4px;}")
                           .arg(theme::typeColor(type).name()));

    count = new QLabel(QStringLiteral("0 辆"), row);
    count->setObjectName(QStringLiteral("muted"));
    count->setMinimumWidth(56);
    count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(dot);
    layout->addWidget(name);
    layout->addWidget(bar, 1);
    layout->addWidget(count);
    return row;
}

} // namespace

StatsPage::StatsPage(app::AppContext* ctx, QWidget* parent)
    : Page(parent)
    , m_ctx(ctx)
{
    setObjectName(QStringLiteral("pageBody"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(14);
    m_kpiEntered = new KpiCard(QStringLiteral("累计入场"), this);
    m_kpiExited = new KpiCard(QStringLiteral("累计出场"), this);
    m_kpiRevenue = new KpiCard(QStringLiteral("累计收入"), this);
    m_kpiHistory = new KpiCard(QStringLiteral("历史流水"), this);
    kpiRow->addWidget(m_kpiEntered, 1);
    kpiRow->addWidget(m_kpiExited, 1);
    kpiRow->addWidget(m_kpiRevenue, 1);
    kpiRow->addWidget(m_kpiHistory, 1);
    root->addLayout(kpiRow);

    auto* mid = new QHBoxLayout();
    mid->setSpacing(14);

    auto* typeCard = new Card(QStringLiteral("车型分布（在场 + 排队）"), this);
    for (pl::VehicleType t : pl::allTypes()) {
        const int index = static_cast<int>(t);
        typeCard->body()->addWidget(makeTypeRow(t, m_bars[index], m_counts[index], typeCard));
    }
    m_totalLabel = new QLabel(typeCard);
    m_totalLabel->setObjectName(QStringLiteral("muted"));
    typeCard->body()->addWidget(m_totalLabel);
    typeCard->body()->addStretch(1);
    mid->addWidget(typeCard, 1);

    auto* monthCard = new Card(QStringLiteral("月度收入趋势"), this);
    m_chart = new RevenueChart(monthCard);
    monthCard->body()->addWidget(m_chart, 1);
    m_monthTotal = new QLabel(monthCard);
    m_monthTotal->setObjectName(QStringLiteral("muted"));
    monthCard->body()->addWidget(m_monthTotal);
    mid->addWidget(monthCard, 1);

    root->addLayout(mid, 2);

    auto* recentCard = new Card(QStringLiteral("最近出场流水"), this);
    auto* recentTools = new QHBoxLayout();
    recentTools->addStretch(1);
    auto* exportBtn = new QPushButton(QStringLiteral("导出 CSV…"), recentCard);
    exportBtn->setObjectName(QStringLiteral("ghost"));
    exportBtn->setCursor(Qt::PointingHandCursor);
    recentTools->addWidget(exportBtn);
    recentCard->body()->addLayout(recentTools);

    m_recentTable = new QTableWidget(0, 5, recentCard);
    m_recentTable->setHorizontalHeaderLabels({QStringLiteral("车牌号"), QStringLiteral("车型"),
                                              QStringLiteral("出场时间"), QStringLiteral("停车时长"),
                                              QStringLiteral("金额")});
    setupGridTable(m_recentTable);
    m_recentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    recentCard->body()->addWidget(m_recentTable, 1);
    root->addWidget(recentCard, 3);

    connect(exportBtn, &QPushButton::clicked, this, &StatsPage::exportCsv);

    refresh();
}

void StatsPage::refresh()
{
    if (!m_ctx)
        return;

    const pl::Lot& lot = m_ctx->lot();
    const QList<app::ExitRecord> exits = m_ctx->allExits();

    m_kpiEntered->setValue(QString::number(lot.totalEntered));
    m_kpiEntered->setHint(QStringLiteral("含排队车辆"));
    m_kpiExited->setValue(QString::number(lot.totalExited));
    m_kpiExited->setHint(QStringLiteral("累计完成结算"));
    m_kpiRevenue->setValue(pl::formatMoney(lot.revenueCents));
    m_kpiRevenue->setHint(QStringLiteral("按实际入位时长计费"));
    m_kpiRevenue->setAccent(theme::accent());
    m_kpiHistory->setValue(QString::number(exits.size()));
    m_kpiHistory->setHint(QStringLiteral("归档在 history/ 中"));

    const int waiting = static_cast<int>(lot.waiting.size());
    const int total = lot.occupyCount() + waiting;
    for (pl::VehicleType t : pl::allTypes()) {
        const int index = static_cast<int>(t);
        const int count = lot.countByType(t, true);
        m_bars[index]->setRange(0, qMax(1, total));
        m_bars[index]->setValue(count);
        m_counts[index]->setText(QStringLiteral("%1 辆").arg(count));
    }
    m_totalLabel->setText(QStringLiteral("在场 %1 / %2 个车位 · 排队 %3 辆 · 合计 %4 辆 · 利用率 %5%")
                              .arg(lot.occupyCount())
                              .arg(lot.cfg.capacity)
                              .arg(waiting)
                              .arg(total)
                              .arg(lot.cfg.capacity > 0 ? lot.occupyCount() * 100 / lot.cfg.capacity : 0));

    // 月度收入：按月份聚合后取最近 6 个月绘制柱状图
    QMap<QString, QPair<int, qint64>> months;
    for (const app::ExitRecord& e : exits) {
        const QString key =
            QDateTime::fromMSecsSinceEpoch(e.exitedAtMs).toString(QStringLiteral("yyyy-MM"));
        months[key].first += 1;
        months[key].second += e.feeCents;
    }
    QVector<RevenueChartImpl::Item> chartData;
    const QStringList keys = months.keys();
    qint64 totalCents = 0;
    for (int i = 0; i < keys.size(); ++i) {
        RevenueChartImpl::Item item;
        item.month = keys.at(i);
        item.cents = months.value(keys.at(i)).second;
        chartData.append(item);
        totalCents += months.value(keys.at(i)).second;
    }
    if (chartData.size() > 6)
        chartData = chartData.mid(chartData.size() - 6);
    m_chart->setData(chartData);
    m_monthTotal->setText(QStringLiteral("共 %1 个月 · 累计 %2")
                              .arg(chartData.size())
                              .arg(pl::formatMoney(totalCents)));

    // 最近流水
    const int shown = qMin(8, exits.size());
    m_recentTable->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const app::ExitRecord& e = exits.at(exits.size() - 1 - i);
        auto* plateItem = new QTableWidgetItem(e.plate);
        auto* typeItem = new QTableWidgetItem(pl::typeLabel(e.type));
        auto* timeItem =
            new QTableWidgetItem(pl::formatDateTime(e.exitedAtMs));
        auto* stayItem = new QTableWidgetItem(pl::formatDuration(e.stayMs));
        auto* feeItem = new QTableWidgetItem(pl::formatMoney(e.feeCents));
        typeItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        timeItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        stayItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        feeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_recentTable->setItem(i, 0, plateItem);
        m_recentTable->setItem(i, 1, typeItem);
        m_recentTable->setItem(i, 2, timeItem);
        m_recentTable->setItem(i, 3, stayItem);
        m_recentTable->setItem(i, 4, feeItem);
    }
}

void StatsPage::exportCsv()
{
    if (!m_ctx)
        return;

    const QString defaultPath =
        QDir(m_ctx->dataDir()).filePath(QStringLiteral("出车流水.csv"));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出出场流水"), defaultPath, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_ctx->exportHistoryCsv(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 条流水到：\n%2")
                                 .arg(m_ctx->allExits().size())
                                 .arg(path));
}

} // namespace ui
