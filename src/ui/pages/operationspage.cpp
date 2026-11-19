#include "ui/pages/operationspage.h"

#include "app/appcontext.h"
#include "domain/billing.h"
#include "ui/dialogs/enterdialog.h"
#include "ui/dialogs/exitdialog.h"
#include "ui/theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui {
namespace {

QLabel* makeCaption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("muted"));
    return label;
}

QLabel* makeValue(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("detailLabel"));
    return label;
}

} // namespace

OperationsPage::OperationsPage(app::AppContext* ctx, QWidget* parent)
    : Page(parent)
    , m_ctx(ctx)
{
    setObjectName(QStringLiteral("pageBody"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    auto* top = new QHBoxLayout();
    top->setSpacing(14);

    // ---- 入库卡片 ----
    auto* enterCard = new Card(QStringLiteral("车辆入库"), this);
    auto* enterDesc = makeCaption(
        QStringLiteral("登记车牌并分配最小编号空位；满场时自动进入等待队列。"), enterCard);
    enterDesc->setWordWrap(true);
    auto* enterBtn = new QPushButton(QStringLiteral("车辆入库…"), enterCard);
    enterBtn->setMinimumHeight(44);
    enterBtn->setCursor(Qt::PointingHandCursor);
    m_emptyLabel = makeValue(QStringLiteral("空位 —"), enterCard);
    m_queueLabel = makeValue(QStringLiteral("等待队列 —"), enterCard);

    enterCard->body()->addWidget(enterDesc);
    enterCard->body()->addStretch(1);
    enterCard->body()->addWidget(enterBtn);
    enterCard->body()->addWidget(m_emptyLabel);
    enterCard->body()->addWidget(m_queueLabel);

    // ---- 出场卡片 ----
    auto* exitCard = new Card(QStringLiteral("出场结算"), this);
    auto* exitDesc = makeCaption(
        QStringLiteral("按实际停车时长计费；排队时间不计费，出场后队首自动补位。"), exitCard);
    exitDesc->setWordWrap(true);
    auto* exitBtn = new QPushButton(QStringLiteral("出场结算…"), exitCard);
    exitBtn->setMinimumHeight(44);
    exitBtn->setCursor(Qt::PointingHandCursor);
    m_insideLabel = makeValue(QStringLiteral("在场 —"), exitCard);
    m_ruleLabel = makeValue(QStringLiteral("计费规则 —"), exitCard);

    exitCard->body()->addWidget(exitDesc);
    exitCard->body()->addStretch(1);
    exitCard->body()->addWidget(exitBtn);
    exitCard->body()->addWidget(m_insideLabel);
    exitCard->body()->addWidget(m_ruleLabel);

    top->addWidget(enterCard, 1);
    top->addWidget(exitCard, 1);
    enterCard->setMinimumHeight(240);
    exitCard->setMinimumHeight(240);
    root->addLayout(top);

    // ---- 规则摘要 ----
    auto* ruleCard = new Card(QStringLiteral("当前业务规则"), this);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(6);

    grid->addWidget(makeCaption(QStringLiteral("免费时长"), ruleCard), 0, 0);
    grid->addWidget(makeCaption(QStringLiteral("计费单位"), ruleCard), 0, 1);
    grid->addWidget(makeCaption(QStringLiteral("计费单价"), ruleCard), 0, 2);
    grid->addWidget(makeCaption(QStringLiteral("等待队列上限"), ruleCard), 0, 3);

    m_ruleFree = makeValue(QStringLiteral("—"), ruleCard);
    m_ruleUnit = makeValue(QStringLiteral("—"), ruleCard);
    m_rulePrice = makeValue(QStringLiteral("—"), ruleCard);
    m_ruleMaxWaiting = makeValue(QStringLiteral("—"), ruleCard);
    grid->addWidget(m_ruleFree, 1, 0);
    grid->addWidget(m_ruleUnit, 1, 1);
    grid->addWidget(m_rulePrice, 1, 2);
    grid->addWidget(m_ruleMaxWaiting, 1, 3);
    grid->setColumnStretch(4, 1);
    grid->setRowStretch(2, 1);

    ruleCard->body()->addLayout(grid);
    auto* hint = makeCaption(
        QStringLiteral("提示：规则可在「系统设置」中调整；容量调整要求场内与队列均为空。"), ruleCard);
    ruleCard->body()->addWidget(hint);

    auto* tipsCard = new Card(QStringLiteral("操作提示"), this);
    auto* tips = new QLabel(
        QStringLiteral("1. 在「车辆入库…」中登记车牌与车型；满场时自动排队。\n"
                       "2. 出场前在「出场结算…」中查询金额，确认后放行。\n"
                       "3. 出场后队首车辆自动补位，排队时长不计费。\n"
                       "4. 每次操作都会立即追加日志，退出程序时自动压缩快照。"),
        tipsCard);
    tips->setObjectName(QStringLiteral("muted"));
    tips->setWordWrap(true);
    tipsCard->body()->addWidget(tips);
    tipsCard->body()->addStretch(1);

    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(14);
    bottom->addWidget(ruleCard, 1);
    bottom->addWidget(tipsCard, 1);
    root->addLayout(bottom, 1);

    connect(enterBtn, &QPushButton::clicked, this, &OperationsPage::openEnterDialog);
    connect(exitBtn, &QPushButton::clicked, this, &OperationsPage::openExitDialog);

    refresh();
}

void OperationsPage::refresh()
{
    if (!m_ctx)
        return;

    const pl::Lot& lot = m_ctx->lot();
    const int inside = lot.occupyCount();
    const int waiting = static_cast<int>(lot.waiting.size());

    m_emptyLabel->setText(QStringLiteral("空位 %1 / %2").arg(lot.cfg.capacity - inside).arg(lot.cfg.capacity));
    m_queueLabel->setText(waiting > 0
                              ? QStringLiteral("等待队列 %1 辆 · 队首 %2").arg(waiting).arg(lot.waiting.first().plate)
                              : QStringLiteral("等待队列为空"));

    m_insideLabel->setText(QStringLiteral("当前在场 %1 辆").arg(inside));
    m_ruleLabel->setText(QStringLiteral("免费 %1 分钟 · 每 %2 分钟 %3")
                             .arg(lot.cfg.freeMinutes)
                             .arg(lot.cfg.billingUnitMinutes)
                             .arg(pl::formatMoney(lot.cfg.unitPriceCents)));

    m_ruleFree->setText(QStringLiteral("%1 分钟").arg(lot.cfg.freeMinutes));
    m_ruleUnit->setText(QStringLiteral("%1 分钟").arg(lot.cfg.billingUnitMinutes));
    m_rulePrice->setText(pl::formatMoney(lot.cfg.unitPriceCents));
    m_ruleMaxWaiting->setText(QStringLiteral("%1 辆").arg(lot.cfg.maxWaiting));
}

void OperationsPage::openEnterDialog()
{
    EnterDialog dialog(m_ctx, this);
    dialog.exec();
}

void OperationsPage::openExitDialog()
{
    ExitDialog dialog(m_ctx, this);
    dialog.exec();
}

} // namespace ui
