#include "ui/mainwindow.h"

#include "app/appcontext.h"
#include "app/devicebridge.h"
#include "domain/billing.h"
#include "ui/page.h"
#include "ui/pages/devicepage.h"
#include "ui/pages/operationspage.h"
#include "ui/pages/overviewpage.h"
#include "ui/pages/searchpage.h"
#include "ui/pages/settingspage.h"
#include "ui/pages/statspage.h"
#include "ui/theme.h"

#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <iterator>

namespace ui {
namespace {

struct PageInfo {
    const char* title;
    const char* subtitle;
};

const PageInfo kPages[] = {
    {"总览", "实时车位分布与关键指标"},
    {"停车操作", "入库登记与出场结算"},
    {"自动识别", "入口/出口设备状态与自动识别流水"},
    {"车辆查询", "按车牌、车型与状态筛选"},
    {"统计报表", "收入、车型分布与历史流水"},
    {"系统设置", "容量、计费规则与数据目录"},
};

} // namespace

MainWindow::MainWindow(app::AppContext* ctx, QWidget* parent)
    : QMainWindow(parent)
    , m_ctx(ctx)
{
    setWindowTitle(QStringLiteral("停车场管理系统"));
    resize(1400, 900);
    setMinimumSize(1120, 720);
    setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    buildUi();
    refreshAll();
    updateStatus();

    connect(m_ctx, &app::AppContext::changed, this, [this]() {
        m_lastSaveAt = pl::nowMs();
        refreshAll();
        updateStatus();
    });
    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_pages.size())
            return;
        m_nav->blockSignals(true);
        m_nav->setCurrentRow(row);
        m_nav->blockSignals(false);
        m_stack->setCurrentIndex(row);
        updateHeader(row);
        m_pages.at(row)->refresh();
    });

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, [this]() {
        const int index = m_stack->currentIndex();
        if (index >= 0 && index < m_pages.size())
            m_pages.at(index)->tick();
    });
    m_tick->start();
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 左侧导航 ----
    auto* sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(224);

    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 22, 0, 16);
    sideLayout->setSpacing(6);

    auto* brand = new QLabel(QStringLiteral("停车场管理"), sidebar);
    brand->setObjectName(QStringLiteral("brandTitle"));
    brand->setContentsMargins(20, 0, 20, 0);
    auto* brandSub = new QLabel(QStringLiteral("Parking Lot · Qt 6"), sidebar);
    brandSub->setObjectName(QStringLiteral("brandSub"));
    brandSub->setContentsMargins(20, 0, 20, 12);
    sideLayout->addWidget(brand);
    sideLayout->addWidget(brandSub);

    m_nav = new QListWidget(sidebar);
    m_nav->setObjectName(QStringLiteral("nav"));
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (const PageInfo& info : kPages)
        m_nav->addItem(QString::fromUtf8(info.title));
    sideLayout->addWidget(m_nav, 1);

    auto* foot = new QLabel(QStringLiteral("数据落盘：分代快照 + journal"), sidebar);
    foot->setObjectName(QStringLiteral("sidebarFoot"));
    foot->setContentsMargins(20, 8, 20, 0);
    foot->setWordWrap(true);
    sideLayout->addWidget(foot);

    root->addWidget(sidebar);

    // ---- 右侧内容区 ----
    auto* content = new QWidget(central);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 18, 24, 12);
    contentLayout->setSpacing(14);

    auto* header = new QHBoxLayout();
    header->setSpacing(12);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    m_title = new QLabel(QStringLiteral("总览"), content);
    m_title->setObjectName(QStringLiteral("pageTitle"));
    m_subtitle = new QLabel(QStringLiteral("实时车位分布与关键指标"), content);
    m_subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    titleBox->addWidget(m_title);
    titleBox->addWidget(m_subtitle);
    header->addLayout(titleBox);
    header->addStretch(1);

    auto* compactBtn = new QPushButton(QStringLiteral("立即压缩"), content);
    compactBtn->setObjectName(QStringLiteral("ghost"));
    compactBtn->setCursor(Qt::PointingHandCursor);
    compactBtn->setToolTip(QStringLiteral("把 journal 归档并写出新一代快照"));
    header->addWidget(compactBtn);
    contentLayout->addLayout(header);

    m_stack = new QStackedWidget(content);
    m_bridge = new app::DeviceBridge(m_ctx, this);
    m_pages.append(new OverviewPage(m_ctx, m_stack));
    m_pages.append(new OperationsPage(m_ctx, m_stack));
    m_pages.append(new DevicePage(m_ctx, m_bridge, m_stack));
    m_pages.append(new SearchPage(m_ctx, m_stack));
    m_pages.append(new StatsPage(m_ctx, m_stack));
    m_pages.append(new SettingsPage(m_ctx, m_stack));
    for (Page* page : m_pages)
        m_stack->addWidget(page);
    contentLayout->addWidget(m_stack, 1);

    root->addWidget(content, 1);

    // ---- 状态栏 ----
    statusBar()->setSizeGripEnabled(false);
    m_statusDir = new QLabel(this);
    m_statusJournal = new QLabel(this);
    m_statusSaved = new QLabel(this);
    statusBar()->addWidget(m_statusDir, 1);
    statusBar()->addPermanentWidget(m_statusJournal);
    statusBar()->addPermanentWidget(m_statusSaved);

    m_nav->setCurrentRow(0);
    m_stack->setCurrentIndex(0);

    connect(compactBtn, &QPushButton::clicked, this, &MainWindow::compactNow);
}

void MainWindow::showPage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return;
    m_nav->blockSignals(true);
    m_nav->setCurrentRow(index);
    m_nav->blockSignals(false);
    m_stack->setCurrentIndex(index);
    updateHeader(index);
    m_pages.at(index)->refresh();
}

void MainWindow::updateHeader(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(kPages)))
        return;
    m_title->setText(QString::fromUtf8(kPages[index].title));
    m_subtitle->setText(QString::fromUtf8(kPages[index].subtitle));
}

void MainWindow::refreshAll()
{
    for (Page* page : m_pages)
        page->refresh();
}

void MainWindow::updateStatus()
{
    const pl::LotStore::Stats stats = m_ctx->store().stats();

    m_statusDir->setText(
        QStringLiteral("数据目录：%1").arg(QDir::toNativeSeparators(m_ctx->dataDir())));
    m_statusJournal->setText(QStringLiteral("journal %1 条待压缩").arg(stats.journalRecords));

    if (m_ctx->isReadOnly()) {
        m_statusSaved->setText(QStringLiteral("只读模式"));
        return;
    }
    if (m_lastSaveAt <= 0)
        m_lastSaveAt = pl::nowMs();
    m_statusSaved->setText(
        QStringLiteral("已自动保存 %1").arg(pl::formatClock(m_lastSaveAt)));
}

void MainWindow::compactNow()
{
    QString error;
    if (!m_ctx->compactNow(&error)) {
        QMessageBox::warning(this, QStringLiteral("压缩失败"), error);
        return;
    }
    m_lastSaveAt = pl::nowMs();
    refreshAll();
    updateStatus();
    statusBar()->showMessage(QStringLiteral("已写入新一代快照"), 3000);
}

} // namespace ui
