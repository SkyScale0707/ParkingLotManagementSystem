#include "ui/dialogs/exitdialog.h"

#include "app/appcontext.h"
#include "domain/billing.h"
#include "ui/theme.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui {

ExitDialog::ExitDialog(app::AppContext* ctx, QWidget* parent)
    : QDialog(parent)
    , m_ctx(ctx)
{
    setWindowTitle(QStringLiteral("出场结算"));
    setModal(true);
    setMinimumWidth(460);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("出场结算"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* subtitle = new QLabel(
        QStringLiteral("查询应缴金额后确认放行；出场后队首车辆会自动补位。"), this);
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);

    root->addWidget(title);
    root->addWidget(subtitle);

    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(8);
    m_plate = new QLineEdit(this);
    m_plate->setPlaceholderText(QStringLiteral("输入车牌号，例如：鄂A12345"));
    m_plate->setMaxLength(12);
    m_lookupBtn = new QPushButton(QStringLiteral("查询"), this);
    searchRow->addWidget(m_plate, 1);
    searchRow->addWidget(m_lookupBtn);
    root->addLayout(searchRow);

    auto* detailCard = new QFrame(this);
    detailCard->setObjectName(QStringLiteral("card"));
    detailCard->setAttribute(Qt::WA_StyledBackground, true);
    auto* form = new QFormLayout(detailCard);
    form->setContentsMargins(16, 14, 16, 14);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makeValue = [detailCard]() {
        auto* label = new QLabel(QStringLiteral("—"), detailCard);
        label->setObjectName(QStringLiteral("detailLabel"));
        return label;
    };
    m_plateVal = makeValue();
    m_typeVal = makeValue();
    m_slotVal = makeValue();
    m_enterVal = makeValue();
    m_stayVal = makeValue();
    m_feeVal = makeValue();
    m_feeVal->setStyleSheet(QStringLiteral("color:#2563EB; font-size:20px; font-weight:700;"));

    form->addRow(QStringLiteral("车牌号"), m_plateVal);
    form->addRow(QStringLiteral("车型"), m_typeVal);
    form->addRow(QStringLiteral("车位"), m_slotVal);
    form->addRow(QStringLiteral("入场时间"), m_enterVal);
    form->addRow(QStringLiteral("停车时长"), m_stayVal);
    form->addRow(QStringLiteral("应缴金额"), m_feeVal);
    root->addWidget(detailCard);

    m_feedback = new QLabel(this);
    m_feedback->setWordWrap(true);
    m_feedback->setMinimumHeight(38);
    m_feedback->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    root->addWidget(m_feedback);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setObjectName(QStringLiteral("ghost"));
    m_settleBtn = new QPushButton(QStringLiteral("确认结算"), this);
    m_settleBtn->setEnabled(false);
    buttons->addWidget(closeBtn);
    buttons->addWidget(m_settleBtn);
    root->addLayout(buttons);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_lookupBtn, &QPushButton::clicked, this, &ExitDialog::lookup);
    connect(m_settleBtn, &QPushButton::clicked, this, &ExitDialog::settle);
    connect(m_plate, &QLineEdit::returnPressed, this, &ExitDialog::lookup);
    connect(m_plate, &QLineEdit::textChanged, this, [this]() {
        m_settleBtn->setEnabled(false);
        m_found = false;
    });

    if (m_ctx->isReadOnly()) {
        m_settleBtn->setEnabled(false);
        setFeedback(QStringLiteral("当前为只读模式（另一实例正在使用数据目录），无法结算。"), true);
    }

    m_plate->setFocus();
}

void ExitDialog::setPlateAndLookup(const QString& plate)
{
    m_plate->setText(plate.trimmed().toUpper());
    lookup();
}

void ExitDialog::setFeedback(const QString& text, bool warn)
{
    m_feedback->setText(text);
    m_feedback->setStyleSheet(warn ? QStringLiteral("color:#B45309;") : QStringLiteral("color:#047857;"));
}

void ExitDialog::resetDetails()
{
    for (QLabel* label : {m_plateVal, m_typeVal, m_slotVal, m_enterVal, m_stayVal})
        label->setText(QStringLiteral("—"));
    m_feeVal->setText(QStringLiteral("¥0.00"));
}

void ExitDialog::lookup()
{
    const QString plate = m_plate->text().trimmed().toUpper();
    if (plate.isEmpty()) {
        setFeedback(QStringLiteral("请输入车牌号。"), true);
        return;
    }

    const pl::Lot& lot = m_ctx->lot();
    const auto it = lot.active.constFind(plate);
    if (it == lot.active.constEnd()) {
        resetDetails();
        m_found = false;
        m_settleBtn->setEnabled(false);
        const int pos = lot.waitingIndex(plate);
        if (pos >= 0)
            setFeedback(QStringLiteral("该车在等待队列第 %1 位，尚未入位，无法结算。").arg(pos + 1), true);
        else
            setFeedback(QStringLiteral("未找到该车辆，请核对车牌号。"), true);
        return;
    }

    const pl::ActiveEntry& e = it.value();
    const qint64 now = pl::nowMs();
    const qint64 stay = qMax<qint64>(0, now - e.enteredAtMs);
    const qint64 fee = pl::feeCentsFor(e.enteredAtMs, now, lot.cfg);

    m_plateVal->setText(e.plate);
    m_typeVal->setText(pl::typeLabel(e.type));
    m_slotVal->setText(lot.slotLabel(e.slot));
    m_enterVal->setText(pl::formatDateTime(e.enteredAtMs));
    m_stayVal->setText(pl::formatDuration(stay));
    m_feeVal->setText(pl::formatMoney(fee));

    m_found = true;
    m_settleBtn->setEnabled(!m_ctx->isReadOnly());
    setFeedback(QStringLiteral("已找到车辆，请确认金额后结算。"), false);
}

void ExitDialog::settle()
{
    if (!m_found)
        return;

    const QString plate = m_plate->text().trimmed().toUpper();
    QString error;
    const pl::ExitResult r = m_ctx->leaveVehicle(plate, &error);
    if (!error.isEmpty()) {
        setFeedback(QStringLiteral("结算失败：%1").arg(error), true);
        return;
    }

    if (r.status == pl::ExitStatus::StillWaiting) {
        setFeedback(QStringLiteral("该车仍在等待队列，无法结算。"), true);
        return;
    }
    if (r.status != pl::ExitStatus::Ok) {
        setFeedback(QStringLiteral("未找到该车辆，可能已被结算。"), true);
        return;
    }

    QString message = QStringLiteral("结算完成：%1 · %2 · 停车 %3 · 实收 %4")
                          .arg(r.plate, pl::typeLabel(r.type), pl::formatDuration(r.stayMs),
                               pl::formatMoney(r.feeCents));
    if (r.promoted) {
        message += QStringLiteral("\n队首 %1 已自动补位到 %2。")
                       .arg(r.promotedPlate, m_ctx->lot().slotLabel(r.slot));
    }
    setFeedback(message, false);

    m_settleBtn->setEnabled(false);
    m_found = false;
    resetDetails();
    m_plate->clear();
    m_plate->setFocus();
}

} // namespace ui
