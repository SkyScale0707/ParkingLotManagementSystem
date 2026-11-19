#pragma once
// ---------------------------------------------------------------------------
// theme.h — 配色、全局样式与 KPI 卡片
// ---------------------------------------------------------------------------
#include "domain/types.h"

#include <QColor>
#include <QFrame>
#include <QString>

class QLabel;
class QVBoxLayout;

namespace ui {

namespace theme {

QColor accent();
QColor textPrimary();
QColor textMuted();
QColor surface();
QColor pageBackground();
QColor border();

QColor typeColor(pl::VehicleType t);       // 实色（边框/文字）
QColor typeSoftColor(pl::VehicleType t);   // 浅色（车位填充）

QString styleSheet();

} // namespace theme

// 总览页顶部指标卡：标题 / 大号数值 / 副标题
class KpiCard : public QFrame
{
    Q_OBJECT

public:
    explicit KpiCard(const QString& title, QWidget* parent = nullptr);

    void setValue(const QString& value);
    void setHint(const QString& hint);
    void setAccent(const QColor& color);

private:
    QLabel* m_value = nullptr;
    QLabel* m_hint = nullptr;
};

// 带标题的白色卡片容器
class Card : public QFrame
{
    Q_OBJECT

public:
    explicit Card(const QString& title = QString(), QWidget* parent = nullptr);

    QVBoxLayout* body() const { return m_body; }

private:
    QVBoxLayout* m_body = nullptr;
};

} // namespace ui
