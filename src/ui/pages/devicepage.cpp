#include "ui/pages/devicepage.h"

#include "app/appcontext.h"
#include "app/devicebridge.h"
#include "domain/billing.h"
#include "interface/entry_exit_simulator.h"
#include "ui/theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ui {
namespace {

void setupLogTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(28);
    table->horizontalHeader()->setHighlightSections(false);
}

QColor stateColor(pl::devices::LaneState state)
{
    switch (state) {
    case pl::devices::LaneState::Idle:             return QColor(0x04, 0x78, 0x57);
    case pl::devices::LaneState::Fault:            return QColor(0xB9, 0x1C, 0x1C);
    case pl::devices::LaneState::Rejected:         return QColor(0xB4, 0x53, 0x09);
    case pl::devices::LaneState::VehiclePassing:   return QColor(0x25, 0x63, 0xEB);
    case pl::devices::LaneState::AwaitingApproval: return QColor(0xB4, 0x53, 0x09);
    default:                                       return QColor(0x25, 0x63, 0xEB);
    }
}

} // namespace

DevicePage::DevicePage(app::AppContext* ctx, app::DeviceBridge* bridge, QWidget* parent)
    : Page(parent)
    , m_ctx(ctx)
    , m_bridge(bridge)
{
    setObjectName(QStringLiteral("pageBody"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    buildLaneCards();

    auto* top = new QHBoxLayout();
    top->setSpacing(14);
    top->addWidget(m_entryCard, 1);
    top->addWidget(m_exitCard, 1);
    root->addLayout(top);

    // ---- 自动识别流水 ----
    auto* logCard = new Card(QStringLiteral("自动识别流水"), this);
    auto* logTools = new QHBoxLayout();
    auto* hint = new QLabel(
        QStringLiteral("这里只记录自动识别通道；人工登记/结算仍可在「停车操作」页完成。"), logCard);
    hint->setObjectName(QStringLiteral("muted"));
    auto* clearBtn = new QPushButton(QStringLiteral("清空"), logCard);
    clearBtn->setObjectName(QStringLiteral("ghost"));
    logTools->addWidget(hint, 1);
    logTools->addWidget(clearBtn);
    logCard->body()->addLayout(logTools);

    m_logTable = new QTableWidget(0, 6, logCard);
    m_logTable->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("车道"),
                                           QStringLiteral("车牌"), QStringLiteral("结果"),
                                           QStringLiteral("票据 / 金额"), QStringLiteral("说明")});
    setupLogTable(m_logTable);
    m_logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    logCard->body()->addWidget(m_logTable, 1);
    root->addWidget(logCard, 1);

    // ---- 设备状态 ----
    auto* bottomCard = new Card(QStringLiteral("设备与计数器"), this);
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(18);

    m_counterLabel = new QLabel(
        QStringLiteral("外设计数器：空置（等待真实外设数据接入）"), bottomCard);
    m_counterLabel->setObjectName(QStringLiteral("muted"));
    m_statsLabel = new QLabel(bottomCard);
    m_statsLabel->setObjectName(QStringLiteral("muted"));
    m_listenButton = new QPushButton(QStringLiteral("停止监听"), bottomCard);
    m_listenButton->setObjectName(QStringLiteral("ghost"));

    bottom->addWidget(m_counterLabel);
    bottom->addWidget(m_statsLabel);
    bottom->addStretch(1);
    bottom->addWidget(m_listenButton);
    bottomCard->body()->addLayout(bottom);
    root->addWidget(bottomCard);

    // ---- 连线 ----
    using namespace pl::devices;
    connect(m_bridge, &app::DeviceBridge::autoEntered, this,
            [this](const QString& plate, const QString& ticket, const QString& detail) {
                ++m_autoIn;
                appendLog(QStringLiteral("入口"), plate, QStringLiteral("自动入场"), ticket, detail);
                m_entryLast->setText(QStringLiteral("最近：%1 · %2 · %3")
                                         .arg(QDateTime::currentDateTime().toString(
                                                  QStringLiteral("HH:mm:ss")),
                                              plate, detail));
                updateCountersUi();
            });
    connect(m_bridge, &app::DeviceBridge::autoExited, this,
            [this](const QString& plate, qint64 feeCents, const QString& detail) {
                ++m_autoOut;
                appendLog(QStringLiteral("出口"), plate, QStringLiteral("自动出场"),
                          pl::formatMoney(feeCents), detail);
                m_exitLast->setText(QStringLiteral("最近：%1 · %2 · %3")
                                        .arg(QDateTime::currentDateTime().toString(
                                                 QStringLiteral("HH:mm:ss")),
                                             plate, detail));
                updateCountersUi();
            });
    connect(m_bridge, &app::DeviceBridge::autoRejected, this,
            [this](const QString& plate, const QString& reason) {
                ++m_autoErrors;
                appendLog(QStringLiteral("—"), plate, QStringLiteral("拒绝"),
                          QStringLiteral("—"), reason);
                updateCountersUi();
            });
    connect(m_bridge, &app::DeviceBridge::autoError, this, [this](const QString& message) {
        ++m_autoErrors;
        appendLog(QStringLiteral("—"), QStringLiteral("—"), QStringLiteral("异常"),
                  QStringLiteral("—"), message);
        updateCountersUi();
    });
    connect(m_bridge, &app::DeviceBridge::laneStateChanged, this,
            [this](LaneKind lane, LaneState state) {
                QLabel* label = lane == LaneKind::Entry ? m_entryState : m_exitState;
                label->setText(QStringLiteral("状态：%1").arg(laneStateText(state)));
                label->setStyleSheet(
                    QStringLiteral("color:%1; font-weight:700;").arg(stateColor(state).name()));
            });
    connect(m_bridge, &app::DeviceBridge::displayChanged, this,
            [this](LaneKind lane, const QString& text) {
                (lane == LaneKind::Entry ? m_entryDisplay : m_exitDisplay)->setText(text);
            });
    connect(m_bridge, &app::DeviceBridge::listeningChanged, this,
            [this](bool) { updateListeningUi(); });

    connect(m_entryButton, &QPushButton::clicked, this, [this]() {
        m_bridge->requestEntry(m_entryPlate->text(), m_entryType->currentData().toInt());
        m_entryPlate->clear();
    });
    connect(m_exitButton, &QPushButton::clicked, this, [this]() {
        m_bridge->requestExit(m_exitPlate->text());
        m_exitPlate->clear();
    });
    connect(m_listenButton, &QPushButton::clicked, this, [this]() {
        m_bridge->setListening(!m_bridge->isListening());
    });
    connect(clearBtn, &QPushButton::clicked, this, [this]() { m_logTable->setRowCount(0); });

    refresh();
}

void DevicePage::buildLaneCards()
{
    // 入口
    auto* entryCard = new Card(QStringLiteral("入口车道"), this);
    m_entryCard = entryCard;
    m_entryState = new QLabel(QStringLiteral("状态：空闲"), entryCard);
    m_entryState->setStyleSheet(QStringLiteral("color:#047857; font-weight:700;"));
    auto* entryTop = new QHBoxLayout();
    entryTop->addWidget(m_entryState);
    entryTop->addStretch(1);
    entryCard->body()->addLayout(entryTop);

    m_entryDisplay = new QLabel(QStringLiteral("入口空闲，等待车辆"), entryCard);
    m_entryDisplay->setObjectName(QStringLiteral("detailLabel"));
    m_entryDisplay->setWordWrap(true);
    m_entryDisplay->setMinimumHeight(34);
    entryCard->body()->addWidget(m_entryDisplay);

    m_entryLast = new QLabel(QStringLiteral("最近：—"), entryCard);
    m_entryLast->setObjectName(QStringLiteral("muted"));
    entryCard->body()->addWidget(m_entryLast);

    auto* entryForm = new QHBoxLayout();
    m_entryPlate = new QLineEdit(entryCard);
    m_entryPlate->setPlaceholderText(QStringLiteral("车牌（留空随机）"));
    m_entryPlate->setMaxLength(12);
    m_entryType = new QComboBox(entryCard);
    for (pl::VehicleType type : pl::allTypes())
        m_entryType->addItem(pl::typeLabel(type), static_cast<int>(type));
    m_entryButton = new QPushButton(QStringLiteral("模拟入口来车"), entryCard);
    m_entryButton->setCursor(Qt::PointingHandCursor);
    entryForm->addWidget(m_entryPlate, 1);
    entryForm->addWidget(m_entryType);
    entryForm->addWidget(m_entryButton);
    entryCard->body()->addLayout(entryForm);

    auto* entryHint = new QLabel(
        QStringLiteral("自动识别与人工登记共用同一份数据；人工入库仍可在「停车操作」完成。"),
        entryCard);
    entryHint->setObjectName(QStringLiteral("muted"));
    entryHint->setWordWrap(true);
    entryCard->body()->addWidget(entryHint);

    // 出口
    auto* exitCard = new Card(QStringLiteral("出口车道"), this);
    m_exitCard = exitCard;
    m_exitState = new QLabel(QStringLiteral("状态：空闲"), exitCard);
    m_exitState->setStyleSheet(QStringLiteral("color:#047857; font-weight:700;"));
    auto* exitTop = new QHBoxLayout();
    exitTop->addWidget(m_exitState);
    exitTop->addStretch(1);
    exitCard->body()->addLayout(exitTop);

    m_exitDisplay = new QLabel(QStringLiteral("出口空闲，等待车辆"), exitCard);
    m_exitDisplay->setObjectName(QStringLiteral("detailLabel"));
    m_exitDisplay->setWordWrap(true);
    m_exitDisplay->setMinimumHeight(34);
    exitCard->body()->addWidget(m_exitDisplay);

    m_exitLast = new QLabel(QStringLiteral("最近：—"), exitCard);
    m_exitLast->setObjectName(QStringLiteral("muted"));
    exitCard->body()->addWidget(m_exitLast);

    auto* exitForm = new QHBoxLayout();
    m_exitPlate = new QLineEdit(exitCard);
    m_exitPlate->setPlaceholderText(QStringLiteral("车牌（留空随机选场内车）"));
    m_exitPlate->setMaxLength(12);
    m_exitButton = new QPushButton(QStringLiteral("模拟出口来车"), exitCard);
    m_exitButton->setCursor(Qt::PointingHandCursor);
    exitForm->addWidget(m_exitPlate, 1);
    exitForm->addWidget(m_exitButton);
    exitCard->body()->addLayout(exitForm);

    auto* exitHint = new QLabel(
        QStringLiteral("出口先自动结算再抬杆；人工结算对话框仍可同时使用。"), exitCard);
    exitHint->setObjectName(QStringLiteral("muted"));
    exitHint->setWordWrap(true);
    exitCard->body()->addWidget(exitHint);
}

void DevicePage::appendLog(const QString& lane, const QString& plate, const QString& result,
                           const QString& amount, const QString& detail)
{
    if (m_logTable->rowCount() >= 300)
        m_logTable->removeRow(0);

    const int row = m_logTable->rowCount();
    m_logTable->insertRow(row);

    auto* timeItem = new QTableWidgetItem(
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    auto* laneItem = new QTableWidgetItem(lane);
    auto* plateItem = new QTableWidgetItem(plate);
    auto* resultItem = new QTableWidgetItem(result);
    auto* amountItem = new QTableWidgetItem(amount);
    auto* detailItem = new QTableWidgetItem(detail);

    timeItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    laneItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    resultItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    if (result == QStringLiteral("异常") || result == QStringLiteral("拒绝")) {
        resultItem->setForeground(QColor(0xB4, 0x53, 0x09));
        detailItem->setForeground(QColor(0xB4, 0x53, 0x09));
    }

    m_logTable->setItem(row, 0, timeItem);
    m_logTable->setItem(row, 1, laneItem);
    m_logTable->setItem(row, 2, plateItem);
    m_logTable->setItem(row, 3, resultItem);
    m_logTable->setItem(row, 4, amountItem);
    m_logTable->setItem(row, 5, detailItem);
    m_logTable->scrollToBottom();
}

void DevicePage::updateLaneUi()
{
    if (!m_bridge)
        return;

    using namespace pl::devices;
    if (auto* entry = m_bridge->simulator()->entryController()) {
        m_entryState->setText(QStringLiteral("状态：%1").arg(laneStateText(entry->state())));
        m_entryState->setStyleSheet(
            QStringLiteral("color:%1; font-weight:700;").arg(stateColor(entry->state()).name()));
    }
    if (auto* exit = m_bridge->simulator()->exitController()) {
        m_exitState->setText(QStringLiteral("状态：%1").arg(laneStateText(exit->state())));
        m_exitState->setStyleSheet(
            QStringLiteral("color:%1; font-weight:700;").arg(stateColor(exit->state()).name()));
    }

    if (auto* display = m_bridge->simulator()->entryDisplay())
        m_entryDisplay->setText(display->message());
    if (auto* display = m_bridge->simulator()->exitDisplay())
        m_exitDisplay->setText(display->message());
}

void DevicePage::updateListeningUi()
{
    if (!m_bridge)
        return;

    const bool listening = m_bridge->isListening();
    const bool readOnly = m_bridge->isReadOnly();
    m_listenButton->setText(listening ? QStringLiteral("停止监听")
                                      : QStringLiteral("开始监听"));

    const bool canSimulate = listening && !readOnly;
    m_entryButton->setEnabled(canSimulate);
    m_exitButton->setEnabled(canSimulate);
    m_entryPlate->setEnabled(canSimulate);
    m_exitPlate->setEnabled(canSimulate);
    m_entryType->setEnabled(canSimulate);

    if (readOnly)
        m_counterLabel->setText(QStringLiteral("外设计数器：空置（只读模式）"));
    else
        m_counterLabel->setText(QStringLiteral("外设计数器：空置（等待真实外设数据接入）"));
}

void DevicePage::updateCountersUi()
{
    m_statsLabel->setText(QStringLiteral("自动入场 %1 · 自动出场 %2 · 异常 %3")
                              .arg(m_autoIn)
                              .arg(m_autoOut)
                              .arg(m_autoErrors));
}

void DevicePage::refresh()
{
    if (!m_ctx || !m_bridge)
        return;
    updateLaneUi();
    updateListeningUi();
    updateCountersUi();
}

void DevicePage::tick()
{
    updateLaneUi();
}

} // namespace ui
