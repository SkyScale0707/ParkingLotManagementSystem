#include "ui/dialogs/enterdialog.h"

#include "app/appcontext.h"
#include "ui/theme.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui {

EnterDialog::EnterDialog(app::AppContext* ctx, QWidget* parent)
    : QDialog(parent)
    , m_ctx(ctx)
{
    setWindowTitle(QStringLiteral("车辆入库"));
    setModal(true);
    setMinimumWidth(430);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("车辆入库"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* subtitle = new QLabel(
        QStringLiteral("车牌号需唯一；车位已满时自动进入 FIFO 等待队列。"), this);
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);

    root->addWidget(title);
    root->addWidget(subtitle);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_plate = new QLineEdit(this);
    m_plate->setPlaceholderText(QStringLiteral("例如：鄂A12345"));
    m_plate->setMaxLength(12);
    m_plate->setClearButtonEnabled(true);

    m_type = new QComboBox(this);
    for (pl::VehicleType t : pl::allTypes())
        m_type->addItem(pl::typeLabel(t), static_cast<int>(t));
    m_type->setCurrentIndex(0);

    form->addRow(QStringLiteral("车牌号"), m_plate);
    form->addRow(QStringLiteral("车型"), m_type);
    root->addLayout(form);

    m_feedback = new QLabel(this);
    m_feedback->setWordWrap(true);
    m_feedback->setMinimumHeight(42);
    m_feedback->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    root->addWidget(m_feedback);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setObjectName(QStringLiteral("ghost"));
    m_submit = new QPushButton(QStringLiteral("确认入库"), this);
    m_submit->setDefault(true);
    buttons->addWidget(closeBtn);
    buttons->addWidget(m_submit);
    root->addLayout(buttons);

    if (m_ctx->isReadOnly()) {
        m_submit->setEnabled(false);
        setFeedback(QStringLiteral("当前为只读模式（另一实例正在使用数据目录），无法入库。"), true);
    }

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_submit, &QPushButton::clicked, this, &EnterDialog::submit);
    connect(m_plate, &QLineEdit::returnPressed, this, &EnterDialog::submit);

    m_plate->setFocus();
}

void EnterDialog::setFeedback(const QString& text, bool warn)
{
    m_feedback->setText(text);
    m_feedback->setStyleSheet(warn ? QStringLiteral("color:#B45309;") : QStringLiteral("color:#047857;"));
}

void EnterDialog::submit()
{
    const QString plate = m_plate->text().trimmed().toUpper();
    if (plate.isEmpty()) {
        setFeedback(QStringLiteral("请输入车牌号。"), true);
        m_plate->setFocus();
        return;
    }
    if (plate.size() < 2 || plate.size() > 12) {
        setFeedback(QStringLiteral("车牌号长度不合法（2~12 个字符）。"), true);
        m_plate->setFocus();
        return;
    }

    const auto type = static_cast<pl::VehicleType>(m_type->currentData().toInt());
    QString error;
    const pl::EnterResult r = m_ctx->enterVehicle(plate, type, &error);
    if (!error.isEmpty()) {
        setFeedback(QStringLiteral("写入失败：%1").arg(error), true);
        return;
    }

    switch (r.status) {
    case pl::EnterStatus::Parked:
        setFeedback(QStringLiteral("已入位 %1，凭证 %2。")
                        .arg(m_ctx->lot().slotLabel(r.slot), r.ticket),
                    false);
        m_plate->clear();
        m_plate->setFocus();
        break;
    case pl::EnterStatus::Queued: {
        const int pos = m_ctx->lot().waitingIndex(plate);
        setFeedback(QStringLiteral("车位已满，已进入等待队列第 %1 位（凭证 %2）。")
                        .arg(pos + 1)
                        .arg(r.ticket),
                    true);
        m_plate->clear();
        m_plate->setFocus();
        break;
    }
    case pl::EnterStatus::DuplicateActive:
        setFeedback(QStringLiteral("该车牌已在场内，不能重复入场。"), true);
        m_plate->selectAll();
        break;
    case pl::EnterStatus::DuplicateWaiting:
        setFeedback(QStringLiteral("该车牌已在等待队列中。"), true);
        m_plate->selectAll();
        break;
    case pl::EnterStatus::QueueFull:
        setFeedback(QStringLiteral("等待队列已满，请稍后再试。"), true);
        break;
    case pl::EnterStatus::InvalidPlate:
    default:
        setFeedback(QStringLiteral("车牌号无效。"), true);
        break;
    }
}

} // namespace ui
