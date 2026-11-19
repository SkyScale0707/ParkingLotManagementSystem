#include "ui/theme.h"

#include <QLabel>
#include <QVBoxLayout>

namespace ui {
namespace theme {

QColor accent() { return QColor(0x25, 0x63, 0xEB); }
QColor textPrimary() { return QColor(0x0F, 0x17, 0x2A); }
QColor textMuted() { return QColor(0x64, 0x74, 0x8B); }
QColor surface() { return QColor(0xFF, 0xFF, 0xFF); }
QColor pageBackground() { return QColor(0xF4, 0xF6, 0xFA); }
QColor border() { return QColor(0xE6, 0xEA, 0xF2); }

QColor typeColor(pl::VehicleType t)
{
    switch (t) {
    case pl::VehicleType::CarSmall:    return QColor(0x25, 0x63, 0xEB);  // 蓝
    case pl::VehicleType::TruckSmall:  return QColor(0x10, 0xB9, 0x81);  // 绿
    case pl::VehicleType::TruckMedium: return QColor(0xF5, 0x9E, 0x0B);  // 琥珀
    case pl::VehicleType::TruckLarge:  return QColor(0x8B, 0x5C, 0xF6);  // 紫
    }
    return accent();
}

QColor typeSoftColor(pl::VehicleType t)
{
    switch (t) {
    case pl::VehicleType::CarSmall:    return QColor(0xDB, 0xEA, 0xFE);
    case pl::VehicleType::TruckSmall:  return QColor(0xD1, 0xFA, 0xE5);
    case pl::VehicleType::TruckMedium: return QColor(0xFE, 0xF3, 0xC7);
    case pl::VehicleType::TruckLarge:  return QColor(0xED, 0xE9, 0xFE);
    }
    return QColor(0xE2, 0xE8, 0xF0);
}

QString styleSheet()
{
    return QStringLiteral(R"(
QMainWindow, QWidget#pageRoot, QWidget#pageBody { background: #F4F6FA; }

QFrame#sidebar { background: #0F172A; border: none; }
QLabel#brandTitle { color: #FFFFFF; font-size: 17px; font-weight: 700; }
QLabel#brandSub { color: #94A3B8; font-size: 11px; }
QLabel#sidebarFoot { color: #64748B; font-size: 10px; }

QListWidget#nav { background: transparent; border: none; outline: none; color: #CBD5E1; }
QListWidget#nav::item { padding: 11px 14px; margin: 2px 10px; border-radius: 8px; }
QListWidget#nav::item:hover { background: #1E293B; color: #FFFFFF; }
QListWidget#nav::item:selected { background: #2563EB; color: #FFFFFF; }

QLabel#pageTitle { font-size: 22px; font-weight: 700; color: #0F172A; }
QLabel#pageSubtitle { color: #64748B; font-size: 12px; }

QFrame#card, QFrame#kpiCard {
    background: #FFFFFF;
    border: 1px solid #E6EAF2;
    border-radius: 12px;
}
QLabel#cardTitle { font-size: 13px; font-weight: 600; color: #0F172A; }
QLabel#kpiTitle { color: #64748B; font-size: 12px; }
QLabel#kpiValue { font-size: 26px; font-weight: 700; color: #0F172A; }
QLabel#kpiHint { color: #94A3B8; font-size: 11px; }
QLabel#muted { color: #64748B; }
QLabel#detailLabel { color: #334155; font-size: 12px; }
QLabel#warnLabel { color: #B45309; font-size: 12px; }
QLabel#okLabel { color: #047857; font-size: 12px; }

QPushButton {
    background: #2563EB; color: #FFFFFF; border: none; border-radius: 8px;
    padding: 8px 16px; font-weight: 600;
}
QPushButton:hover { background: #1D4ED8; }
QPushButton:pressed { background: #1E40AF; }
QPushButton:disabled { background: #E2E8F0; color: #94A3B8; }
QPushButton#ghost { background: #EEF2F7; color: #0F172A; }
QPushButton#ghost:hover { background: #E2E8F0; }
QPushButton#link { background: transparent; color: #2563EB; padding: 4px 6px; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 8px;
    padding: 6px 10px; min-height: 20px;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border: 1px solid #2563EB;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #FFFFFF; border: 1px solid #E6EAF2; selection-background-color: #DBEAFE;
    selection-color: #0F172A;
}

QTableView {
    background: #FFFFFF; border: 1px solid #E6EAF2; border-radius: 10px;
    gridline-color: #F1F5F9; selection-background-color: #DBEAFE; selection-color: #0F172A;
    alternate-background-color: #FAFBFD;
}
QTableView::item { padding: 6px 8px; border: none; }
QHeaderView::section {
    background: #F8FAFC; color: #64748B; border: none; border-bottom: 1px solid #E6EAF2;
    padding: 8px; font-weight: 600;
}
QTableCornerButton::section { background: #F8FAFC; border: none; }

QProgressBar {
    background: #EEF2F7; border: none; border-radius: 4px; max-height: 8px;
}
QProgressBar::chunk { background: #2563EB; border-radius: 4px; }

QStatusBar { background: #FFFFFF; border-top: 1px solid #E6EAF2; }
QStatusBar QLabel { color: #64748B; padding: 0 8px; }

QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }

QToolTip { background: #0F172A; color: #FFFFFF; border: none; padding: 6px 8px; }

QGroupBox { border: 1px solid #E6EAF2; border-radius: 10px; margin-top: 10px; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #64748B; }
)");
}

} // namespace theme

KpiCard::KpiCard(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("kpiCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumHeight(96);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("kpiTitle"));
    m_value = new QLabel(QStringLiteral("—"), this);
    m_value->setObjectName(QStringLiteral("kpiValue"));
    m_hint = new QLabel(QString(), this);
    m_hint->setObjectName(QStringLiteral("kpiHint"));

    layout->addWidget(titleLabel);
    layout->addWidget(m_value);
    layout->addWidget(m_hint);
    layout->addStretch(1);
}

void KpiCard::setValue(const QString& value)
{
    m_value->setText(value);
}

void KpiCard::setHint(const QString& hint)
{
    m_hint->setText(hint);
}

void KpiCard::setAccent(const QColor& color)
{
    m_value->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
}

Card::Card(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 14, 16, 14);
    outer->setSpacing(10);

    if (!title.isEmpty()) {
        auto* titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("cardTitle"));
        outer->addWidget(titleLabel);
    }

    m_body = new QVBoxLayout();
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(10);
    outer->addLayout(m_body, 1);
}

} // namespace ui
