#include "ui/pages/settingspage.h"

#include "app/appcontext.h"
#include "domain/billing.h"
#include "ui/theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

namespace ui {
namespace {

QString formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

QLabel* makeValue(QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("—"), parent);
    label->setObjectName(QStringLiteral("detailLabel"));
    return label;
}

} // namespace

SettingsPage::SettingsPage(app::AppContext* ctx, QWidget* parent)
    : Page(parent)
    , m_ctx(ctx)
{
    setObjectName(QStringLiteral("pageBody"));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    // ---- 基本设置 ----
    auto* settingsCard = new Card(QStringLiteral("计费与容量"), this);
    m_readOnly = new QLabel(
        QStringLiteral("只读模式：另一实例正在使用该数据目录，设置不可修改。"), settingsCard);
    m_readOnly->setObjectName(QStringLiteral("warnLabel"));
    m_readOnly->setWordWrap(true);
    m_readOnly->setVisible(false);
    settingsCard->body()->addWidget(m_readOnly);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_capacity = new QSpinBox(settingsCard);
    m_capacity->setRange(1, 10000);
    m_capacity->setSuffix(QStringLiteral(" 个"));
    m_capacity->setToolTip(QStringLiteral("仅在停车场与等待队列均为空时可修改"));

    m_price = new QDoubleSpinBox(settingsCard);
    m_price->setRange(0.0, 10000.0);
    m_price->setDecimals(2);
    m_price->setSingleStep(0.5);
    m_price->setPrefix(QStringLiteral("¥ "));

    m_free = new QSpinBox(settingsCard);
    m_free->setRange(0, 1440);
    m_free->setSuffix(QStringLiteral(" 分钟"));

    m_unit = new QSpinBox(settingsCard);
    m_unit->setRange(1, 1440);
    m_unit->setSuffix(QStringLiteral(" 分钟"));

    m_maxWaiting = new QSpinBox(settingsCard);
    m_maxWaiting->setRange(0, 10000);
    m_maxWaiting->setSuffix(QStringLiteral(" 辆"));

    for (QWidget* box : {static_cast<QWidget*>(m_capacity), static_cast<QWidget*>(m_price),
                         static_cast<QWidget*>(m_free), static_cast<QWidget*>(m_unit),
                         static_cast<QWidget*>(m_maxWaiting)}) {
        box->setMaximumWidth(220);
        if (auto* spin = qobject_cast<QAbstractSpinBox*>(box))
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }

    form->addRow(QStringLiteral("车位容量"), m_capacity);
    form->addRow(QStringLiteral("计费单价（每计费单位）"), m_price);
    form->addRow(QStringLiteral("免费时长"), m_free);
    form->addRow(QStringLiteral("计费单位"), m_unit);
    form->addRow(QStringLiteral("等待队列上限"), m_maxWaiting);
    settingsCard->body()->addLayout(form);

    auto* hint = new QLabel(
        QStringLiteral("调整容量要求场内车辆与等待队列都为空；其余计费项可随时修改，立即生效。"),
        settingsCard);
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    settingsCard->body()->addWidget(hint);

    m_example = new QLabel(settingsCard);
    m_example->setObjectName(QStringLiteral("detailLabel"));
    m_example->setWordWrap(true);
    settingsCard->body()->addWidget(m_example);

    auto* saveRow = new QHBoxLayout();
    m_feedback = new QLabel(settingsCard);
    m_feedback->setWordWrap(true);
    m_feedback->setObjectName(QStringLiteral("muted"));
    m_saveBtn = new QPushButton(QStringLiteral("保存设置"), settingsCard);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    saveRow->addWidget(m_feedback, 1);
    saveRow->addWidget(m_saveBtn);
    settingsCard->body()->addLayout(saveRow);
    settingsCard->body()->addStretch(1);

    root->addWidget(settingsCard, 1);

    // ---- 数据与存储 ----
    auto* storageCard = new Card(QStringLiteral("数据与存储"), this);

    m_dir = new QLineEdit(storageCard);
    m_dir->setReadOnly(true);
    auto* openBtn = new QPushButton(QStringLiteral("打开目录"), storageCard);
    openBtn->setObjectName(QStringLiteral("ghost"));
    openBtn->setCursor(Qt::PointingHandCursor);
    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(m_dir, 1);
    dirRow->addWidget(openBtn);
    storageCard->body()->addLayout(dirRow);

    auto* statForm = new QFormLayout();
    statForm->setSpacing(8);
    statForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statSeq = makeValue(storageCard);
    m_statSnapshot = makeValue(storageCard);
    m_statGens = makeValue(storageCard);
    m_statJournal = makeValue(storageCard);
    m_statSize = makeValue(storageCard);
    m_statHistory = makeValue(storageCard);
    statForm->addRow(QStringLiteral("当前日志代号"), m_statSeq);
    statForm->addRow(QStringLiteral("当前快照代号"), m_statSnapshot);
    statForm->addRow(QStringLiteral("快照代数"), m_statGens);
    statForm->addRow(QStringLiteral("待压缩 journal"), m_statJournal);
    statForm->addRow(QStringLiteral("数据目录大小"), m_statSize);
    statForm->addRow(QStringLiteral("历史流水笔数"), m_statHistory);
    storageCard->body()->addLayout(statForm);

    auto* buttonRow = new QHBoxLayout();
    auto* compactBtn = new QPushButton(QStringLiteral("立即压缩快照"), storageCard);
    compactBtn->setObjectName(QStringLiteral("ghost"));
    compactBtn->setCursor(Qt::PointingHandCursor);
    auto* exportBtn = new QPushButton(QStringLiteral("导出流水 CSV…"), storageCard);
    exportBtn->setObjectName(QStringLiteral("ghost"));
    exportBtn->setCursor(Qt::PointingHandCursor);
    buttonRow->addWidget(compactBtn);
    buttonRow->addWidget(exportBtn);
    buttonRow->addStretch(1);
    storageCard->body()->addLayout(buttonRow);

    auto* storageHint = new QLabel(
        QStringLiteral("每次操作只追加一行 journal；压缩时写出整代快照并原子切换 CURRENT。"),
        storageCard);
    storageHint->setObjectName(QStringLiteral("muted"));
    storageHint->setWordWrap(true);
    storageCard->body()->addWidget(storageHint);
    storageCard->body()->addStretch(1);

    root->addWidget(storageCard, 1);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsPage::save);
    connect(compactBtn, &QPushButton::clicked, this, &SettingsPage::compactNow);
    connect(exportBtn, &QPushButton::clicked, this, &SettingsPage::exportCsv);
    connect(openBtn, &QPushButton::clicked, this, &SettingsPage::openDataDir);

    refresh();
}

void SettingsPage::refresh()
{
    if (!m_ctx)
        return;

    const pl::Config cfg = m_ctx->config();
    m_capacity->setValue(cfg.capacity);
    m_price->setValue(cfg.unitPriceCents / 100.0);
    m_free->setValue(cfg.freeMinutes);
    m_unit->setValue(cfg.billingUnitMinutes);
    m_maxWaiting->setValue(cfg.maxWaiting);

    const qint64 exampleFee = pl::feeCentsFor(0, 2 * 3'600'000, cfg);
    m_example->setText(
        QStringLiteral("计费示例：停车 2 小时应缴 %1（免费 %2 分钟，此后每 %3 分钟 %4）。")
            .arg(pl::formatMoney(exampleFee))
            .arg(cfg.freeMinutes)
            .arg(cfg.billingUnitMinutes)
            .arg(pl::formatMoney(cfg.unitPriceCents)));

    m_dir->setText(QDir::toNativeSeparators(m_ctx->dataDir()));

    const bool readOnly = m_ctx->isReadOnly();
    m_readOnly->setVisible(readOnly);
    m_saveBtn->setEnabled(!readOnly);
    m_capacity->setEnabled(!readOnly);
    m_price->setEnabled(!readOnly);
    m_free->setEnabled(!readOnly);
    m_unit->setEnabled(!readOnly);
    m_maxWaiting->setEnabled(!readOnly);

    updateStorageLabels();
}

void SettingsPage::updateStorageLabels()
{
    const pl::LotStore::Stats stats = m_ctx->store().stats();
    m_statSeq->setText(QString::number(stats.seq));
    m_statSnapshot->setText(QString::number(stats.snapshotSeq));
    m_statGens->setText(QStringLiteral("%1 代（保留最近 2 代）").arg(stats.generations));
    m_statJournal->setText(QStringLiteral("%1 条 / %2")
                               .arg(stats.journalRecords)
                               .arg(formatBytes(stats.journalBytes)));
    m_statSize->setText(formatBytes(stats.totalBytes));
    m_statHistory->setText(QStringLiteral("%1 笔").arg(stats.historyRecords));
}

void SettingsPage::save()
{
    pl::Config cfg = m_ctx->config();
    cfg.capacity = m_capacity->value();
    cfg.unitPriceCents = qRound(m_price->value() * 100.0);
    cfg.freeMinutes = m_free->value();
    cfg.billingUnitMinutes = m_unit->value();
    cfg.maxWaiting = m_maxWaiting->value();

    QString error;
    if (!m_ctx->applyConfig(cfg, &error)) {
        m_feedback->setText(QStringLiteral("保存失败：%1").arg(error));
        m_feedback->setStyleSheet(QStringLiteral("color:#B45309;"));
        return;
    }

    m_feedback->setText(QStringLiteral("设置已保存。"));
    m_feedback->setStyleSheet(QStringLiteral("color:#047857;"));
    updateStorageLabels();
}

void SettingsPage::compactNow()
{
    QString error;
    if (!m_ctx->compactNow(&error)) {
        m_feedback->setText(QStringLiteral("压缩失败：%1").arg(error));
        m_feedback->setStyleSheet(QStringLiteral("color:#B45309;"));
        return;
    }
    m_feedback->setText(QStringLiteral("已写入新一代快照，journal 已清空。"));
    m_feedback->setStyleSheet(QStringLiteral("color:#047857;"));
    updateStorageLabels();
}

void SettingsPage::exportCsv()
{
    const QString defaultPath = QDir(m_ctx->dataDir()).filePath(QStringLiteral("出车流水.csv"));
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
                             QStringLiteral("流水已导出到：\n%1").arg(path));
}

void SettingsPage::openDataDir()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_ctx->dataDir()));
}

} // namespace ui
